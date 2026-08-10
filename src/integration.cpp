#include <iostream>
#include <stdexcept>
#include <string>
#include "pathfinder.h"
#include "game_data.h"
#include "mechanics_ascii.h"
#include "mechanics.h"
#include <stdexcept>
#include <cassert>
#include "maps.h"



int main()
{
    Mapmaker map({30,30}, 2);
    map.add_random_obstacles(30, 30);
    Entity seth{};

    seth.name = "Seth";
    seth.entity_id = 1;
    seth.location = {6, 6};

    seth.type.UsableWeapons = {SWORD, LANCE};

    seth.Lvl = 1;
    seth.Exp = {0, 100};

    seth.stats   = {30, 14, 0, 13, 12, 8, 11, 8, 8, 13};
    seth.ogstats = seth.stats;

    seth.inventory.slot[0] = {IRON_LANCE, 45};
    seth.inventory.slot[1] = {IRON_SWORD, 46};
    seth.inventory.slot[2] = {VULNERARY, 3};
    seth.inventory.EquippedSlot = 1;
    
    Entity soldier{};

    soldier.name = "Soldier";
    soldier.entity_id = 2;
    soldier.location = {6, 7};

    soldier.type.UsableWeapons = {LANCE};

    soldier.Lvl = 1;
    soldier.Exp = {0, 100};

    soldier.stats   = {20, 5, 0, 0, 0, 0, 3, 0, 4, 8};
    soldier.ogstats = soldier.stats;

    soldier.inventory.slot[0] = {IRON_LANCE, 45};
    soldier.inventory.EquippedSlot = 0;


    Guild A{};
    A.name = "Seth's guild";
    A.guild_id = 1;
    A.add(seth);

    Guild B{};
    B.name = "Soldier's guild";
    B.guild_id = 2;
    B.add(soldier);
    
    map.place_unit(seth);
    map.place_unit(soldier);

    auto attempt_move = [&map](Entity& unit, std::vector<int> destination)
    {
        try
        {
            map.move(unit, destination);
        }
        catch (const std::invalid_argument& error)
        {
            std::cout << unit.name << " cannot move there: "
                      << error.what() << '\n';
        }
    };

    plot_points(map.get_map(), 0, 32, 0, 32, {0,0});
    std::cout << '\n';
    std::cout << '\n';
    map.path_trace(seth);
    plot_state(seth.path, 0, 32, 0, 32, seth.location);
    std::cout << '\n';
    std::cout << '\n';
    //attempt_move(seth, {3, 4});
    std::cout << '\n';
    std::cout << '\n';
    //map.path_trace(soldier);
    //plot_state(soldier.path, 0, 32, 0, 32, soldier.location);
    std::cout << '\n';
    std::cout << '\n';
    //attempt_move(soldier, {-2, 1});
    std::cout << '\n';
    std::cout << '\n';
    //map.path_trace(seth);
    //plot_state(seth.path, 0, 32, 0, 32, seth.location);
    std::cout << '\n';
    std::cout << '\n';
    //attempt_move(seth, {2, 5});

    auto forecast = interact(seth, soldier);

    //mechanics_ascii::interact_window(seth, soldier, forecast[0], forecast[1]);

    battle(seth, soldier);
    //mechanics_ascii::battle_window(seth, soldier);
    
    return 0;
}
