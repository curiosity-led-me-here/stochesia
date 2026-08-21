#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>
#include <random>
#include "game_data.h"
#include <algorithm>
#include <utility>
#include "pathfinder.h"
#include "maps.h"
#include "entity_registry.h"
#include "general_pathtracing.h"
#include "entity_registry.h"
#include "map_ascii.h"
#include "mechanics.h"
#include "mechanics_ascii.h"
#include "mechanics.h"

std::vector<std::vector<int>> Mapmaker::generate_map(const std::vector<int>& dimensions)
{
    return std::vector<std::vector<int>>(dimensions[0], std::vector<int>(dimensions[1], 0));
}

std::vector<std::vector<int>> Mapmaker::load_map(const maps::TerrainMap& recipe)
{
    if (recipe.empty() || recipe[0].empty())
    {
        throw std::invalid_argument("Map recipe is empty.");
    }
    map = recipe;
    dimensions = {
        static_cast<int>(map.size()),
        static_cast<int>(map[0].size())
    };
    occupancy = generate_map(dimensions);
    guilds = generate_map(dimensions);
    return map;
}

std::vector<int>& Mapmaker::get_dimensions()
{
    return dimensions;
}

std::vector<std::vector<int>>& Mapmaker::get_occ()
{
    return occupancy;
}

std::vector<std::vector<int>>& Mapmaker::get_guilds()
{
    return guilds;
}


void Mapmaker::pathtrace(
    std::vector<int> current_coord,
    int budget,
    std::vector<std::vector<int>>& state,
    int guild_id,
    terrain::MovementType movement)
{
    for (std::vector<int> i : helper)
    {
        std::vector<int> next_coord = { current_coord[0] + i[0], current_coord[1] + i[1] };
        int x = next_coord[0];
        int y = next_coord[1];
        if (x < 0 || y < 0 || x >= state[0].size() || y >= state.size() || !terrain::can_enter(map[y][x], movement) || guilds[y][x] != 0 && guilds[y][x] != guild_id) continue;
        int penalty = terrain::movement_cost(map[y][x], movement);
        int rem_budget = budget - penalty;
        if (rem_budget < 0) continue;
        if (state[y][x] < rem_budget) state[y][x] = rem_budget;
        else continue;
        pathtrace(next_coord, rem_budget, state, guild_id, movement);
    }
}

void Mapmaker::death(Entity& X)
{
    if (X.group == nullptr)
    {
        throw std::runtime_error("death(): entity has no guild");
    }

    X.group->remove(X);
    X.alive = false;
    int x = X.location[0];
    int y = X.location[1];
    occupancy[y][x] = 0;
    guilds[y][x] = 0;
}

void Mapmaker::move(Entity& unit, std::vector<int>& coord, std::vector<std::vector<int>>& out)
{
    std::vector<std::vector<int>> state = unit.path;
    if (state[coord[1]][coord[0]] == -1) throw std::invalid_argument("Unreachable tile");
    for (std::vector<int> i : std::vector<std::vector<int>>({{0, 1}, {0, -1}, {1, 0}, {-1, 0}}))
    {
        std::vector<int> next_coord = {coord[0] + i[0], coord[1] + i[1]};
        int old_x = coord[0], old_y = coord[1], new_x = next_coord[0], new_y = next_coord[1];
        if (new_x >= map[0].size() || new_y >= map.size() || new_x < 0 || new_y < 0) continue;
        if (state[new_y][new_x] != state[old_y][old_x] + terrain::movement_cost(map[old_y][old_x], unit.movement)) continue;
        out.push_back(next_coord);
        move(unit, next_coord, out);
        return;
    }
}


Mapmaker::Mapmaker(const std::vector<int>& dimensions)
    : map(generate_map(dimensions)), dimensions(dimensions), occupancy(generate_map(dimensions)), guilds(generate_map(dimensions))
{
    for (std::vector<int>& row : map)
    {
        std::fill(row.begin(), row.end(), TERRAIN_PLAINS);
    }
}
Mapmaker::Mapmaker(maps::TerrainMap recipe) : map(std::move(recipe))
{
    if (map.empty() || map[0].empty()) { throw std::invalid_argument("Map recipe is empty"); }
    dimensions = {static_cast<int>(map.size()), static_cast<int>(map[0].size())};
    occupancy = generate_map(dimensions);
    guilds = generate_map(dimensions); }

Mapmaker::Mapmaker(const maps::MapRecipe& recipe)
    : Mapmaker(recipe.terrain)
{}

int Mapmaker::entity_at(std::vector<int> coordinates)
{
    return occupancy[coordinates[1]][coordinates[0]];
}

std::vector<std::vector<int>> Mapmaker::get_generate() { return generate_map(dimensions); }
std::vector<std::vector<int>> Mapmaker::get_map() { return map; }

void Mapmaker::place_unit(Entity& unit)
{
    std::vector<int> coords = unit.location;
    if (occupancy[coords[1]][coords[0]] == 0)
    {
        occupancy[coords[1]][coords[0]] = unit.entity_id;
	if (unit.group == nullptr)
	{
	    guilds[coords[1]][coords[0]] = -1;
	}
	else
	{
	    guilds[coords[1]][coords[0]] = unit.group->guild_id;
	}
        unit.location = coords;
	unit.terrain_id = map[unit.location[1]][unit.location[0]];
        unit.path = generate_map(dimensions);
    }
    else throw std::invalid_argument("Unit already placed here!");
}

void Mapmaker::add_random_obstacles(int n, int m)
{
    std::vector<int> obstacle_ids = terrain::default_obstacle_ids();
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> x_dist(0, static_cast<int>(map[0].size()) - 1);
    std::uniform_int_distribution<int> y_dist(0, static_cast<int>(map.size()) - 1);
    std::uniform_int_distribution<int> obstacle_dist(0, static_cast<int>(obstacle_ids.size()) - 1);
    int placed = 0;
    while (placed < n)
    {
        int x = x_dist(rng), y = y_dist(rng);
        if (map[y][x] == TERRAIN_PLAINS) { map[y][x] = obstacle_ids[obstacle_dist(rng)]; ++placed; }
    }
    placed = 0;
    while (placed < m)
    {
        int x = x_dist(rng), y = y_dist(rng);
        if (map[y][x] == TERRAIN_PLAINS) { map[y][x] = TERRAIN_FOREST; ++placed; }
    }
}

void Mapmaker::path_trace(Entity& unit)
{
    for (std::vector<int>& row : unit.path) std::fill(row.begin(), row.end(), -1);
    unit.path[unit.location[1]][unit.location[0]] = unit.stats.MOV;
    pathtrace(unit.location, unit.stats.MOV, unit.path, unit.group->guild_id, unit.movement);
}

std::vector<std::vector<int>> Mapmaker::consider_occupancy(const Entity& unit)
{
    std::vector<std::vector<int>> landable = unit.path;
    for (int y = 0; y < landable.size(); y++)
    {
        for (int x = 0; x < landable[0].size(); x++)
        {
            if (occupancy[y][x] != 0)
            {
                landable[y][x] = -1;
            }
        }
    }
    return landable;
}

int count_inventory_slots(const Entity& unit)
{
    int out = 0;
    for (ItemStack i : unit.inventory.slot)
    {
	out++;
    }
    return out;
}

std::vector<avl_for_atk> Mapmaker::prompt_attack(Entity& unit)
{
    std::vector<avl_for_atk> out_meta;
    Guild guild = *unit.group;
    int team = guild.guild_id;
    for (int i=0; i < count_inventory_slots(unit); i++)
    {
	std::vector<std::vector<int>> out= unit.path;
	for (std::vector<int>& row : out)
	{
	    std::fill(row.begin(), row.end(), -1);
	}
	std::vector<std::vector<int>> out_min= out;
	try
	{
	    Weapon weapon = get_weapon(Armory, unit.inventory.slot[i].ID);
	    trace(out_min, out, unit.location, weapon.MINRG, weapon.MAXRG);
	    for (int p=0; p < unit.path.size(); p++)
	    {
		for (int q=0; q < unit.path[0].size(); q++)
		{
		    if (guilds[p][q] != 0 && guilds[p][q] != team && out[p][q] != 0)
		    {
			out_meta.push_back({{q, p}, weapon, i});
		    }
		}
	    }
	}
	catch (const std::invalid_argument& e) {continue;}
    }
    if (out_meta.empty())
    {
	return {};
    }
    else
    {
	return out_meta;
    }
}

void Mapmaker::update_attack_range(Entity& unit)
{
    Guild guild = *unit.group;
    int team = guild.guild_id;
    std::vector<std::vector<int>> accumulator = unit.path;
    for (std::vector<int>& row : accumulator)
    {
	std::fill(row.begin(), row.end(), 0);
    }
    for (int i=0; i < count_inventory_slots(unit); i++)
    {
	std::vector<std::vector<int>> out= unit.path;
	for (std::vector<int>& row : out)
	{
	    std::fill(row.begin(), row.end(), -1);
	}
	std::vector<std::vector<int>> out_min= out;
	try
	{
	    Weapon weapon = get_weapon(Armory, unit.inventory.slot[i].ID);
	    trace(out_min, out, unit.location, weapon.MINRG, weapon.MAXRG);
	    for (int i=0; i < out.size(); i++)
	    {
		for (int j=0; j < out[0].size(); j++)
		{
		    accumulator[i][j] = accumulator[i][j] || out[i][j];
		}
	    }
	}
	catch (const std::invalid_argument& e) {continue;}
    }
    unit.attack_range = accumulator;
}

std::vector<std::vector<int>> Mapmaker::render_move(Entity& unit, std::vector<int> delta_coord)
{
    std::vector<int> coord = {unit.location[0]+delta_coord[0], unit.location[1]+delta_coord[1]};
    if (coord[0] < 0 || coord[1] < 0 || coord[0] >= map[0].size() || coord[1] >= map.size())
    {
	throw std::invalid_argument("Destination out of the map!");
    }
    auto landable = consider_occupancy(unit);
    if (landable[coord[1]][coord[0]] == -1)
    {
	throw std::invalid_argument("Destination obstructed by another unit!");
    }
    guilds[unit.location[1]][unit.location[0]] = 0;
    occupancy[unit.location[1]][unit.location[0]] = 0;
    std::vector<std::vector<int>> out;
    out.push_back(coord);
    move(unit, coord, out);
    std::vector<std::vector<int>> output;
    for (int i = static_cast<int>(out.size()) - 1; i >= 0; --i)
    {
	output.push_back(out[i]);
	unit.location = out[i];
	unit.terrain_id = map[unit.location[1]][unit.location[0]];
	unit.turn = false;
    }
    path_trace(unit);
    guilds[unit.location[1]][unit.location[0]] = unit.group->guild_id;
    occupancy[unit.location[1]][unit.location[0]] = unit.entity_id;
    return output;
}


bool check_valid_coords(std::vector<int> coords, std::vector<avl_for_atk> prompts)
{
    bool valid = false;
    for (avl_for_atk prompt : prompts)
    {
	if (prompt.coords[0] == coords[0] && prompt.coords[1] == coords[1] && prompt.inventory_id == coords[2])
	{
	    valid = true;
	}
    }
    return valid;
}

std::vector<int> retry(const std::vector<avl_for_atk> prompts)
{
    while (true)
    {
        std::cout << "\n attack_cmd <x, y, slot_id> > ";
        int x, y, z;
        char comma;
        std::cin >> x >> comma >> y >> comma >> z;
        if (check_valid_coords({x, y, z}, prompts)) { return {x, y, z}; }
        std::cout << "Invalid Coords. Check the prompt list and choose one from it.\n";
    }
}

std::vector<std::vector<int>> Mapmaker::move(Entity& unit, std::vector<int> delta_coord)
{
    std::vector<int> coord = {unit.location[0]+delta_coord[0], unit.location[1]+delta_coord[1]};
    if (coord[0] < 0 || coord[1] < 0 || coord[0] >= map[0].size() || coord[1] >= map.size())
    {
	throw std::invalid_argument("Destination out of the map!");
    }
    auto landable = consider_occupancy(unit);
    if (landable[coord[1]][coord[0]] == -1)
    {
	throw std::invalid_argument("Destination obstructed by another unit!");
    }
    guilds[unit.location[1]][unit.location[0]] = 0;
    occupancy[unit.location[1]][unit.location[0]] = 0;
    std::vector<std::vector<int>> out;
    out.push_back(coord);
    move(unit, coord, out);
    std::vector<std::vector<int>> output;
    for (int i = static_cast<int>(out.size()) - 1; i >= 0; --i)
    {
	output.push_back(out[i]);
	unit.location = out[i];
	unit.terrain_id = map[unit.location[1]][unit.location[0]];
    }
    path_trace(unit);
    guilds[unit.location[1]][unit.location[0]] = unit.group->guild_id;
    occupancy[unit.location[1]][unit.location[0]] = unit.entity_id;
    unit.turn = false;
    return output;
    
}


bool check_edge(std::vector<int> coord, std::vector<std::vector<int>> path)
{
    if (path.empty() || path[0].empty())
    {
        return false;
    }

    for (std::vector<int> k : helper)
    {
        int new_x = coord[0] + k[0];
        int new_y = coord[1] + k[1];

        if (new_x < 0 || new_y < 0 ||
            new_y >= path.size() ||
            new_x >= path[new_y].size())
        {
            continue;
        }

        if (path[new_y][new_x] == -1)
        {
            return true;
        }
    }
    return false;
}

std::vector<std::vector<int>> select_helper(std::vector<int> coord, std::vector<int> location)
{
    if (coord[0] < location[0] && coord[1] < location[1])
    {
	// upper left
	return {{0, -1}, {-1, 0}};
	
    }
    else if (coord[0] < location[0] && coord[1] > location[1])
    {
	// upper right
	return {{0, 1}, {-1, 0}};
    }
    else if (coord[0] > location[0] && coord[1] < location[1])
    {
	// lower left
	return {{0, -1}, {1, 0}};
    }
    else if (coord[0] > location[0] && coord[1] > location[1])
    {
	// lower right
	return {{0, 1}, {1, 0}};
    }
    else
    {
	// mid
	return {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    }
}

std::vector<std::vector<int>> Mapmaker::standing_attack_range(const Entity& unit, const Weapon& weapon)
{
    std::vector<std::vector<int>> attack_path = generate_map(dimensions);
    for (std::vector<int>& row : attack_path)
    {
        std::fill(row.begin(), row.end(), -1);
    }

    std::vector<std::vector<int>> inner_range = attack_path;
    trace(inner_range, attack_path, unit.location, weapon.MINRG, weapon.MAXRG);
    return attack_path;
}

std::vector<std::vector<int>> Mapmaker::attack_range(const Entity& unit, const Weapon& weapon)
{
    std::vector<std::vector<int>> attack_path = unit.path;
    for (std::vector<int>& row : attack_path)
    {
	std::fill(row.begin(), row.end(), 0);
    }

    std::vector<std::vector<int>> origins = consider_occupancy(unit);
    origins[unit.location[1]][unit.location[0]] = unit.path[unit.location[1]][unit.location[0]];

    for (int y=0; y < attack_path.size(); y++)
    {
	for (int x=0; x < attack_path[0].size(); x++)
	{
	    if (origins[y][x] >= 0)
	    {
		attack_path[y][x] = 1;
		if (check_edge({x,y}, unit.path))
		{
		    std::vector<std::vector<int>> helper = select_helper({x,y}, unit.location);
		    for (int n=weapon.MINRG; n < weapon.MAXRG+1; n++)
		    {
			for (std::vector<int> k : helper)
			{
			    int new_x = x+(k[0]*n);
			    int new_y = y+(k[1]*n);
			    if (new_x < attack_path[0].size() && new_y < attack_path.size() && new_x >= 0 && new_y >= 0)
			    {
				attack_path[new_y][new_x] = 1;
			    }
			}
		    }
		}
	    }
	}
    }
    return attack_path;
}

std::vector<std::vector<int>> superimpose(const std::vector<std::vector<std::vector<int>>>& out)
{
    if (out.empty())
    {
        return {};
    }
    
    std::vector<std::vector<int>> output = out[0];

    for (int i = 1; i < out.size(); i++)
    {
        for (int j = 0; j < out[i].size(); j++)
        {
            for (int k = 0; k < out[i][j].size(); k++)
            {
                output[j][k] = std::max(output[j][k], out[i][j][k]);
            }
        }
    }
    return output;
}


void Mapmaker::attack_range(Entity& unit)
{
    std::vector<std::vector<std::vector<int>>> out;
    for (ItemStack i : unit.inventory.slot)
    {
	try
	{
	    Weapon weapon = get_weapon(Armory, i.ID);
	    for (WeaponCategory i : unit.type.UsableWeapons)
	    {
		if (i == weapon.CAT)
		{
		    std::vector<std::vector<int>> path = attack_range(unit, weapon);
		    out.push_back(path);
		}
	    }
	}   
	catch (const std::invalid_argument& e) {}
    }
    if (out.empty()) { unit.attack_range = unit.path; }
    else { unit.attack_range  = superimpose(out); }
}
