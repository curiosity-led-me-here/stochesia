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

void plot_state(const std::vector<std::vector<int>>& state, int min_x, int max_x, int min_y, int max_y, const std::vector<int>& start)
{
    for (int y = std::min(max_y, static_cast<int>(state.size()) - 1); y >= std::max(min_y, 0); --y)
    {
        for (int x = std::max(min_x, 0); x <= std::min(max_x, static_cast<int>(state[0].size()) - 1); ++x)
        {
            if (x == start[0] && y == start[1]) std::cout << "S ";
            else if (state[y][x] == -1) std::cout << "- ";
            else std::cout << state[y][x] << ' ';
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


std::vector<std::vector<int>> Mapmaker::generate_map(const std::vector<int>& dimensions)
{
    return std::vector<std::vector<int>>(dimensions[0], std::vector<int>(dimensions[1], 0));
}

void Mapmaker::pathtrace(std::vector<int> current_coord, int budget, std::vector<std::vector<int>>& state)
{
    for (std::vector<int> i : std::vector<std::vector<int>>({{0, 1}, {0, -1}, {1, 0}, {-1, 0}}))
    {
        std::vector<int> next_coord = { current_coord[0] + i[0], current_coord[1] + i[1] };
        int x = next_coord[0];
        int y = next_coord[1];
        if (x < 0 || y < 0 || x >= state[0].size() || y >= state.size() || !findbyid(base_topo, map[y][x]).PASSTHROUGH || occupancy[y][x] != 0) continue;
        int penalty = findbyid(base_topo, map[y][x]).TRV;
        int rem_budget = budget - penalty;
        if (rem_budget < 0) continue;
        if (state[y][x] < rem_budget) state[y][x] = rem_budget;
        else continue;
        pathtrace(next_coord, rem_budget, state);
    }
}

void Mapmaker::move(Entity& unit, std::vector<int>& coord, std::vector<std::vector<int>>& out)
{
    std::vector<std::vector<int>>& state = unit.path;
    if (state[coord[1]][coord[0]] == -1) throw std::invalid_argument("Unreachable tile");
    for (std::vector<int> i : std::vector<std::vector<int>>({{0, 1}, {0, -1}, {1, 0}, {-1, 0}}))
    {
        std::vector<int> next_coord = {coord[0] + i[0], coord[1] + i[1]};
        int old_x = coord[0], old_y = coord[1], new_x = next_coord[0], new_y = next_coord[1];
        if (new_x >= map[0].size() || new_y >= map.size() || new_x < 0 || new_y < 0) continue;
        if (state[new_y][new_x] != state[old_y][old_x] + findbyid(base_topo, map[old_y][old_x]).TRV) continue;
        out.push_back(next_coord);
        move(unit, next_coord, out);
        return;
    }
}


Mapmaker::Mapmaker(const std::vector<int>& dimensions, int units) : map(generate_map(dimensions)), dimensions(dimensions), occupancy(generate_map(dimensions)) {}
std::vector<std::vector<int>> Mapmaker::get_generate() { return generate_map(dimensions); }
std::vector<std::vector<int>> Mapmaker::get_map() { return map; }

void Mapmaker::place_unit(Entity& unit)
{
    std::vector<int> coords = unit.location;
    if (occupancy[coords[1]][coords[0]] == 0)
    {
        occupancy[coords[1]][coords[0]] = unit.entity_id;
        unit.location = coords;
        unit.path = generate_map(dimensions);
    }
    else throw std::invalid_argument("Unit already placed here!");
}

void Mapmaker::add_random_obstacles(int n, int m)
{
    std::vector<int> obstacle_ids;
    for (const Terrain& terrain : base_topo) if (!terrain.PASSTHROUGH) obstacle_ids.push_back(terrain.ID);
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> x_dist(0, static_cast<int>(map[0].size()) - 1);
    std::uniform_int_distribution<int> y_dist(0, static_cast<int>(map.size()) - 1);
    std::uniform_int_distribution<int> obstacle_dist(0, static_cast<int>(obstacle_ids.size()) - 1);
    int placed = 0;
    while (placed < n)
    {
        int x = x_dist(rng), y = y_dist(rng);
        if (map[y][x] == 0) { map[y][x] = obstacle_ids[obstacle_dist(rng)]; ++placed; }
    }
    placed = 0;
    while (placed < m)
    {
        int x = x_dist(rng), y = y_dist(rng);
        if (map[y][x] == 0) { map[y][x] = 2; ++placed; }
    }
}

void Mapmaker::path_trace(Entity& unit)
{
    for (std::vector<int>& row : unit.path) std::fill(row.begin(), row.end(), -1);
    unit.path[unit.location[1]][unit.location[0]] = unit.stats.MOV;
    pathtrace(unit.location, unit.stats.MOV, unit.path);
}

void Mapmaker::move(Entity& unit, std::vector<int> delta_coord)
{
    std::vector<int> coord = {unit.location[0]+delta_coord[0], unit.location[1]+delta_coord[1]};
    std::vector<std::vector<int>> out;
    out.push_back(coord);
    move(unit, coord, out);
    for (int i = static_cast<int>(out.size()) - 1; i >= 0; --i) unit.location = out[i];
    plot_travel_history(out, 0, static_cast<int>(map[0].size()), static_cast<int>(map.size()));
    path_trace(unit);
}
