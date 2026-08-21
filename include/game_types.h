#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "terrain_data.h"

// IDs 65–116 select an exact visual group in fe_map_builder. They are renderer
// only; gameplay terrain uses the native FE8 TerrainId vocabulary.
enum VisualTerrainID
{
    VISUAL_ARENA = 65,
    VISUAL_ARMORY,
    VISUAL_BARREL,
    VISUAL_BONE,
    VISUAL_BRACE,
    VISUAL_BRACE_WALL,
    VISUAL_BRIDGE,
    VISUAL_CHEST,
    VISUAL_CLIFF,
    VISUAL_DARK,
    VISUAL_DASHDASH,
    VISUAL_DECK,
    VISUAL_DESERT,
    VISUAL_DOOR,
    VISUAL_FENCE,
    VISUAL_FENCE_BRACE,
    VISUAL_FENCE_WALL,
    VISUAL_FLAT,
    VISUAL_FLOOR,
    VISUAL_FOREST,
    VISUAL_FORT,
    VISUAL_GATE,
    VISUAL_GUNNELS,
    VISUAL_HOUSE,
    VISUAL_INN,
    VISUAL_LAKE,
    VISUAL_LAKE_CLIFF,
    VISUAL_MAST,
    VISUAL_MOUNTAIN,
    VISUAL_PEAK,
    VISUAL_PILLAR,
    VISUAL_PLAIN,
    VISUAL_PLAIN_ROAD,
    VISUAL_RIVER,
    VISUAL_ROAD,
    VISUAL_ROOF,
    VISUAL_RUINS,
    VISUAL_SAND,
    VISUAL_SEA,
    VISUAL_SNAG,
    VISUAL_STAIRS,
    VISUAL_THICKET,
    VISUAL_THRONE,
    VISUAL_UNDEFINED,
    VISUAL_VALLEY,
    VISUAL_VENDOR,
    VISUAL_VILLAGE,
    VISUAL_VILLAGE_HOUSE,
    VISUAL_WALL,
    VISUAL_WALL_BRACE,
    VISUAL_WALL2,
    VISUAL_WATER,

    VISUAL_TERRAIN_COUNT
};


enum WeaponCategory
{
    NONETYPE = -1,
    SWORD = 0,
    LANCE = 1,
    AXE   = 2,
    BOW   = 3,
    ANIMA = 4,
    LIGHT = 5,
    DARK  = 6,
    STAFF = 7,
};

enum UnitClass
{
    FOOT,
    CAVALIER,
    ARMOURED,
    FLIER,
    WRYM,
    BANDIT,
    PIRATE,
    MAGIC,
};


enum ItemID
{
    MEND = -6,
    HEAL = -5,
    ELIXIR = -4,
    VULNERARY = -3,
    KEY = -2,
    NO_ITEM = -1,
    IRON_SWORD = 0,
    IRON_LANCE = 1,
    IRON_AXE = 2,
    IRON_BOW = 3,
    FIRE = 4,
    LIGHTNING = 5,
    FLUX = 6,
    THUNDER = 7,
};

struct Weapon
{
    std::string NAME;
    std::string CAT_NAME;
    std::string MIN_RNK;
    int RNKEXP;
    ItemID ID;
    WeaponCategory CAT;
    int DUR;
    int MT;
    int WT;
    int HIT;
    int CRIT;
    int MINRG;
    int MAXRG;
    std::vector<UnitClass> effective;
};

struct Healer
{
    std::string Name;
    ItemID ID;
    WeaponCategory CAT;
    int RNKEXP;
    int HEALHP;
};

struct Stats
{
    int HP;
    int STR;
    int MAG;
    int SKL;
    int SPD;
    int LUC;
    int DEF;
    int RES;
    int MOV;
    int CON;
};

struct Growth
{
    double HP;
    double STR;
    double MAG;
    double SKL;
    double SPD;
    double LUC;
    double DEF;
    double RES;
    double MOV;
    double CON;
};

struct LevelExp
{
    int current;
    int next;
};

enum alpharank
{
    E = 0,
    D = 1,
    C = 2,
    B = 3,
    A = 4,
    S = 5,
};

struct Rank
{
    alpharank ID;
    int minexp;
    int maxexp;
};

struct WeaponLevelExp
{
    Rank rank;
    int current;
};

struct EntityWeaponExp
{
    WeaponLevelExp Sword;
    WeaponLevelExp Axe;
    WeaponLevelExp Lance;
    WeaponLevelExp Bow;
    WeaponLevelExp Anima;
    WeaponLevelExp Light;
    WeaponLevelExp Dark;
    WeaponLevelExp Staff;
};

struct ItemStack
{
    ItemID ID = NO_ITEM;
    int usesRemaining = 0;
};

struct Inventory
{
    ItemStack slot[5];
    int EquippedSlot = -1;
};

struct WeaponAffinity
{
   std::vector<WeaponCategory> UsableWeapons;
};

struct Guild;

struct Entity
{
    bool alive=true;
    bool turn=true;
    std::string name;
    UnitClass unitclass;
    int entity_id;
    std::vector<int> location;
    std::vector<std::vector<int>> path;
    std::vector<std::vector<int>> attack_range;
    WeaponAffinity type;
    int Lvl;
    LevelExp Exp;
    Stats ogstats;
    Stats stats;
    Inventory inventory;
    EntityWeaponExp WExp;
    Growth growth;
    terrain::MovementType movement = terrain::MovementType::CommonT1;
    int terrain_id;
    Guild* group = nullptr;
};

struct Guild
{
    std::string name;
    int guild_id = -1; // Guild_ID can never be zero
    std::vector<Entity*> members;
    void add(Entity& unit)
    {
	if (unit.group == nullptr)
	{
	    members.push_back(&unit);
	    unit.group = this;
	}
	else throw std::invalid_argument("Already in a guild!");
    }
    void remove(Entity& unit)
    {
	members.erase(std::remove(members.begin(), members.end(), &unit), members.end());
	unit.group = nullptr;
    }
};

struct CombatInfo
{
    int HP;
    int MT;
    int HIT;
    bool DB;
    int CRIT;
    int WTA;
    bool counter;
};

struct avl_for_atk
{
    std::vector<int> coords;
    Weapon weapon;
    int inventory_id;
};

struct Command
{
    std::string name;
    int id;
    std::vector<int> coords;
};

struct sequence
{
    Entity& unit;
    int turn;
    Entity& opp;

    // battle() resolves its mechanics immediately, but map combat is played
    // round by round afterward. Preserve the HP snapshot made directly after
    // this round so the renderer can lower a bar at its visible hit frame.
    int unit_hp_after;
    int opp_hp_after;

    sequence(Entity& attacker, int outcome, Entity& opponent)
        : unit(attacker),
          turn(outcome),
          opp(opponent),
          unit_hp_after(attacker.stats.HP),
          opp_hp_after(opponent.stats.HP)
    {
    }
};

struct paths
{
    std::vector<std::vector<int>> pathset;
    int tries;
};
