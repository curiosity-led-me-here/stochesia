#include "entity_data.h"

namespace
{
Entity make_entity(
    const std::string& name,
    int level,
    const Stats& stats,
    const Growth& growth,
    std::vector<WeaponCategory> weapon_types
)
{
    Entity unit{};
    unit.name = name;
    unit.Lvl = level;
    unit.Exp = {0, 100};
    unit.stats = stats;
    unit.ogstats = stats;
    unit.growth = growth;
    unit.type.UsableWeapons = weapon_types;
    unit.inventory.EquippedSlot = -1;

    // Deliberately not set: entity_id, location, inventory, and group.
    return unit;
}
}

namespace entities
{
Entity seth()
{
    return make_entity("Seth", 1,
        {30, 14, 0, 13, 12, 13, 11, 8, 8, 11},
        {0.90, 0.50, 0.00, 0.45, 0.45, 0.25, 0.40, 0.30, 0.00, 0.00},
        {SWORD, LANCE});
}

Entity eirika()
{
    return make_entity("Eirika", 1,
        {16, 4, 0, 8, 9, 5, 3, 1, 5, 5},
        {0.70, 0.40, 0.00, 0.60, 0.60, 0.60, 0.30, 0.30, 0.00, 0.00},
        {SWORD});
}

Entity franz()
{
    return make_entity("Franz", 1,
        {20, 7, 0, 5, 7, 2, 6, 1, 7, 9},
        {0.80, 0.40, 0.00, 0.40, 0.50, 0.40, 0.25, 0.20, 0.00, 0.00},
        {SWORD, LANCE});
}

Entity gilliam()
{
    return make_entity("Gilliam", 4,
        {25, 9, 0, 6, 3, 3, 9, 3, 4, 14},
        {0.90, 0.45, 0.00, 0.35, 0.30, 0.30, 0.55, 0.20, 0.00, 0.00},
        {LANCE});
}

Entity vanessa()
{
    return make_entity("Vanessa", 1,
        {17, 5, 0, 7, 11, 4, 6, 5, 7, 5},
        {0.50, 0.35, 0.00, 0.55, 0.60, 0.50, 0.20, 0.30, 0.00, 0.00},
        {LANCE});
}

Entity neimi()
{
    return make_entity("Neimi", 1,
        {17, 4, 0, 5, 6, 4, 3, 2, 5, 5},
        {0.55, 0.45, 0.00, 0.50, 0.60, 0.50, 0.15, 0.35, 0.00, 0.00},
        {BOW});
}

Entity colm()
{
    return make_entity("Colm", 2,
        {18, 4, 0, 4, 10, 8, 3, 1, 6, 6},
        {0.75, 0.40, 0.00, 0.40, 0.65, 0.45, 0.25, 0.20, 0.00, 0.00},
        {SWORD});
}

Entity garcia()
{
    return make_entity("Garcia", 4,
        {28, 8, 0, 7, 7, 3, 5, 1, 5, 14},
        {0.80, 0.65, 0.00, 0.40, 0.20, 0.40, 0.25, 0.15, 0.00, 0.00},
        {AXE});
}

Entity lute()
{
    return make_entity("Lute", 1,
        {17, 0, 6, 6, 7, 8, 3, 5, 5, 3},
        {0.45, 0.00, 0.65, 0.30, 0.45, 0.45, 0.15, 0.40, 0.00, 0.00},
        {ANIMA});
}

Entity natasha()
{
    return make_entity("Natasha", 1,
        {18, 0, 3, 4, 8, 6, 2, 6, 5, 4},
        {0.50, 0.00, 0.60, 0.25, 0.40, 0.60, 0.15, 0.55, 0.00, 0.00},
        {STAFF});
}

Entity artur()
{
    return make_entity("Artur", 2,
        {19, 0, 6, 6, 8, 2, 2, 6, 5, 6},
        {0.55, 0.00, 0.50, 0.50, 0.40, 0.25, 0.15, 0.55, 0.00, 0.00},
        {LIGHT});
}

Entity joshua()
{
    return make_entity("Joshua", 5,
        {24, 8, 0, 13, 14, 7, 5, 2, 5, 8},
        {0.80, 0.35, 0.00, 0.55, 0.55, 0.30, 0.20, 0.20, 0.00, 0.00},
        {SWORD});
}

Entity soldier()
{
    return make_entity("Soldier", 1,
        {20, 3, 0, 0, 1, 0, 0, 0, 5, 6},
        {0.80, 0.50, 0.00, 0.30, 0.20, 0.25, 0.12, 0.15, 0.00, 0.00},
        {LANCE});
}
}
