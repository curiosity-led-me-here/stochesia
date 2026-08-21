#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

// Native FE8 terrain IDs. Raw chapter maps already use this vocabulary.
enum TerrainId : int
{
    TERRAIN_NONE = 0x00, TERRAIN_PLAINS = 0x01, TERRAIN_ROAD = 0x02,
    TERRAIN_VILLAGE_REGULAR = 0x03, TERRAIN_VILLAGE_CLOSED = 0x04,
    TERRAIN_HOUSE = 0x05, TERRAIN_ARMORY = 0x06, TERRAIN_VENDOR = 0x07,
    TERRAIN_ARENA_REGULAR = 0x08, TERRAIN_C_ROOM_09 = 0x09,
    TERRAIN_FORT = 0x0A, TERRAIN_GATE_CASTLE = 0x0B,
    TERRAIN_FOREST = 0x0C, TERRAIN_THICKET = 0x0D,
    TERRAIN_SAND = 0x0E, TERRAIN_DESERT = 0x0F,
    TERRAIN_RIVER = 0x10, TERRAIN_MOUNTAIN = 0x11,
    TERRAIN_PEAK = 0x12, TERRAIN_BRIDGE_REGULAR = 0x13,
    TERRAIN_BRIDGE_14 = 0x14, TERRAIN_SEA = 0x15,
    TERRAIN_LAKE = 0x16, TERRAIN_FLOOR_REGULAR = 0x17,
    TERRAIN_FLOOR_MAGIC = 0x18, TERRAIN_FENCE_REGULAR = 0x19,
    TERRAIN_WALL_REGULAR = 0x1A, TERRAIN_WALL_DAMAGED = 0x1B,
    TERRAIN_RUBBLE = 0x1C, TERRAIN_PILLAR = 0x1D,
    TERRAIN_DOOR = 0x1E, TERRAIN_THRONE = 0x1F,
    TERRAIN_CHEST_EMPTY = 0x20, TERRAIN_CHEST_FULL = 0x21,
    TERRAIN_ROOF = 0x22, TERRAIN_GATE_REGULAR = 0x23,
    TERRAIN_CHURCH = 0x24, TERRAIN_RUINS_REGULAR = 0x25,
    TERRAIN_CLIFF = 0x26, TERRAIN_BALLISTA_REGULAR = 0x27,
    TERRAIN_BALLISTA_LONG = 0x28, TERRAIN_BALLISTA_KILLER = 0x29,
    TERRAIN_SHIP_FLAT = 0x2A, TERRAIN_SHIP_WRECK = 0x2B,
    TERRAIN_TILE_2C = 0x2C, TERRAIN_STAIRS = 0x2D,
    TERRAIN_TILE_2E = 0x2E, TERRAIN_GLACIER = 0x2F,
    TERRAIN_ARENA_30 = 0x30, TERRAIN_VALLEY = 0x31,
    TERRAIN_FENCE_32 = 0x32, TERRAIN_SNAG = 0x33,
    TERRAIN_BRIDGE_SNAG = 0x34, TERRAIN_SKY = 0x35,
    TERRAIN_DEEPS = 0x36, TERRAIN_RUINS_VILLAGE = 0x37,
    TERRAIN_INN = 0x38, TERRAIN_BARREL = 0x39,
    TERRAIN_BONE = 0x3A, TERRAIN_DARK = 0x3B,
    TERRAIN_WATER = 0x3C, TERRAIN_GUNNELS = 0x3D,
    TERRAIN_DECK = 0x3E, TERRAIN_BRACE = 0x3F,
    TERRAIN_MAST = 0x40, TERRAIN_COUNT = 0x41,
};

namespace terrain
{
constexpr int IMPASSABLE = -1;

// Each option corresponds to one of FE8's normal-weather movement tables.
enum class MovementType : std::uint8_t
{
    CommonT2, CommonT1, Armor, Fighter, Berserker, Brigand, Pirate, Thief,
    Magic, Civilian, HorseT1, HorseT2, AnimalT1, AnimalT2, Fly, DemonKing,
};

struct TerrainData
{
    TerrainId id;
    std::string_view name;
    int heal_percent;
    int avoid_bonus;
    int defense_bonus;
    // The tile is blocked now, but a gameplay action can make it traversable.
    bool passable_with_action;
};

const TerrainData& get(int terrain_id);
int movement_cost(int terrain_id, MovementType movement);
bool can_enter(int terrain_id, MovementType movement);
bool is_known(int terrain_id);
bool blocks_common_foot(int terrain_id);
bool is_passable_with_action(int terrain_id);
std::vector<int> default_obstacle_ids();
std::string_view movement_type_name(MovementType movement);

// One-way import adapter for existing fe_map_builder_api recipes labelled
// "strategic_procedural_generation_v1". New gameplay maps should use the
// native TerrainId values above directly.
int v1_id_to_fe8(int legacy_id);
std::vector<std::vector<int>> v1_map_to_fe8(const std::vector<std::vector<int>>& legacy_map);
}
