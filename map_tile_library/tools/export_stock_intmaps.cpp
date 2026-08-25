#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "maps.h"
#include "stock_intmaps.h"
using namespace std;

namespace
{
struct TileKey
{
    int theme_id = 0;
    int class_id = 0;
    int subclass_id = 0;
    int orientation_id = 0;
    int terrain_id = 0;

    bool operator<(const TileKey& other) const
    {
        return tie(theme_id, class_id, subclass_id, orientation_id, terrain_id) <
            tie(other.theme_id,
                     other.class_id,
                     other.subclass_id,
                     other.orientation_id,
                     other.terrain_id);
    }
};

struct VisualKey
{
    int theme_id = 0;
    int class_id = 0;
    int subclass_id = 0;
    int orientation_id = 0;

    bool operator<(const VisualKey& other) const
    {
        return tie(theme_id, class_id, subclass_id, orientation_id) <
            tie(other.theme_id,
                     other.class_id,
                     other.subclass_id,
                     other.orientation_id);
    }
};

struct StockMapSpec
{
    const char* name;
    maps::MapRecipe (*factory)();
};

const array<StockMapSpec, 48> kStockMaps = {{
    {"prologue", maps::prologue},
    {"chapter_1", maps::chapter_1},
    {"chapter_2", maps::chapter_2},
    {"chapter_3", maps::chapter_3},
    {"chapter_4", maps::chapter_4},
    {"chapter_5x", maps::chapter_5x},
    {"chapter_5", maps::chapter_5},
    {"chapter_6", maps::chapter_6},
    {"chapter_7", maps::chapter_7},
    {"chapter_8", maps::chapter_8},
    {"chapter_9_eirika", maps::chapter_9_eirika},
    {"chapter_10_eirika", maps::chapter_10_eirika},
    {"chapter_11_eirika", maps::chapter_11_eirika},
    {"chapter_12_eirika", maps::chapter_12_eirika},
    {"chapter_13_eirika", maps::chapter_13_eirika},
    {"chapter_14_eirika", maps::chapter_14_eirika},
    {"chapter_9_ephraim", maps::chapter_9_ephraim},
    {"chapter_10_ephraim", maps::chapter_10_ephraim},
    {"chapter_11_ephraim", maps::chapter_11_ephraim},
    {"chapter_12_ephraim", maps::chapter_12_ephraim},
    {"chapter_13_ephraim", maps::chapter_13_ephraim},
    {"chapter_14_ephraim", maps::chapter_14_ephraim},
    {"chapter_15", maps::chapter_15},
    {"chapter_16", maps::chapter_16},
    {"chapter_17", maps::chapter_17},
    {"chapter_18", maps::chapter_18},
    {"chapter_19", maps::chapter_19},
    {"chapter_20", maps::chapter_20},
    {"final_chapter_1", maps::final_chapter_1},
    {"final_chapter_2", maps::final_chapter_2},
    {"tower_of_valni_1", maps::tower_of_valni_1},
    {"tower_of_valni_2", maps::tower_of_valni_2},
    {"tower_of_valni_3", maps::tower_of_valni_3},
    {"tower_of_valni_4", maps::tower_of_valni_4},
    {"tower_of_valni_5", maps::tower_of_valni_5},
    {"tower_of_valni_6", maps::tower_of_valni_6},
    {"tower_of_valni_7", maps::tower_of_valni_7},
    {"tower_of_valni_8", maps::tower_of_valni_8},
    {"lagdou_ruins_1", maps::lagdou_ruins_1},
    {"lagdou_ruins_2", maps::lagdou_ruins_2},
    {"lagdou_ruins_3", maps::lagdou_ruins_3},
    {"lagdou_ruins_4", maps::lagdou_ruins_4},
    {"lagdou_ruins_5", maps::lagdou_ruins_5},
    {"lagdou_ruins_6", maps::lagdou_ruins_6},
    {"lagdou_ruins_7", maps::lagdou_ruins_7},
    {"lagdou_ruins_8", maps::lagdou_ruins_8},
    {"lagdou_ruins_9", maps::lagdou_ruins_9},
    {"lagdou_ruins_10", maps::lagdou_ruins_10},
}};

struct LoadedMap
{
    const char* name;
    maps::MapRecipe recipe;
};

void validate_recipe(const LoadedMap& map)
{
    const maps::MapRecipe& recipe = map.recipe;
    if (recipe.theme_id <= 0 || recipe.terrain.empty() || recipe.classes.empty() || recipe.tiles.empty())
    {
        throw runtime_error(string("Stock map is missing visual data: ") + map.name);
    }
    if (recipe.terrain.size() != recipe.classes.size() ||
        recipe.terrain.size() != recipe.tiles.size())
    {
        throw runtime_error(string("Stock map has inconsistent row counts: ") + map.name);
    }
    for (size_t y = 0; y < recipe.terrain.size(); ++y)
    {
        if (recipe.terrain[y].empty() ||
            recipe.terrain[y].size() != recipe.classes[y].size() ||
            recipe.terrain[y].size() != recipe.tiles[y].size())
        {
            throw runtime_error(string("Stock map has inconsistent row widths: ") + map.name);
        }
    }
}

TileKey tile_key_at(const maps::MapRecipe& recipe, size_t y, size_t x)
{
    return {
        recipe.theme_id,
        recipe.classes[y][x],
        maps::visual_subclass(recipe.tiles[y][x]),
        maps::visual_orientation(recipe.tiles[y][x]),
        recipe.terrain[y][x],
    };
}

VisualKey visual_key(const TileKey& key)
{
    return {key.theme_id, key.class_id, key.subclass_id, key.orientation_id};
}

stock_intmaps::TileCode cell_code(const TileKey& key)
{
    if (key.theme_id <= 0 || key.theme_id > 99 ||
        key.class_id < 0 || key.class_id > 99 ||
        key.subclass_id < 0 || key.subclass_id > 99 ||
        key.orientation_id < 0 || key.orientation_id > 999 ||
        key.terrain_id < 0 || key.terrain_id > 99)
    {
        throw runtime_error("A stock map tile does not fit the TTCCSSOOORR code format.");
    }

    return stock_intmaps::encode({
        key.theme_id,
        key.class_id,
        key.subclass_id,
        key.orientation_id,
        key.terrain_id,
    });
}

vector<string> split_tab_line(const string& line)
{
    vector<string> fields;
    size_t start = 0;
    while (true)
    {
        const size_t end = line.find('\t', start);
        fields.push_back(line.substr(start, end - start));
        if (end == string::npos)
        {
            return fields;
        }
        start = end + 1;
    }
}

map<VisualKey, string> load_visual_paths(const filesystem::path& path)
{
    ifstream file(path);
    if (!file)
    {
        throw runtime_error("Could not read legacy visual catalogue: " + path.string());
    }

    string line;
    if (!getline(file, line) ||
        line != "theme_id\ttheme_source\tclass_id\tclass_name\tsubclass_id\torientation_id\ttile_hash\trelative_png")
    {
        throw runtime_error("Legacy visual catalogue has an unexpected header: " + path.string());
    }

    map<VisualKey, string> paths;
    int line_number = 1;
    while (getline(file, line))
    {
        ++line_number;
        const vector<string> fields = split_tab_line(line);
        if (fields.size() != 8 || fields[7].empty())
        {
            throw runtime_error(
                "Legacy visual catalogue has an invalid row at line " +
                to_string(line_number) + "."
            );
        }

        const VisualKey key = {
            stoi(fields[0]),
            stoi(fields[2]),
            stoi(fields[4]),
            stoi(fields[5]),
        };
        const auto inserted = paths.emplace(key, fields[7]);
        if (!inserted.second && inserted.first->second != fields[7])
        {
            throw runtime_error("Legacy visual catalogue assigns two PNGs to one visual key.");
        }
    }
    return paths;
}

void write_code(ostream& file, stock_intmaps::TileCode code)
{
    file << setw(11) << setfill('0') << code << setfill(' ');
}

void write_dictionary(const filesystem::path& path,
                      const map<TileKey, string>& definitions)
{
    ofstream file(path, ios::trunc);
    if (!file)
    {
        throw runtime_error("Could not write tile dictionary: " + path.string());
    }

    file << "cell_code\trelative_png\n";
    for (const auto& entry : definitions)
    {
        write_code(file, cell_code(entry.first));
        file << '\t' << entry.second << '\n';
    }
}

void write_intmap(const filesystem::path& path,
                  const LoadedMap& map,
                  const ::map<TileKey, string>& definitions)
{
    ofstream file(path, ios::trunc);
    if (!file)
    {
        throw runtime_error("Could not write stock intmap: " + path.string());
    }

    for (size_t y = 0; y < map.recipe.terrain.size(); ++y)
    {
        for (size_t x = 0; x < map.recipe.terrain[y].size(); ++x)
        {
            if (x != 0)
            {
                file << '\t';
            }
            const TileKey key = tile_key_at(map.recipe, y, x);
            if (definitions.find(key) == definitions.end())
            {
                throw logic_error("Tile dictionary lost a loaded map cell.");
            }
            write_code(file, cell_code(key));
        }
        file << '\n';
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        const filesystem::path data_root = argc == 2
            ? filesystem::path(argv[1])
            : filesystem::path("map_tile_library/data");
        if (argc > 2)
        {
            throw invalid_argument("Usage: export_stock_intmaps [data-root]");
        }

        const map<VisualKey, string> visual_paths =
            load_visual_paths("map_tile_library/data/catalogue.tsv");

        vector<LoadedMap> maps;
        maps.reserve(kStockMaps.size());
        map<TileKey, string> definitions;
        for (const StockMapSpec& spec : kStockMaps)
        {
            maps.push_back({spec.name, spec.factory()});
            validate_recipe(maps.back());

            const maps::MapRecipe& recipe = maps.back().recipe;
            for (size_t y = 0; y < recipe.terrain.size(); ++y)
            {
                for (size_t x = 0; x < recipe.terrain[y].size(); ++x)
                {
                    const TileKey key = tile_key_at(recipe, y, x);
                    const auto visual = visual_paths.find(visual_key(key));
                    if (visual == visual_paths.end())
                    {
                        throw runtime_error(
                            "No source PNG exists for a stock map visual cell in " +
                            string(spec.name) + "."
                        );
                    }
                    definitions.emplace(key, visual->second);
                }
            }
        }

        const filesystem::path intmap_directory = data_root / "stock_intmaps";
        filesystem::create_directories(intmap_directory);
        write_dictionary(data_root / "tile_dictionary.tsv", definitions);
        for (const LoadedMap& map : maps)
        {
            write_intmap(intmap_directory / (string(map.name) + ".intmap.tsv"),
                         map,
                         definitions);
        }

        cout << "Exported " << maps.size() << " stock intmaps through " <<
            definitions.size() << " tile dictionary entries.\n";
        return 0;
    }
    catch (const exception& error)
    {
        cerr << "export_stock_intmaps: " << error.what() << '\n';
        return 1;
    }
}
