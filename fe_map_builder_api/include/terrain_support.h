#pragma once

#include <string>
#include <vector>

namespace fe_map_builder {

// The first eleven ids intentionally match the terrain ids already used by the
// sandbox. New values are appended so saved integer maps remain valid.
enum TerrainCategory {
    PLAINS = 0, ROAD, WOODS, DESERT, MOUNTAIN, RIVER, BRIDGE, FORT, WALL, VILLAGE, FLOOR,
    NONE, VILLAGE_CLOSED, HOUSE, ARMORY, VENDOR, ARENA, CHAMBER, CASTLE_GATE, THICKET, SAND,
    PEAK, BRIDGE_14, SEA, LAKE, MAGIC_FLOOR, FENCE, DAMAGED_WALL, RUBBLE, PILLAR, DOOR,
    THRONE, CHEST_EMPTY, CHEST_FULL, ROOF, GATE, CHURCH, RUINS, CLIFF, BALLISTA,
    LONGBALLISTA, KILLER_BALLISTA, SHIP, SHIP_WRECK, SPECIAL_2C, STAIRS, SPECIAL_2E,
    GLACIER, ARENA_30, VALLEY, FENCE_32, SNAG, BRIDGE_SNAG, SKY, DEEPS, RUINS_VILLAGE,
    INN, BARREL, BONE, DARK_TERRAIN, WATER, GUNNELS, DECK, BRACE, MAST,

    // Renderer-specific vocabulary. These map one-to-one to the consolidated
    // asset pack's visual groups; they do not change combat or movement rules.
    VISUAL_ARENA, VISUAL_ARMORY, VISUAL_BARREL, VISUAL_BONE, VISUAL_BRACE,
    VISUAL_BRACE_WALL, VISUAL_BRIDGE, VISUAL_CHEST, VISUAL_CLIFF, VISUAL_DARK,
    VISUAL_DASHDASH, VISUAL_DECK, VISUAL_DESERT, VISUAL_DOOR, VISUAL_FENCE,
    VISUAL_FENCE_BRACE, VISUAL_FENCE_WALL, VISUAL_FLAT, VISUAL_FLOOR,
    VISUAL_FOREST, VISUAL_FORT, VISUAL_GATE, VISUAL_GUNNELS, VISUAL_HOUSE,
    VISUAL_INN, VISUAL_LAKE, VISUAL_LAKE_CLIFF, VISUAL_MAST, VISUAL_MOUNTAIN,
    VISUAL_PEAK, VISUAL_PILLAR, VISUAL_PLAIN, VISUAL_PLAIN_ROAD, VISUAL_RIVER,
    VISUAL_ROAD, VISUAL_ROOF, VISUAL_RUINS, VISUAL_SAND, VISUAL_SEA,
    VISUAL_SNAG, VISUAL_STAIRS, VISUAL_THICKET, VISUAL_THRONE,
    VISUAL_UNDEFINED, VISUAL_VALLEY, VISUAL_VENDOR, VISUAL_VILLAGE,
    VISUAL_VILLAGE_HOUSE, VISUAL_WALL, VISUAL_WALL_BRACE, VISUAL_WALL2,
    VISUAL_WATER,
    TERRAIN_COUNT
};

struct Terrain {
    TerrainCategory category;
    double TRV;   // 1.0 = no penalty, 0.0 = impassable for the baseline mover.
    double HEAL;  // Fraction of maximum HP restored after a unit turn.
    double AVO;   // 0.20 = +20 avoid.
    int DEF;
    std::string info;
};

// FE8 common-unit defence/avoid data, expressed using this project's TRV and
// HEAL conventions. Unit-specific traversal exceptions belong in overrides.
// Header-local on purpose: this keeps the API C++11/C++14 compatible.
static const std::vector<Terrain> base_topo = {
    {PLAINS,1,0,0,0,"Plains"}, {ROAD,1,0,0,0,"Road"}, {WOODS,.5,0,.2,1,"Woods / Forest"},
    {DESERT,.5,0,.05,0,"Desert"}, {MOUNTAIN,.25,0,.3,1,"Mountain"}, {RIVER,.2,0,0,0,"River"},
    {BRIDGE,1,0,0,0,"Bridge"}, {FORT,.5,.2,.2,2,"Fort"}, {WALL,0,0,.2,1,"Wall"},
    {VILLAGE,1,0,.1,0,"Village"}, {FLOOR,1,0,0,0,"Indoor floor"},
    {NONE,0,0,0,0,"No terrain / void"}, {VILLAGE_CLOSED,0,0,.1,0,"Closed village"},
    {HOUSE,1,0,.1,0,"House"}, {ARMORY,1,0,.1,0,"Armory"}, {VENDOR,1,0,.1,0,"Vendor"},
    {ARENA,1,0,.1,0,"Arena"}, {CHAMBER,1,0,.1,0,"Chamber"},
    {CASTLE_GATE,1,.1,.2,3,"Castle gate"}, {THICKET,0,0,.3,2,"Thicket"},
    {SAND,1,0,.05,0,"Sand"}, {PEAK,0,0,.4,2,"Peak"}, {BRIDGE_14,0,0,0,0,"Special bridge"},
    {SEA,0,0,.1,0,"Sea"}, {LAKE,0,0,.1,0,"Lake"}, {MAGIC_FLOOR,1,0,0,0,"Magic floor"},
    {FENCE,0,0,.2,1,"Fence"}, {DAMAGED_WALL,0,0,0,0,"Destructible wall"},
    {RUBBLE,1,0,.05,0,"Rubble"}, {PILLAR,.5,0,.2,1,"Pillar"}, {DOOR,0,0,0,0,"Locked door"},
    {THRONE,1,.1,.3,3,"Throne"}, {CHEST_EMPTY,1,0,0,0,"Opened chest"},
    {CHEST_FULL,1,0,0,0,"Treasure chest"}, {ROOF,0,0,0,0,"Roof"},
    {GATE,1,.1,.3,3,"Gate"}, {CHURCH,1,.1,.15,0,"Church"}, {RUINS,.5,0,0,0,"Ruins"},
    {CLIFF,0,0,0,0,"Cliff"}, {BALLISTA,.5,0,.05,0,"Ballista emplacement"},
    {LONGBALLISTA,.5,0,.05,0,"Long ballista emplacement"},
    {KILLER_BALLISTA,.5,0,.05,0,"Killer ballista emplacement"}, {SHIP,1,0,0,0,"Ship"},
    {SHIP_WRECK,1,0,0,0,"Ship wreck"}, {SPECIAL_2C,0,0,0,0,"FE8 special terrain 0x2C"},
    {STAIRS,1,0,0,0,"Stairs"}, {SPECIAL_2E,0,0,0,0,"FE8 special terrain 0x2E"},
    {GLACIER,1,0,0,0,"Glacier"}, {ARENA_30,1,0,0,0,"Arena variant"},
    {VALLEY,0,0,.2,1,"Valley"}, {FENCE_32,0,0,.2,1,"Fence variant"},
    {SNAG,0,0,0,0,"Destructible snag"}, {BRIDGE_SNAG,1,0,0,0,"Destroyed bridge"},
    {SKY,0,0,0,0,"Sky"}, {DEEPS,0,0,0,0,"Deep water"},
    {RUINS_VILLAGE,1,0,.1,0,"Ruined village"}, {INN,1,0,.1,0,"Inn"},
    {BARREL,0,0,0,0,"Barrel"}, {BONE,0,0,0,0,"Bone"},
    {DARK_TERRAIN,0,0,0,0,"Darkness"}, {WATER,0,0,.1,0,"Water"},
    {GUNNELS,0,0,0,0,"Ship gunnels"}, {DECK,1,0,0,0,"Ship deck"},
    {BRACE,0,0,.2,1,"Ship brace"}, {MAST,0,0,.2,1,"Ship mast"},

    // Visual variants: same tactical values as their base terrain, but an
    // exact visual-group request for the self-contained renderer.
    {VISUAL_ARENA,1,0,.1,0,"Visual arena"}, {VISUAL_ARMORY,1,0,.1,0,"Visual armory"},
    {VISUAL_BARREL,0,0,0,0,"Visual barrel"}, {VISUAL_BONE,0,0,0,0,"Visual bone"},
    {VISUAL_BRACE,0,0,.2,1,"Visual brace"}, {VISUAL_BRACE_WALL,0,0,.2,1,"Visual brace wall"},
    {VISUAL_BRIDGE,1,0,0,0,"Visual bridge"}, {VISUAL_CHEST,1,0,0,0,"Visual chest"},
    {VISUAL_CLIFF,0,0,0,0,"Visual cliff"}, {VISUAL_DARK,0,0,0,0,"Visual darkness"},
    {VISUAL_DASHDASH,0,0,0,0,"Visual special terrain"}, {VISUAL_DECK,1,0,0,0,"Visual deck"},
    {VISUAL_DESERT,.5,0,.05,0,"Visual desert"}, {VISUAL_DOOR,0,0,0,0,"Visual door"},
    {VISUAL_FENCE,0,0,.2,1,"Visual fence"}, {VISUAL_FENCE_BRACE,0,0,.2,1,"Visual fence brace"},
    {VISUAL_FENCE_WALL,0,0,.2,1,"Visual fence wall"}, {VISUAL_FLAT,1,0,0,0,"Visual flat floor"},
    {VISUAL_FLOOR,1,0,0,0,"Visual floor"}, {VISUAL_FOREST,.5,0,.2,1,"Visual forest"},
    {VISUAL_FORT,.5,.2,.2,2,"Visual fort"}, {VISUAL_GATE,1,.1,.3,3,"Visual gate"},
    {VISUAL_GUNNELS,0,0,0,0,"Visual ship gunnels"}, {VISUAL_HOUSE,1,0,.1,0,"Visual house"},
    {VISUAL_INN,1,0,.1,0,"Visual inn"}, {VISUAL_LAKE,0,0,.1,0,"Visual lake"},
    {VISUAL_LAKE_CLIFF,0,0,0,0,"Visual lake cliff"}, {VISUAL_MAST,0,0,.2,1,"Visual mast"},
    {VISUAL_MOUNTAIN,.25,0,.3,1,"Visual mountain"}, {VISUAL_PEAK,0,0,.4,2,"Visual peak"},
    {VISUAL_PILLAR,.5,0,.2,1,"Visual pillar"}, {VISUAL_PLAIN,1,0,0,0,"Visual plain"},
    {VISUAL_PLAIN_ROAD,1,0,0,0,"Visual plain road"}, {VISUAL_RIVER,.2,0,0,0,"Visual river"},
    {VISUAL_ROAD,1,0,0,0,"Visual road"}, {VISUAL_ROOF,0,0,0,0,"Visual roof"},
    {VISUAL_RUINS,.5,0,0,0,"Visual ruins"}, {VISUAL_SAND,1,0,.05,0,"Visual sand"},
    {VISUAL_SEA,0,0,.1,0,"Visual sea"}, {VISUAL_SNAG,0,0,0,0,"Visual snag"},
    {VISUAL_STAIRS,1,0,0,0,"Visual stairs"}, {VISUAL_THICKET,0,0,.3,2,"Visual thicket"},
    {VISUAL_THRONE,1,.1,.3,3,"Visual throne"}, {VISUAL_UNDEFINED,0,0,0,0,"Undefined visual"},
    {VISUAL_VALLEY,0,0,.2,1,"Visual valley"}, {VISUAL_VENDOR,1,0,.1,0,"Visual vendor"},
    {VISUAL_VILLAGE,1,0,.1,0,"Visual village"}, {VISUAL_VILLAGE_HOUSE,1,0,.1,0,"Visual village house"},
    {VISUAL_WALL,0,0,.2,1,"Visual wall"}, {VISUAL_WALL_BRACE,0,0,.2,1,"Visual wall brace"},
    {VISUAL_WALL2,0,0,.2,1,"Visual wall variant"}, {VISUAL_WATER,0,0,.1,0,"Visual water"},
};

} // namespace fe_map_builder
