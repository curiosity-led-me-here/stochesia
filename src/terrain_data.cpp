#include "terrain_data.h"

#include <array>
#include <cstddef>

namespace
{
using terrain::MovementType;

constexpr std::array<std::string_view, TERRAIN_COUNT> kNames = {
    "Void", "Plains", "Road", "Village", "Closed village", "House", "Armory", "Vendor",
    "Arena", "Room", "Fort", "Castle gate", "Forest", "Thicket", "Sand", "Desert",
    "River", "Mountain", "Peak", "Bridge", "Bridge", "Sea", "Lake", "Floor",
    "Magic floor", "Fence", "Wall", "Damaged wall", "Rubble", "Pillar", "Door", "Throne",
    "Open chest", "Treasure chest", "Roof", "Gate", "Church", "Ruins", "Cliff", "Ballista",
    "Long ballista", "Killer ballista", "Ship", "Ship wreck", "Terrain 2C", "Stairs", "Terrain 2E", "Glacier",
    "Arena", "Valley", "Fence", "Snag", "Bridge snag", "Sky", "Deeps", "Ruined village",
    "Inn", "Barrel", "Bone", "Darkness", "Water", "Gunnels", "Deck", "Brace", "Mast",
};

// Direct transcription of FE8's TerrainTable_HealAmount, Avo_Common and
// Def_Common. Values are applied after combat takes place on a tile.
constexpr std::array<int, TERRAIN_COUNT> kHeal = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 10, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10,
    0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

constexpr std::array<int, TERRAIN_COUNT> kAvoid = {
    0, 0, 0, 10, 10, 10, 10, 10, 10, 10, 20, 20, 20, 30, 5, 5,
    0, 30, 40, 0, 0, 10, 10, 0, 0, 20, 20, 0, 5, 20, 0, 30,
    0, 0, 0, 30, 15, 0, 0, 5, 5, 5, 0, 0, 0, 0, 0, 0,
    0, 20, 20, 0, 0, 0, 0, 10, 10, 0, 0, 0, 10, 0, 0, 20, 20,
};

constexpr std::array<int, TERRAIN_COUNT> kDefense = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 1, 2, 0, 0,
    0, 1, 2, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 3,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
};

// Rows are FE8's normal-weather movement-cost tables. -1 means impassable.
constexpr std::array<std::array<int, TERRAIN_COUNT>, 16> kMovementCosts = {{
    // CommonT2
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 2, 5, 4, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // CommonT1
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 2, -1, 4, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // Armor
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 3, -1, -1, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // Fighter
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 3, -1, 3, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // Berserker
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 2, 2, 3, 4, 1, -1, 2, 3, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, 3, -1, 1, -1, -1},
    // Brigand
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 2, 5, 3, 4, 1, -1, -1, -1, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // Pirate
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 2, 2, 3, -1, 1, -1, 2, 3, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, 3, -1, 1, -1, -1},
    // Thief
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 2, 5, 4, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // Magic
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 1, -1, 4, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // Civilian
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 2, -1, 4, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 1, 2, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // HorseT1
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 3, -1, 1, 4, -1, -1, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 2, 3, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // HorseT2 / Paladin
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 3, -1, 1, 4, -1, 6, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 2, 3, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // AnimalT1
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 3, -1, -1, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 2, 3, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // AnimalT2
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 3, 5, 5, -1, 1, -1, -1, -1, 1, 1, -1, -1, -1, 2, 3, -1, 1, 1, 1, -1, 1, 1, 2, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
    // Fly
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, 1, 1, 1, -1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1},
    // DemonKing
    {-1, 1, 1, 1, -1, 1, 1, 1, 1, 1, 2, 1, 2, -1, 1, 2, -1, 4, -1, 1, -1, -1, -1, 1, 1, 1, -1, -1, 1, 2, -1, 2, 1, 1, -1, 1, 1, 1, -1, 2, 2, 2, 1, 1, -1, 1, -1, 1, 1, -1, 1, 0, 0, -1, -1, 1, 1, -1, -1, -1, -1, -1, 1, -1, -1},
}};

const std::array<terrain::TerrainData, TERRAIN_COUNT> kTerrainData = []
{
    std::array<terrain::TerrainData, TERRAIN_COUNT> data{};
    for (int id = 0; id < TERRAIN_COUNT; ++id)
    {
        data[id] = {static_cast<TerrainId>(id), kNames[id], kHeal[id], kAvoid[id], kDefense[id]};
    }
    return data;
}();

const terrain::TerrainData kUnknownTerrain = {
    TERRAIN_NONE, "Unknown terrain", 0, 0, 0
};

constexpr std::array<std::string_view, 16> kMovementNames = {
    "Common T2", "Common T1", "Armor", "Fighter", "Berserker", "Brigand", "Pirate", "Thief",
    "Magic", "Civilian", "Horse T1", "Horse T2", "Animal T1", "Animal T2", "Fly", "Demon King",
};

constexpr std::array<int, 65> kV1ToFe8 = {
    TERRAIN_PLAINS, TERRAIN_ROAD, TERRAIN_FOREST, TERRAIN_DESERT,
    TERRAIN_MOUNTAIN, TERRAIN_RIVER, TERRAIN_BRIDGE_REGULAR, TERRAIN_FORT,
    TERRAIN_WALL_REGULAR, TERRAIN_VILLAGE_REGULAR, TERRAIN_FLOOR_REGULAR,
    TERRAIN_NONE, TERRAIN_VILLAGE_CLOSED, TERRAIN_HOUSE, TERRAIN_ARMORY,
    TERRAIN_VENDOR, TERRAIN_ARENA_REGULAR, TERRAIN_C_ROOM_09,
    TERRAIN_GATE_CASTLE, TERRAIN_THICKET, TERRAIN_SAND, TERRAIN_PEAK,
    TERRAIN_BRIDGE_14, TERRAIN_SEA, TERRAIN_LAKE, TERRAIN_FLOOR_MAGIC,
    TERRAIN_FENCE_REGULAR,
    TERRAIN_WALL_DAMAGED, TERRAIN_RUBBLE, TERRAIN_PILLAR, TERRAIN_DOOR,
    TERRAIN_THRONE, TERRAIN_CHEST_EMPTY, TERRAIN_CHEST_FULL, TERRAIN_ROOF,
    TERRAIN_GATE_REGULAR, TERRAIN_CHURCH, TERRAIN_RUINS_REGULAR, TERRAIN_CLIFF,
    TERRAIN_BALLISTA_REGULAR, TERRAIN_BALLISTA_LONG, TERRAIN_BALLISTA_KILLER,
    TERRAIN_SHIP_FLAT, TERRAIN_SHIP_WRECK, TERRAIN_TILE_2C, TERRAIN_STAIRS,
    TERRAIN_TILE_2E, TERRAIN_GLACIER, TERRAIN_ARENA_30, TERRAIN_VALLEY,
    TERRAIN_FENCE_32, TERRAIN_SNAG, TERRAIN_BRIDGE_SNAG, TERRAIN_SKY,
    TERRAIN_DEEPS, TERRAIN_RUINS_VILLAGE, TERRAIN_INN, TERRAIN_BARREL,
    TERRAIN_BONE, TERRAIN_DARK, TERRAIN_WATER, TERRAIN_GUNNELS, TERRAIN_DECK,
    TERRAIN_BRACE, TERRAIN_MAST,
};
}

namespace terrain
{
const TerrainData& get(int terrain_id)
{
    return is_known(terrain_id) ? kTerrainData[terrain_id] : kUnknownTerrain;
}

int movement_cost(int terrain_id, MovementType movement)
{
    if (!is_known(terrain_id)) return IMPASSABLE;
    return kMovementCosts[static_cast<std::size_t>(movement)][terrain_id];
}

bool can_enter(int terrain_id, MovementType movement)
{
    return movement_cost(terrain_id, movement) != IMPASSABLE;
}

bool is_known(int terrain_id)
{
    return terrain_id >= TERRAIN_NONE && terrain_id < TERRAIN_COUNT;
}

bool blocks_common_foot(int terrain_id)
{
    return !can_enter(terrain_id, MovementType::CommonT1);
}

std::vector<int> default_obstacle_ids()
{
    return {TERRAIN_WALL_REGULAR, TERRAIN_FENCE_REGULAR, TERRAIN_CLIFF, TERRAIN_PEAK};
}

std::string_view movement_type_name(MovementType movement)
{
    return kMovementNames[static_cast<std::size_t>(movement)];
}

int v1_id_to_fe8(int legacy_id)
{
    if (legacy_id < 0 || legacy_id >= static_cast<int>(kV1ToFe8.size()))
    {
        return TERRAIN_NONE;
    }
    return kV1ToFe8[legacy_id];
}

std::vector<std::vector<int>> v1_map_to_fe8(const std::vector<std::vector<int>>& legacy_map)
{
    std::vector<std::vector<int>> converted = legacy_map;
    for (std::vector<int>& row : converted)
    {
        for (int& terrain_id : row)
        {
            terrain_id = v1_id_to_fe8(terrain_id);
        }
    }
    return converted;
}
}
