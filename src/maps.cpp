#include "maps.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
const std::string map_root = "/Users/ashu/FE8/graphics/map/";
const int terrain_lookup_offset = 0x2000;

std::vector<unsigned char> read_binary(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Could not open FE8 map asset: " + path);

    return std::vector<unsigned char>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

int read_json_number(const std::string& json, const std::string& key)
{
    std::size_t start = json.find("\"" + key + "\"");
    if (start == std::string::npos)
        throw std::runtime_error("Map metadata is missing " + key);

    start = json.find(':', start);
    if (start == std::string::npos)
        throw std::runtime_error("Map metadata has an invalid " + key);

    return std::stoi(json.substr(start + 1));
}

maps::TerrainMap load(const std::string& layout_name, const std::string& config_name)
{
    const std::string layout_dir = map_root + "layout/";

    std::ifstream metadata_file(layout_dir + layout_name + ".json");
    if (!metadata_file)
        throw std::runtime_error("Could not open FE8 map metadata: " + layout_name);

    std::string metadata{
        std::istreambuf_iterator<char>(metadata_file),
        std::istreambuf_iterator<char>()
    };

    int width = read_json_number(metadata, "width");
    int height = read_json_number(metadata, "height");

    std::vector<unsigned char> layout = read_binary(layout_dir + layout_name + ".mar");
    std::vector<unsigned char> config = read_binary(map_root + config_name);

    if (layout.size() != static_cast<std::size_t>(width * height * 2))
        throw std::runtime_error("Unexpected layout size for " + layout_name);

    maps::TerrainMap output(height, std::vector<int>(width));

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            std::size_t layout_index = static_cast<std::size_t>((y * width + x) * 2);
            std::uint16_t base_tile = static_cast<std::uint16_t>(layout[layout_index]) |
                                      (static_cast<std::uint16_t>(layout[layout_index + 1]) << 8);

            // .mar files are FEBuilder editor cells. FE8's own
            // scripts/mar_to_map.py converts them to base tiles by >> 3.
            base_tile >>= 3;

            std::size_t terrain_index = terrain_lookup_offset + (base_tile >> 2);
            if (terrain_index >= config.size())
                throw std::runtime_error("Invalid tile ID in " + layout_name);

            output[y][x] = config[terrain_index];
        }
    }

    return output;
}
}

namespace maps
{
TerrainMap prologue()             { return load("PrologueMap", "TileConfiguration1.bin"); }
TerrainMap chapter_1()            { return load("Ch1Map", "TileConfiguration1.bin"); }
TerrainMap chapter_2()            { return load("Ch2Map", "TileConfiguration1.bin"); }
TerrainMap chapter_3()            { return load("Ch3Map", "TileConfiguration2.bin"); }
TerrainMap chapter_4()            { return load("Ch4Map", "TileConfiguration1.bin"); }
TerrainMap chapter_5x()           { return load("Ch5XMap", "TileConfiguration3.bin"); }
TerrainMap chapter_5()            { return load("Ch5Map", "TileConfiguration2.bin"); }
TerrainMap chapter_6()            { return load("Ch6Map", "TileConfiguration1.bin"); }
TerrainMap chapter_7()            { return load("Ch7Map", "TileConfiguration1.bin"); }
TerrainMap chapter_8()            { return load("Ch8Map", "TileConfiguration3.bin"); }

TerrainMap chapter_9_eirika()     { return load("Ch9EirikaMap", "TileConfiguration2.bin"); }
TerrainMap chapter_10_eirika()    { return load("Ch10EirikaMap", "TileConfiguration4.bin"); }
TerrainMap chapter_11_eirika()    { return load("Ch11EirikaMap", "TileConfiguration4.bin"); }
TerrainMap chapter_12_eirika()    { return load("Ch12EirikaMap", "TileConfiguration1.bin"); }
TerrainMap chapter_13_eirika()    { return load("Ch13EirikaMap", "TileConfiguration5.bin"); }
TerrainMap chapter_14_eirika()    { return load("Ch14EirikaMap", "TileConfiguration6.bin"); }

TerrainMap chapter_9_ephraim()    { return load("Ch9EphMap", "TileConfiguration5.bin"); }
TerrainMap chapter_10_ephraim()   { return load("Ch10EphraimMap", "TileConfiguration9.bin"); }
TerrainMap chapter_11_ephraim()   { return load("Ch11EphraimMap", "TileConfiguration2.bin"); }
TerrainMap chapter_12_ephraim()   { return load("Ch12EphraimMap", "TileConfiguration1.bin"); }
TerrainMap chapter_13_ephraim()   { return load("Ch13EphraimMap", "TileConfiguration10.bin"); }
TerrainMap chapter_14_ephraim()   { return load("Ch14EphraimMap", "TileConfiguration6.bin"); }

TerrainMap chapter_15()           { return load("Ch15Map", "TileConfiguration3.bin"); }
TerrainMap chapter_16()           { return load("Ch16Map", "TileConfiguration1.bin"); }
TerrainMap chapter_17()           { return load("Ch17Map", "TileConfiguration7.bin"); }
TerrainMap chapter_18()           { return load("Ch18Map", "TileConfiguration3.bin"); }
TerrainMap chapter_19()           { return load("Ch19Map", "TileConfiguration1.bin"); }
TerrainMap chapter_20()           { return load("Ch20Map", "TileConfiguration8.bin"); }
TerrainMap final_chapter_1()      { return load("FinalChapterMap1", "TileConfiguration8.bin"); }
TerrainMap final_chapter_2()      { return load("FinalChapterMap2", "TileConfiguration8.bin"); }

TerrainMap tower_of_valni_1()     { return load("TowerOfValni1Map", "TowerOfValniTileConfiguration.bin"); }
TerrainMap tower_of_valni_2()     { return load("TowerOfValni2Map", "TowerOfValniTileConfiguration.bin"); }
TerrainMap tower_of_valni_3()     { return load("TowerOfValni3Map", "TowerOfValniTileConfiguration.bin"); }
TerrainMap tower_of_valni_4()     { return load("TowerOfValni4Map", "TowerOfValniTileConfiguration.bin"); }
TerrainMap tower_of_valni_5()     { return load("TowerOfValni5Map", "TowerOfValniTileConfiguration.bin"); }
TerrainMap tower_of_valni_6()     { return load("TowerOfValni6Map", "TowerOfValniTileConfiguration.bin"); }
TerrainMap tower_of_valni_7()     { return load("TowerOfValni7Map", "TowerOfValniTileConfiguration.bin"); }
TerrainMap tower_of_valni_8()     { return load("TowerOfValni8Map", "TowerOfValniTileConfiguration.bin"); }

TerrainMap lagdou_ruins_1()       { return load("LagdouRuins1Map", "TileConfiguration9.bin"); }
TerrainMap lagdou_ruins_2()       { return load("LagdouRuins2Map", "TileConfiguration9.bin"); }
TerrainMap lagdou_ruins_3()       { return load("LagdouRuins3Map", "TileConfiguration9.bin"); }
TerrainMap lagdou_ruins_4()       { return load("LagdouRuins4Map", "TileConfiguration9.bin"); }
TerrainMap lagdou_ruins_5()       { return load("LagdouRuins5Map", "TileConfiguration9.bin"); }
TerrainMap lagdou_ruins_6()       { return load("LagdouRuins6Map", "TileConfiguration9.bin"); }
TerrainMap lagdou_ruins_7()       { return load("LagdouRuins7Map", "TileConfiguration9.bin"); }
TerrainMap lagdou_ruins_8()       { return load("LagdouRuins8Map", "TileConfiguration3.bin"); }
TerrainMap lagdou_ruins_9()       { return load("LagdouRuins9Map", "TileConfiguration7.bin"); }
TerrainMap lagdou_ruins_10()      { return load("LagdouRuins10Map", "TileConfiguration9.bin"); }
}
