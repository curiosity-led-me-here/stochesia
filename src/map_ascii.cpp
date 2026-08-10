#include "map_ascii.h"

#include <algorithm>
#include <iostream>

#include "entity_registry.h"
#include "game_data.h"
#include "pathfinder.h"

void plot_points(const std::vector<std::vector<int>>& terrain_map, int min_x, int max_x, int min_y, int max_y, const std::vector<int>& start)
{
    for (int y = std::min(max_y, static_cast<int>(terrain_map.size()) - 1); y >= std::max(min_y, 0); --y)
    {
        for (int x = std::max(min_x, 0); x <= std::min(max_x, static_cast<int>(terrain_map[0].size()) - 1); ++x)
        {
            if (x == start[0] && y == start[1]) std::cout << "S ";
            else if (!findbyid(base_topo, terrain_map[y][x]).PASSTHROUGH) std::cout << "# ";
            else if (terrain_map[y][x] == 2) std::cout << "W ";
            else std::cout << ". ";
        }
        std::cout << '\n';
    }
}

void plot_state(const Entity& unit, int min_x, int max_x, int min_y, int max_y)
{
    const std::vector<std::vector<int>>& movement = unit.path;
    const std::vector<std::vector<int>>& attack = unit.attack_range;

    if (movement.empty() || movement[0].empty())
        return;

    for (int y = std::min(max_y, static_cast<int>(movement.size()) - 1); y >= std::max(min_y, 0); --y)
    {
        for (int x = std::max(min_x, 0); x <= std::min(max_x, static_cast<int>(movement[0].size()) - 1); ++x)
        {
            if (unit.location.size() >= 2 && x == unit.location[0] && y == unit.location[1])
                std::cout << "S ";
            else if (movement[y][x] >= 0)
                std::cout << "O ";
            else if (y < attack.size() && x < attack[y].size() && attack[y][x] > 0)
                std::cout << "X ";
            else
                std::cout << "- ";
        }
        std::cout << '\n';
    }
}

void plot_travel_history(const std::vector<std::vector<int>>& route, int current_index, int width, int height)
{
    for (int y = height - 1; y >= 0; --y)
    {
        for (int x = 0; x < width; ++x)
        {
            char marker = '-';
            for (int i = static_cast<int>(route.size()) - 1; i > current_index; --i)
            {
                const std::vector<int>& from = route[i];
                const std::vector<int>& to = route[i - 1];
                if (x != from[0] || y != from[1]) continue;
                if (to[0] > from[0]) marker = '>';
                if (to[0] < from[0]) marker = '<';
                if (to[1] > from[1]) marker = '^';
                if (to[1] < from[1]) marker = 'v';
            }
            if (!route.empty() && x == route[current_index][0] && y == route[current_index][1]) marker = 'o';
            std::cout << marker << ' ';
        }
        std::cout << '\n';
    }
    std::cout << '\n';
}

std::string unit_icon(const Entity& unit)
{
    const std::vector<WeaponCategory>& weapons = unit.type.UsableWeapons;

    auto has = [&](WeaponCategory category)
    {
        return std::find(weapons.begin(), weapons.end(), category) != weapons.end();
    };

    if (unit.stats.MOV >= 8 && has(SWORD) && has(LANCE)) return "🐴";
    if (has(STAFF)) return "🩺";
    if (has(ANIMA) || has(LIGHT) || has(DARK)) return "🧙";
    if (has(BOW)) return "🏹";
    if (has(AXE)) return "🪓";
    if (has(LANCE)) return "🛡️";
    if (has(SWORD)) return "⚔️";

    return "❔";
}

void Mapmaker::plot_with_units(Registry& registry)
{
    for (int y = map.size() - 1; y >= 0; y--)
    {
        for (int x = 0; x < map[0].size(); x++)
        {
            int entity_id = occupancy[y][x];

            if (entity_id != 0)
            {
                std::cout << unit_icon(registry.get_unit(entity_id)) << " ";
                continue;
            }

            int terrain_id = map[y][x];

            if (!findbyid(base_topo, terrain_id).PASSTHROUGH) std::cout << "#  ";
            else if (terrain_id == 2) std::cout << "W  ";
            else if (terrain_id == 1) std::cout << "=  ";
            else if (terrain_id == 5) std::cout << "~  ";
            else std::cout << ".  ";
        }
        std::cout << '\n';
    }
}

void print_unit_stats(const Entity& unit)
{
    std::cout << "+----------------------------------------+\n";
    std::cout << "| " << unit.name << "  Lv " << unit.Lvl
              << "  HP " << unit.stats.HP << '/' << unit.ogstats.HP << '\n';
    std::cout << "| ID: " << unit.entity_id
              << "  Guild: " << (unit.group ? unit.group->name : "None") << '\n';

    if (unit.location.size() >= 2)
        std::cout << "| Position: (" << unit.location[0] << ", " << unit.location[1] << ")\n";

    std::cout << "| STR " << unit.stats.STR << "  MAG " << unit.stats.MAG
              << "  SKL " << unit.stats.SKL << "  SPD " << unit.stats.SPD << '\n';
    std::cout << "| LUC " << unit.stats.LUC << "  DEF " << unit.stats.DEF
              << "  RES " << unit.stats.RES << "  MOV " << unit.stats.MOV
              << "  CON " << unit.stats.CON << '\n';
    std::cout << "+----------------------------------------+\n";
}

void print_attack_prompts(
    const std::vector<avl_for_atk>& prompts,
    const std::vector<std::vector<int>>& occupancy,
    Registry& registry)
{
    if (prompts.empty())
    {
        std::cout << "No enemies can be attacked.\n";
        return;
    }

    std::vector<std::vector<int>> already_printed;
    int target_number = 1;

    for (const avl_for_atk& prompt : prompts)
    {
        if (prompt.coords.size() != 2) continue;

        bool duplicate = false;
        for (const std::vector<int>& coords : already_printed)
        {
            if (coords == prompt.coords)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        int x = prompt.coords[0];
        int y = prompt.coords[1];

        if (y < 0 || y >= occupancy.size() ||
            x < 0 || x >= occupancy[y].size() ||
            occupancy[y][x] == 0)
        {
            continue;
        }

        Entity& enemy = registry.get_unit(occupancy[y][x]);
        already_printed.push_back(prompt.coords);

        std::cout << "[" << target_number++ << "] "
                  << enemy.name << " at (" << x << ", " << y << ")\n";
        std::cout << "HP:  " << enemy.stats.HP << "/" << enemy.ogstats.HP << '\n';
        std::cout << "STR: " << enemy.stats.STR
                  << "  MAG: " << enemy.stats.MAG
                  << "  SKL: " << enemy.stats.SKL
                  << "  SPD: " << enemy.stats.SPD << '\n';
        std::cout << "LUC: " << enemy.stats.LUC
                  << "  DEF: " << enemy.stats.DEF
                  << "  RES: " << enemy.stats.RES
                  << "  MOV: " << enemy.stats.MOV << '\n';

        std::cout << "Eligible weapons:\n";
        std::vector<int> shown_weapon_ids;

        for (const avl_for_atk& option : prompts)
        {
            if (option.coords != prompt.coords) continue;

            int weapon_id = option.weapon.ID;
            if (std::find(shown_weapon_ids.begin(), shown_weapon_ids.end(), weapon_id) != shown_weapon_ids.end())
            {
                continue;
            }
            shown_weapon_ids.push_back(weapon_id);

            std::cout << "  [ID " << option.weapon.ID << "] "
                      << option.weapon.NAME
                      << "  MT " << option.weapon.MT
                      << "  HIT " << option.weapon.HIT
                      << "  CRIT " << option.weapon.CRIT
                      << "  Range " << option.weapon.MINRG
                      << "-" << option.weapon.MAXRG << '\n';
        }

        std::cout << '\n';
    }
}
