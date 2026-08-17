#include "maps.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "terrain_data.h"

namespace
{
const std::string kTileLibraryRoot =
    "/Users/ashu/Stochesia/map_tile_library";
const std::string kFe8MapRoot =
    kTileLibraryRoot + "/reference_gameplay_maps/";
const std::string kEditorMapRoot =
    kTileLibraryRoot + "/reference_layouts";
const int kTerrainLookupOffset = 0x2000;

struct VisualAddress
{
    int tile_class = 0;
    int tile_code = 0;
};

std::vector<unsigned char> read_binary(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Could not open FE8 map asset: " + path);
    }

    return std::vector<unsigned char>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

std::string read_text(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Could not open map metadata: " + path);
    }
    return {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };
}

std::vector<std::string> split(const std::string& line, char separator)
{
    std::vector<std::string> result;
    std::stringstream stream(line);
    std::string part;
    while (std::getline(stream, part, separator))
    {
        result.push_back(part);
    }
    return result;
}

int read_json_number(const std::string& json, const std::string& key)
{
    std::size_t start = json.find("\"" + key + "\"");
    if (start == std::string::npos)
    {
        throw std::runtime_error("Map metadata is missing " + key);
    }

    start = json.find(':', start);
    if (start == std::string::npos)
    {
        throw std::runtime_error("Map metadata has an invalid " + key);
    }
    return std::stoi(json.substr(start + 1));
}

void require_rectangular(const maps::IntGrid& grid, const char* name)
{
    if (grid.empty() || grid.front().empty())
    {
        throw std::invalid_argument(std::string(name) + " must not be empty.");
    }
    const std::size_t width = grid.front().size();
    for (const std::vector<int>& row : grid)
    {
        if (row.size() != width)
        {
            throw std::invalid_argument(std::string(name) + " must be rectangular.");
        }
    }
}

void require_same_dimensions(const maps::IntGrid& left,
                             const maps::IntGrid& right,
                             const char* left_name,
                             const char* right_name)
{
    require_rectangular(left, left_name);
    require_rectangular(right, right_name);
    if (left.size() != right.size() || left.front().size() != right.front().size())
    {
        throw std::invalid_argument(
            std::string(left_name) + " and " + right_name + " have different dimensions."
        );
    }
}

maps::TerrainMap load_terrain(const std::string& layout_name, const std::string& config_name)
{
    const std::string layout_dir = kFe8MapRoot + "layout/";
    const std::string metadata = read_text(layout_dir + layout_name + ".json");
    const int width = read_json_number(metadata, "width");
    const int height = read_json_number(metadata, "height");

    const std::vector<unsigned char> layout = read_binary(layout_dir + layout_name + ".mar");
    const std::vector<unsigned char> config = read_binary(kFe8MapRoot + config_name);
    if (layout.size() != static_cast<std::size_t>(width * height * 2))
    {
        throw std::runtime_error("Unexpected layout size for " + layout_name);
    }

    maps::TerrainMap terrain(height, std::vector<int>(width));
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const std::size_t layout_index = static_cast<std::size_t>((y * width + x) * 2);
            std::uint16_t base_tile = static_cast<std::uint16_t>(layout[layout_index]) |
                (static_cast<std::uint16_t>(layout[layout_index + 1]) << 8);

            // FEBuilder editor map cells encode a base tile in their upper
            // bits. This is the same conversion as FE8's mar_to_map.py.
            base_tile >>= 3;
            const std::size_t terrain_index =
                static_cast<std::size_t>(kTerrainLookupOffset + (base_tile >> 2));
            if (terrain_index >= config.size())
            {
                throw std::runtime_error("Invalid tile ID in " + layout_name);
            }
            terrain[y][x] = config[terrain_index];
        }
    }
    return terrain;
}

int theme_id_for(const std::string& source)
{
    std::ifstream file(kTileLibraryRoot + "/data/themes.tsv");
    if (!file)
    {
        throw std::runtime_error("Cannot open map_tile_library/data/themes.tsv.");
    }

    std::string line;
    std::getline(file, line); // header
    while (std::getline(file, line))
    {
        const std::vector<std::string> fields = split(line, '\t');
        if (fields.size() >= 2 && fields[1] == source)
        {
            return std::stoi(fields[0]);
        }
    }
    throw std::runtime_error("No visual theme exists for " + source);
}

std::unordered_map<std::string, VisualAddress> catalogue_for_theme(int theme_id)
{
    std::ifstream file(kTileLibraryRoot + "/data/catalogue.tsv");
    if (!file)
    {
        throw std::runtime_error("Cannot open map_tile_library/data/catalogue.tsv.");
    }

    std::unordered_map<std::string, VisualAddress> result;
    std::string line;
    std::getline(file, line); // header
    while (std::getline(file, line))
    {
        const std::vector<std::string> fields = split(line, '\t');
        if (fields.size() < 8 || std::stoi(fields[0]) != theme_id)
        {
            continue;
        }
        const int tile_class = std::stoi(fields[2]);
        const int subclass = std::stoi(fields[4]);
        const int orientation = std::stoi(fields[5]);
        result.emplace(fields[6], VisualAddress{
            tile_class, maps::make_visual_tile_code(subclass, orientation)
        });
    }
    if (result.empty())
    {
        throw std::runtime_error("Visual theme has no catalogue entries.");
    }
    return result;
}

std::vector<std::vector<std::string>> read_editor_hash_grid(const std::string& path)
{
    const std::string json = read_text(path);
    std::vector<std::vector<std::string>> rows;
    int depth = 0;

    for (std::size_t index = 0; index < json.size(); ++index)
    {
        if (json[index] == '[')
        {
            ++depth;
            if (depth == 2)
            {
                rows.emplace_back();
            }
            continue;
        }
        if (json[index] == ']')
        {
            --depth;
            continue;
        }
        if (json[index] != '"')
        {
            continue;
        }

        const std::size_t end_quote = json.find('"', index + 1);
        if (end_quote == std::string::npos)
        {
            throw std::runtime_error("Unterminated tile hash in " + path);
        }
        if (depth == 2)
        {
            rows.back().push_back(json.substr(index + 1, end_quote - index - 1));
        }
        index = end_quote;
    }

    if (rows.empty() || rows.front().empty())
    {
        throw std::runtime_error("Editor map JSON is empty: " + path);
    }
    const std::size_t width = rows.front().size();
    for (const std::vector<std::string>& row : rows)
    {
        if (row.size() != width)
        {
            throw std::runtime_error("Editor map JSON is not rectangular: " + path);
        }
    }
    return rows;
}

maps::MapRecipe load_visual_recipe(const std::string& layout_name,
                                   const std::string& config_name,
                                   const std::string& theme_source)
{
    maps::MapRecipe result;
    result.terrain = load_terrain(layout_name, config_name);
    result.theme_id = theme_id_for(theme_source);

    constexpr const char* kFe8Prefix = "Fire Emblem 8/";
    if (theme_source.rfind(kFe8Prefix, 0) != 0)
    {
        throw std::runtime_error("Unexpected FE8 theme path: " + theme_source);
    }
    const std::string editor_map = kEditorMapRoot + "/" +
        theme_source + "/001.png.json";
    const auto hashes = read_editor_hash_grid(editor_map);
    const auto catalogue = catalogue_for_theme(result.theme_id);

    result.classes.resize(hashes.size());
    result.tiles.resize(hashes.size());
    for (std::size_t y = 0; y < hashes.size(); ++y)
    {
        result.classes[y].reserve(hashes[y].size());
        result.tiles[y].reserve(hashes[y].size());
        for (const std::string& hash : hashes[y])
        {
            const auto address = catalogue.find(hash);
            if (address == catalogue.end())
            {
                throw std::runtime_error(
                    "Tile hash " + hash + " is absent from visual theme " + theme_source
                );
            }
            result.classes[y].push_back(address->second.tile_class);
            result.tiles[y].push_back(address->second.tile_code);
        }
    }

    require_same_dimensions(result.terrain, result.classes, "Gameplay terrain", "Visual class grid");
    require_same_dimensions(result.terrain, result.tiles, "Gameplay terrain", "Visual tile grid");
    return result;
}

int terrain_for_visual_class(int tile_class)
{
    // These values match map_tile_library's stable TileClass integers. This
    // core maps module deliberately depends only on integers, not Cocoa or
    // map_tile_library headers, so pure gameplay builds remain lightweight.
    switch (tile_class)
    {
        case 0:  return TERRAIN_NONE;                 // EMPTY
        case 2:  return TERRAIN_FOREST;
        case 3:  return TERRAIN_MOUNTAIN;
        case 4:  return TERRAIN_PEAK;
        case 5:  return TERRAIN_VALLEY;
        case 6:
        case 51: return TERRAIN_CLIFF;
        case 7:
        case 8:  return TERRAIN_ROAD;
        case 9:  return TERRAIN_DESERT;
        case 10: return TERRAIN_SAND;
        case 11: return TERRAIN_RIVER;
        case 12: return TERRAIN_WATER;
        case 13: return TERRAIN_LAKE;
        case 14: return TERRAIN_SEA;
        case 15: return TERRAIN_BRIDGE_REGULAR;
        case 16: return TERRAIN_FORT;
        case 17: return TERRAIN_VILLAGE_REGULAR;
        case 18:
        case 52: return TERRAIN_HOUSE;
        case 19: return TERRAIN_ARMORY;
        case 20: return TERRAIN_VENDOR;
        case 21: return TERRAIN_ARENA_REGULAR;
        case 22: return TERRAIN_INN;
        case 23:
        case 24: return TERRAIN_WALL_REGULAR;
        case 25: return TERRAIN_FLOOR_REGULAR;
        case 26: return TERRAIN_STAIRS;
        case 27: return TERRAIN_DOOR;
        case 28: return TERRAIN_GATE_REGULAR;
        case 29:
        case 30:
        case 31: return TERRAIN_FENCE_REGULAR;
        case 32:
        case 33: return TERRAIN_BRACE;
        case 34: return TERRAIN_PILLAR;
        case 35: return TERRAIN_THRONE;
        case 36: return TERRAIN_CHEST_FULL;
        case 37: return TERRAIN_ROOF;
        case 38: return TERRAIN_RUINS_REGULAR;
        case 39: return TERRAIN_THICKET;
        case 40: return TERRAIN_SNAG;
        case 41: return TERRAIN_BARREL;
        case 42: return TERRAIN_BONE;
        case 43: return TERRAIN_DARK;
        case 44: return TERRAIN_DECK;
        case 45: return TERRAIN_GUNNELS;
        case 46: return TERRAIN_MAST;
        case 47: return TERRAIN_BRACE;
        case 1:  // PLAIN
        case 48: // FLAT
        case 49: // DASHDASH
        case 50: // UNDEFINED
        default: return TERRAIN_PLAINS;
    }
}
}

namespace maps
{
bool MapRecipe::has_visuals() const
{
    return theme_id > 0 && !classes.empty() && !tiles.empty();
}

int MapRecipe::rows() const
{
    return static_cast<int>(terrain.size());
}

int MapRecipe::columns() const
{
    return terrain.empty() ? 0 : static_cast<int>(terrain.front().size());
}

MapRecipe compose(int theme_id,
                  TerrainMap terrain,
                  VisualClassMap classes,
                  VisualTileMap tiles)
{
    if (theme_id <= 0)
    {
        throw std::invalid_argument("A visual map recipe needs a positive theme ID.");
    }
    require_same_dimensions(terrain, classes, "Gameplay terrain", "Visual class grid");
    require_same_dimensions(terrain, tiles, "Gameplay terrain", "Visual tile grid");
    return {std::move(terrain), theme_id, std::move(classes), std::move(tiles)};
}

MapRecipe from_visual(int theme_id, VisualClassMap classes, VisualTileMap tiles)
{
    require_same_dimensions(classes, tiles, "Visual class grid", "Visual tile grid");
    TerrainMap terrain = classes;
    for (std::vector<int>& row : terrain)
    {
        for (int& tile_class : row)
        {
            tile_class = terrain_for_visual_class(tile_class);
        }
    }
    return compose(theme_id, std::move(terrain), std::move(classes), std::move(tiles));
}

MapRecipe gameplay_only(TerrainMap terrain)
{
    require_rectangular(terrain, "Gameplay terrain");
    return {std::move(terrain), 0, {}, {}};
}

MapRecipe prologue()             { return load_visual_recipe("PrologueMap", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/Prologue"); }
MapRecipe chapter_1()            { return load_visual_recipe("Ch1Map", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/01"); }
MapRecipe chapter_2()            { return load_visual_recipe("Ch2Map", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/02"); }
MapRecipe chapter_3()            { return load_visual_recipe("Ch3Map", "TileConfiguration2.bin", "Fire Emblem 8/Chapters/03"); }
MapRecipe chapter_4()            { return load_visual_recipe("Ch4Map", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/04"); }
MapRecipe chapter_5x()           { return load_visual_recipe("Ch5XMap", "TileConfiguration3.bin", "Fire Emblem 8/Chapters/05x"); }
MapRecipe chapter_5()            { return load_visual_recipe("Ch5Map", "TileConfiguration2.bin", "Fire Emblem 8/Chapters/05"); }
MapRecipe chapter_6()            { return load_visual_recipe("Ch6Map", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/06"); }
MapRecipe chapter_7()            { return load_visual_recipe("Ch7Map", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/07"); }
MapRecipe chapter_8()            { return load_visual_recipe("Ch8Map", "TileConfiguration3.bin", "Fire Emblem 8/Chapters/08"); }

MapRecipe chapter_9_eirika()     { return load_visual_recipe("Ch9EirikaMap", "TileConfiguration2.bin", "Fire Emblem 8/Chapters/09Eirika"); }
MapRecipe chapter_10_eirika()    { return load_visual_recipe("Ch10EirikaMap", "TileConfiguration4.bin", "Fire Emblem 8/Chapters/10Eirika"); }
MapRecipe chapter_11_eirika()    { return load_visual_recipe("Ch11EirikaMap", "TileConfiguration4.bin", "Fire Emblem 8/Chapters/11Eirika"); }
MapRecipe chapter_12_eirika()    { return load_visual_recipe("Ch12EirikaMap", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/12Eirika"); }
MapRecipe chapter_13_eirika()    { return load_visual_recipe("Ch13EirikaMap", "TileConfiguration5.bin", "Fire Emblem 8/Chapters/13Eirika"); }
MapRecipe chapter_14_eirika()    { return load_visual_recipe("Ch14EirikaMap", "TileConfiguration6.bin", "Fire Emblem 8/Chapters/14Eirika"); }

MapRecipe chapter_9_ephraim()    { return load_visual_recipe("Ch9EphMap", "TileConfiguration5.bin", "Fire Emblem 8/Chapters/09Ephraim"); }
MapRecipe chapter_10_ephraim()   { return load_visual_recipe("Ch10EphraimMap", "TileConfiguration9.bin", "Fire Emblem 8/Chapters/10Ephraim"); }
MapRecipe chapter_11_ephraim()   { return load_visual_recipe("Ch11EphraimMap", "TileConfiguration2.bin", "Fire Emblem 8/Chapters/11Ephraim"); }
MapRecipe chapter_12_ephraim()   { return load_visual_recipe("Ch12EphraimMap", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/12Ephraim"); }
MapRecipe chapter_13_ephraim()   { return load_visual_recipe("Ch13EphraimMap", "TileConfiguration10.bin", "Fire Emblem 8/Chapters/13Ephraim"); }
MapRecipe chapter_14_ephraim()   { return load_visual_recipe("Ch14EphraimMap", "TileConfiguration6.bin", "Fire Emblem 8/Chapters/14Ephraim"); }

MapRecipe chapter_15()           { return load_visual_recipe("Ch15Map", "TileConfiguration3.bin", "Fire Emblem 8/Chapters/15"); }
MapRecipe chapter_16()           { return load_visual_recipe("Ch16Map", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/16"); }
MapRecipe chapter_17()           { return load_visual_recipe("Ch17Map", "TileConfiguration7.bin", "Fire Emblem 8/Chapters/17"); }
MapRecipe chapter_18()           { return load_visual_recipe("Ch18Map", "TileConfiguration3.bin", "Fire Emblem 8/Chapters/18"); }
MapRecipe chapter_19()           { return load_visual_recipe("Ch19Map", "TileConfiguration1.bin", "Fire Emblem 8/Chapters/19"); }
MapRecipe chapter_20()           { return load_visual_recipe("Ch20Map", "TileConfiguration8.bin", "Fire Emblem 8/Chapters/20"); }
MapRecipe final_chapter_1()      { return load_visual_recipe("FinalChapterMap1", "TileConfiguration8.bin", "Fire Emblem 8/Chapters/Final1"); }
MapRecipe final_chapter_2()      { return load_visual_recipe("FinalChapterMap2", "TileConfiguration8.bin", "Fire Emblem 8/Chapters/Final2"); }

MapRecipe tower_of_valni_1()     { return load_visual_recipe("TowerOfValni1Map", "TowerOfValniTileConfiguration.bin", "Fire Emblem 8/Tower of Valni/1"); }
MapRecipe tower_of_valni_2()     { return load_visual_recipe("TowerOfValni2Map", "TowerOfValniTileConfiguration.bin", "Fire Emblem 8/Tower of Valni/2"); }
MapRecipe tower_of_valni_3()     { return load_visual_recipe("TowerOfValni3Map", "TowerOfValniTileConfiguration.bin", "Fire Emblem 8/Tower of Valni/3"); }
MapRecipe tower_of_valni_4()     { return load_visual_recipe("TowerOfValni4Map", "TowerOfValniTileConfiguration.bin", "Fire Emblem 8/Tower of Valni/4"); }
MapRecipe tower_of_valni_5()     { return load_visual_recipe("TowerOfValni5Map", "TowerOfValniTileConfiguration.bin", "Fire Emblem 8/Tower of Valni/5"); }
MapRecipe tower_of_valni_6()     { return load_visual_recipe("TowerOfValni6Map", "TowerOfValniTileConfiguration.bin", "Fire Emblem 8/Tower of Valni/6"); }
MapRecipe tower_of_valni_7()     { return load_visual_recipe("TowerOfValni7Map", "TowerOfValniTileConfiguration.bin", "Fire Emblem 8/Tower of Valni/7"); }
MapRecipe tower_of_valni_8()     { return load_visual_recipe("TowerOfValni8Map", "TowerOfValniTileConfiguration.bin", "Fire Emblem 8/Tower of Valni/8"); }

MapRecipe lagdou_ruins_1()       { return load_visual_recipe("LagdouRuins1Map", "TileConfiguration9.bin", "Fire Emblem 8/Lagdou Ruins/01"); }
MapRecipe lagdou_ruins_2()       { return load_visual_recipe("LagdouRuins2Map", "TileConfiguration9.bin", "Fire Emblem 8/Lagdou Ruins/02"); }
MapRecipe lagdou_ruins_3()       { return load_visual_recipe("LagdouRuins3Map", "TileConfiguration9.bin", "Fire Emblem 8/Lagdou Ruins/03"); }
MapRecipe lagdou_ruins_4()       { return load_visual_recipe("LagdouRuins4Map", "TileConfiguration9.bin", "Fire Emblem 8/Lagdou Ruins/04"); }
MapRecipe lagdou_ruins_5()       { return load_visual_recipe("LagdouRuins5Map", "TileConfiguration9.bin", "Fire Emblem 8/Lagdou Ruins/05"); }
MapRecipe lagdou_ruins_6()       { return load_visual_recipe("LagdouRuins6Map", "TileConfiguration9.bin", "Fire Emblem 8/Lagdou Ruins/06"); }
MapRecipe lagdou_ruins_7()       { return load_visual_recipe("LagdouRuins7Map", "TileConfiguration9.bin", "Fire Emblem 8/Lagdou Ruins/07"); }
MapRecipe lagdou_ruins_8()       { return load_visual_recipe("LagdouRuins8Map", "TileConfiguration3.bin", "Fire Emblem 8/Lagdou Ruins/08"); }
MapRecipe lagdou_ruins_9()       { return load_visual_recipe("LagdouRuins9Map", "TileConfiguration7.bin", "Fire Emblem 8/Lagdou Ruins/09"); }
MapRecipe lagdou_ruins_10()      { return load_visual_recipe("LagdouRuins10Map", "TileConfiguration9.bin", "Fire Emblem 8/Lagdou Ruins/10"); }
}
