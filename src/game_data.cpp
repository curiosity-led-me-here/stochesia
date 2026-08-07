#include <vector>
#include "game_data.h"

const std::vector<Weapon> Armory=
{
    // NAME, CAT_NAME, MIN_RNK, RNKEXP, ID, CAT, DUR, MT, WT, HIT, CRIT, MINRG, MAXRG

    {"Iron Sword", "Sword", "E",  1, IRON_SWORD, SWORD, 46, 5,  5, 90, 0, 1, 1},
    {"Iron Lance", "Lance", "E",  1, IRON_LANCE, LANCE, 45, 7,  8, 80, 0, 1, 1},
    {"Iron Axe",   "Axe",   "E",  1, IRON_AXE, AXE,   45, 8, 10, 75, 0, 1, 1},
    {"Iron Bow",   "Bow",   "E",  1, IRON_BOW, BOW,   45, 6,  5, 85, 0, 2, 2},

    {"Fire",       "Anima", "E",  1, FIRE, ANIMA, 40, 5,  4, 90, 0, 1, 2},
    {"Lightning",  "Light", "E",  1, LIGHTNING, LIGHT, 35, 4,  6, 95, 5, 1, 2},
    {"Flux",       "Dark",  "D", 31, FLUX, DARK,  45, 7,  8, 80, 0, 1, 2},

    {"Thunder",    "Anima", "D", 31, THUNDER, ANIMA, 35, 8,  6, 80, 5, 1, 2},
};

// ID, TRV, HEAL, AVO, DEF, PASSTHROUGH, info
const std::vector<Terrain> base_topo =
{
    { 0, 1,  0,  0, 0, true,  "Plains"},
    { 1, 1,  0,  0, 0, true,  "Road"},
    { 2, 2,  0, 20, 1, true,  "Woods / Forest"},
    { 3, 2,  0,  5, 0, true,  "Desert"},
    { 4, 4,  0, 30, 1, true,  "Mountain"},
    { 5, 5,  0,  0, 0, true,  "River"},
    { 6, 1,  0,  0, 0, true,  "Bridge"},
    { 7, 2, 20, 20, 2, true,  "Fort"},
    { 8, 0,  0, 20, 1, false, "Wall"},
    { 9, 1,  0, 10, 0, true,  "Village"},
    {10, 1,  0,  0, 0, true,  "Indoor floor"},

    {11, 0,  0,  0, 0, false, "No terrain / void"},
    {12, 0,  0, 10, 0, false, "Closed village"},
    {13, 1,  0, 10, 0, true,  "House"},
    {14, 1,  0, 10, 0, true,  "Armory"},
    {15, 1,  0, 10, 0, true,  "Vendor"},
    {16, 1,  0, 10, 0, true,  "Arena"},
    {17, 1,  0, 10, 0, true,  "Chamber"},
    {18, 1, 10, 20, 3, true,  "Castle gate"},
    {19, 0,  0, 30, 2, false, "Thicket"},
    {20, 1,  0,  5, 0, true,  "Sand"},
    {21, 0,  0, 40, 2, false, "Peak"},
    {22, 0,  0,  0, 0, false, "Special bridge"},
    {23, 0,  0, 10, 0, false, "Sea"},
    {24, 0,  0, 10, 0, false, "Lake"},
    {25, 1,  0,  0, 0, true,  "Magic floor"},
    {26, 0,  0, 20, 1, false, "Fence"},
    {27, 0,  0,  0, 0, false, "Destructible wall"},
    {28, 1,  0,  5, 0, true,  "Rubble"},
    {29, 2,  0, 20, 1, true,  "Pillar"},
    {30, 0,  0,  0, 0, false, "Locked door"},
    {31, 1, 10, 30, 3, true,  "Throne"},
    {32, 1,  0,  0, 0, true,  "Opened chest"},
    {33, 1,  0,  0, 0, true,  "Treasure chest"},
    {34, 0,  0,  0, 0, false, "Roof"},
    {35, 1, 10, 30, 3, true,  "Gate"},
    {36, 1, 10, 15, 0, true,  "Church"},
    {37, 2,  0,  0, 0, true,  "Ruins"},
    {38, 0,  0,  0, 0, false, "Cliff"},
    {39, 2,  0,  5, 0, true,  "Ballista emplacement"},
    {40, 2,  0,  5, 0, true,  "Long ballista emplacement"},
    {41, 2,  0,  5, 0, true,  "Killer ballista emplacement"},
    {42, 1,  0,  0, 0, true,  "Ship"},
    {43, 1,  0,  0, 0, true,  "Ship wreck"},
    {44, 0,  0,  0, 0, false, "FE8 special terrain 0x2C"},
    {45, 1,  0,  0, 0, true,  "Stairs"},
    {46, 0,  0,  0, 0, false, "FE8 special terrain 0x2E"},
    {47, 1,  0,  0, 0, true,  "Glacier"},
    {48, 1,  0,  0, 0, true,  "Arena variant"},
    {49, 0,  0, 20, 1, false, "Valley"},
    {50, 0,  0, 20, 1, false, "Fence variant"},
    {51, 0,  0,  0, 0, false, "Destructible snag"},
    {52, 1,  0,  0, 0, true,  "Destroyed bridge"},
    {53, 0,  0,  0, 0, false, "Sky"},
    {54, 0,  0,  0, 0, false, "Deep water"},
    {55, 1,  0, 10, 0, true,  "Ruined village"},
    {56, 1,  0, 10, 0, true,  "Inn"},
    {57, 0,  0,  0, 0, false, "Barrel"},
    {58, 0,  0,  0, 0, false, "Bone"},
    {59, 0,  0,  0, 0, false, "Darkness"},
    {60, 0,  0, 10, 0, false, "Water"},
    {61, 0,  0,  0, 0, false, "Ship gunnels"},
    {62, 1,  0,  0, 0, true,  "Ship deck"},
    {63, 0,  0, 20, 1, false, "Ship brace"},
    {64, 0,  0, 20, 1, false, "Ship mast"},
};

const std::vector<Rank> TierInfo=
{
    // ID, minexp, maxexp

    {E,   1,  30},
    {D,  31,  70},
    {C,  71, 120},
    {B, 121, 180},
    {A, 181, 250},
    {S, 251, 251},
};

Weapon get_weapon(const std::vector<Weapon>& Armory, int id)
{
    return findbyid(Armory, id);
}

void get_next_rank(WeaponLevelExp& x)
{
    if (x.rank.ID != S)
    {
	if (x.current >= x.rank.maxexp)
	{
	    x.rank.ID = static_cast<alpharank>(x.rank.ID+1);
	    x.current = x.current - x.rank.maxexp;
	    Rank new_rank_stats = findbyid(TierInfo, x.rank.ID);
	    x.rank = new_rank_stats;
	    if (x.current >= x.rank.maxexp)
	    {
		get_next_rank(x);
	    }
	}
    }
}


