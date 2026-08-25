#include "map_ascii.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

#include "entity_registry.h"
#include "game_data.h"
#include "pathfinder.h"
using namespace std;

void plot_points(const vector<vector<int>>& terrain_map, int min_x, int max_x, int min_y, int max_y, const vector<int>& start)
{
    for (int y = min(max_y, static_cast<int>(terrain_map.size()) - 1); y >= max(min_y, 0); --y)
    {
        for (int x = max(min_x, 0); x <= min(max_x, static_cast<int>(terrain_map[0].size()) - 1); ++x)
        {
            if (x == start[0] && y == start[1]) cout << "S ";
            else if (terrain_map[y][x] == TERRAIN_FOREST) cout << "W ";
            else if (terrain_map[y][x] == TERRAIN_MOUNTAIN) cout << "^ ";
            else if (terrain::blocks_common_foot(terrain_map[y][x])) cout << "# ";
            else cout << ". ";
        }
        cout << '\n';
    }
}

void plot_state(const Entity& unit, int min_x, int max_x, int min_y, int max_y)
{
    const vector<vector<int>>& movement = unit.path;
    const vector<vector<int>>& attack = unit.attack_range;

    if (movement.empty() || movement[0].empty())
        return;

    for (int y = min(max_y, static_cast<int>(movement.size()) - 1); y >= max(min_y, 0); --y)
    {
        for (int x = max(min_x, 0); x <= min(max_x, static_cast<int>(movement[0].size()) - 1); ++x)
        {
            if (unit.location.size() >= 2 && x == unit.location[0] && y == unit.location[1])
                cout << "S ";
            else if (movement[y][x] >= 0)
                cout << "O ";
            else if (y < attack.size() && x < attack[y].size() && attack[y][x] > 0)
                cout << "X ";
            else
                cout << "- ";
        }
        cout << '\n';
    }
}

void plot_travel_history(const vector<vector<int>>& route, int current_index, int width, int height)
{
    for (int y = height - 1; y >= 0; --y)
    {
        for (int x = 0; x < width; ++x)
        {
            char marker = '-';
            for (int i = static_cast<int>(route.size()) - 1; i > current_index; --i)
            {
                const vector<int>& from = route[i];
                const vector<int>& to = route[i - 1];
                if (x != from[0] || y != from[1]) continue;
                if (to[0] > from[0]) marker = '>';
                if (to[0] < from[0]) marker = '<';
                if (to[1] > from[1]) marker = '^';
                if (to[1] < from[1]) marker = 'v';
            }
            if (!route.empty() && x == route[current_index][0] && y == route[current_index][1]) marker = 'o';
            cout << marker << ' ';
        }
        cout << '\n';
    }
    cout << '\n';
}

string unit_icon(const Entity& unit)
{
    const vector<WeaponCategory>& weapons = unit.type.UsableWeapons;

    auto has = [&](WeaponCategory category)
    {
        return find(weapons.begin(), weapons.end(), category) != weapons.end();
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
                cout << unit_icon(registry.get_unit(entity_id)) << " ";
                continue;
            }

            int terrain_id = map[y][x];

            if (terrain_id == TERRAIN_FOREST) cout << "W  ";
            else if (terrain_id == TERRAIN_ROAD) cout << "=  ";
            else if (terrain_id == TERRAIN_RIVER) cout << "~  ";
            else if (terrain_id == TERRAIN_MOUNTAIN) cout << "^  ";
            else if (terrain::blocks_common_foot(terrain_id)) cout << "#  ";
            else cout << ".  ";
        }
        cout << '\n';
    }
}

void Mapmaker::plot_movement_frame(Registry& registry, const Entity& moving_unit, int delay_ms)
{
    cout << "\x1B[2J\x1B[H";

    for (int y = map.size() - 1; y >= 0; y--)
    {
        for (int x = 0; x < map[0].size(); x++)
        {
            if (moving_unit.location.size() >= 2 &&
                x == moving_unit.location[0] &&
                y == moving_unit.location[1])
            {
                cout << unit_icon(moving_unit) << " ";
                continue;
            }

            int entity_id = occupancy[y][x];
            if (entity_id != 0)
            {
                cout << unit_icon(registry.get_unit(entity_id)) << " ";
                continue;
            }

            int terrain_id = map[y][x];
            if (terrain_id == TERRAIN_FOREST) cout << "W  ";
            else if (terrain_id == TERRAIN_ROAD) cout << "=  ";
            else if (terrain_id == TERRAIN_RIVER) cout << "~  ";
            else if (terrain_id == TERRAIN_MOUNTAIN) cout << "^  ";
            else if (terrain::blocks_common_foot(terrain_id)) cout << "#  ";
            else cout << ".  ";
        }
        cout << '\n';
    }

    cout << flush;
    this_thread::sleep_for(chrono::milliseconds(delay_ms));
}

void print_unit_stats(const Entity& unit)
{
    cout << "+----------------------------------------+\n";
    cout << "| " << unit.name << "  Lv " << unit.Lvl
              << "  HP " << unit.stats.HP << '/' << unit.ogstats.HP << '\n';
    cout << "| ID: " << unit.entity_id
              << "  Guild: " << (unit.group ? unit.group->name : "None") << '\n';

    if (unit.location.size() >= 2)
        cout << "| Position: (" << unit.location[0] << ", " << unit.location[1] << ")\n";

    cout << "| STR " << unit.stats.STR << "  MAG " << unit.stats.MAG
              << "  SKL " << unit.stats.SKL << "  SPD " << unit.stats.SPD << '\n';
    cout << "| LUC " << unit.stats.LUC << "  DEF " << unit.stats.DEF
              << "  RES " << unit.stats.RES << "  MOV " << unit.stats.MOV
              << "  CON " << unit.stats.CON << '\n';
    cout << "+----------------------------------------+\n";
}

void print_guild_status(const Guild& guild)
{
    cout << "+------------------------------------------------------------+\n";
    cout << "| " << guild.name << "  [Guild ID: " << guild.guild_id << "]\n";
    cout << "+------+------------------+----------+-------+-------+---------+\n";
    cout << "| ID   | Name             | HP       | Turn  | Alive | Pos     |\n";
    cout << "+------+------------------+----------+-------+-------+---------+\n";

    for (const Entity* unit : guild.members)
    {
        if (unit == nullptr) continue;

        string hp = to_string(unit->stats.HP) + "/" +
                         to_string(unit->ogstats.HP);
        string position = "-";
        if (unit->location.size() >= 2)
        {
            position = "(" + to_string(unit->location[0]) + "," +
                       to_string(unit->location[1]) + ")";
        }

        cout << "| " << left << setw(4) << unit->entity_id
                  << " | " << setw(16) << unit->name
                  << " | " << setw(8) << hp
                  << " | " << setw(5) << (unit->turn ? "READY" : "DONE")
                  << " | " << setw(5) << (unit->alive ? "YES" : "NO")
                  << " | " << setw(7) << position << " |\n";
    }

    cout << "+------+------------------+----------+-------+-------+---------+\n";
}

void print_attack_prompts(
    const vector<avl_for_atk>& prompts,
    const vector<vector<int>>& occupancy,
    Registry& registry)
{
    if (prompts.empty())
    {
        cout << "No enemies can be attacked.\n";
        return;
    }

    vector<vector<int>> already_printed;
    int target_number = 1;

    for (const avl_for_atk& prompt : prompts)
    {
        if (prompt.coords.size() != 2) continue;

        bool duplicate = false;
        for (const vector<int>& coords : already_printed)
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

        cout << "[" << target_number++ << "] "
                  << enemy.name << " at (" << x << ", " << y << ")\n";
        cout << "HP:  " << enemy.stats.HP << "/" << enemy.ogstats.HP << '\n';
        cout << "STR: " << enemy.stats.STR
                  << "  MAG: " << enemy.stats.MAG
                  << "  SKL: " << enemy.stats.SKL
                  << "  SPD: " << enemy.stats.SPD << '\n';
        cout << "LUC: " << enemy.stats.LUC
                  << "  DEF: " << enemy.stats.DEF
                  << "  RES: " << enemy.stats.RES
                  << "  MOV: " << enemy.stats.MOV << '\n';

        cout << "Eligible weapons:\n";
        vector<int> shown_inventory_slots;

        for (const avl_for_atk& option : prompts)
        {
            if (option.coords != prompt.coords) continue;

            int inventory_slot = option.inventory_id;
            if (find(shown_inventory_slots.begin(), shown_inventory_slots.end(), inventory_slot) != shown_inventory_slots.end())
            {
                continue;
            }
            shown_inventory_slots.push_back(inventory_slot);

            cout << "  [Slot " << option.inventory_id << "] "
                      << option.weapon.NAME
                      << "  MT " << option.weapon.MT
                      << "  HIT " << option.weapon.HIT
                      << "  CRIT " << option.weapon.CRIT
                      << "  Range " << option.weapon.MINRG
                      << "-" << option.weapon.MAXRG << '\n';
        }

        cout << '\n';
    }
}
