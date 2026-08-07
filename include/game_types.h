#pragma once
#include <vector>
#include <string>
#include <algorithm>


// IDs 65–116 select an exact visual group in fe_map_builder. They are for the
// renderer only: do not index base_topo with them. The gameplay terrain beneath
// a visual cell remains a TerrainCategory value from the enum above.
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

struct Terrain
{
    int ID;
    int TRV;
    int HEAL;
    int AVO;
    int DEF;
    bool PASSTHROUGH;
    std::string info;
};

struct TerrainOverride
{
    int ID;
    int TRV;
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
    int EquippedSlot;
};

struct WeaponAffinity
{
   std::vector<WeaponCategory> UsableWeapons;
};

struct Guild;

struct Entity
{
    std::string name;
    int entity_id;
    std::vector<int> location;
    std::vector<std::vector<int>> path;
    WeaponAffinity type;
    int Lvl;
    LevelExp Exp;
    Stats ogstats;
    Stats stats;
    Inventory inventory;
    EntityWeaponExp WExp;
    Growth growth;
    std::vector<TerrainOverride> terrain;
    Guild* group = nullptr;
};


struct Guild
{
    std::string name;
    int guild_id;
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
};
