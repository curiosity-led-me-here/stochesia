#include <vector>
#include <iostream>
#include <stdexcept>
#include "general_pathtracing.h"
#include "terrain_data.h"
#include "game_types.h"
#include "cmath"
#include "integration.h"
#include "maps.h"
#include <cstdlib>
#include "map_monitor.h"
#include <algorithm>
#include <deque>

void print(const std::vector<std::vector<int>>& matrix)
{
    for (const auto& row : matrix)
    {
        for (const auto& value : row)
        {
            std::cout << value << ' ';
        }
        std::cout << '\n';
    }
}

void path(int MOV, const std::vector<int>& start, std::vector<std::vector<int>>& out)
{
    if (MOV < 0) throw std::invalid_argument("Movement budget cannot be negative.");
    if (start.size() != 2) throw std::invalid_argument("Start coordinate must be {x, y}.");
    if (out.empty() || out[0].empty()) throw std::invalid_argument("Path grid cannot be empty.");
    for (const std::vector<int>& row : out)
    {
        if (row.size() != out[0].size()) throw std::invalid_argument("Path grid must be rectangular.");
    }
    if (start[0] < 0 || start[1] < 0 ||
        start[0] >= out[0].size() || start[1] >= out.size())
    {
        throw std::out_of_range("Start coordinate is outside the path grid.");
    }

    for (const std::vector<int>& help : helper)
    {
	int budget = MOV;
	std::vector<int> next_coord = {start[0] + help[0], start[1] + help[1]};
	int new_x = next_coord[0];
	int new_y = next_coord[1];
	if (budget == 0) continue;
	if (new_x < 0 || new_x >= out[0].size() ||
	new_y < 0 || new_y >= out.size())
	{
	    continue;
	}
	if (out[new_y][new_x] == -1 || out[new_y][new_x] < budget)
	{
	    out[new_y][new_x] = budget;
	}
	else
	{
	    continue;
	}
	path(budget-1, next_coord, out);
    }
}


void normalize_path(const std::vector<int>& start, std::vector<std::vector<int>>& out)
{
    if (start.size() != 2) throw std::invalid_argument("Start coordinate must be {x, y}.");
    if (out.empty() || out[0].empty()) throw std::invalid_argument("Path grid cannot be empty.");
    for (const std::vector<int>& row : out)
    {
        if (row.size() != out[0].size()) throw std::invalid_argument("Path grid must be rectangular.");
    }
    if (start[0] < 0 || start[1] < 0 ||
        start[0] >= out[0].size() || start[1] >= out.size())
    {
        throw std::out_of_range("Start coordinate is outside the path grid.");
    }

    for (int i=0; i < out.size(); i++)
    {
	for (int j=0; j < out[0].size(); j++)
	{
	    if (out[i][j] == -1)
	    {
		out[i][j] = 0;
	    }
	    else
	    {
		out[i][j] = 1;
	    }
	}
    }
    out[start[1]][start[0]] = 0;
}

void trace(std::vector<std::vector<int>>& out_min, std::vector<std::vector<int>>& out_max, const std::vector<int>& start, int MIN, int MAX)
{
    if (MIN < 0 || MAX < 0 || MIN > MAX)
    {
        throw std::invalid_argument("Range must satisfy 0 <= MIN <= MAX.");
    }
    if (out_min.size() != out_max.size() || out_min.empty() || out_min[0].empty())
    {
        throw std::invalid_argument("Range grids must be non-empty and have matching dimensions.");
    }
    for (int i=0; i < out_min.size(); i++)
    {
	if (out_min[i].size() != out_max[i].size())
	{
	    throw std::invalid_argument("Range grids must be non-empty and have matching dimensions.");
	}
    }

    path(MIN-1, start, out_min);
    normalize_path(start, out_min);
    path(MAX, start, out_max);
    normalize_path(start, out_max);
    for (int i=0; i < out_min.size(); i++)
    {
	for (int j=0; j < out_min[0].size(); j++)
	{
	    if (out_min[i][j] + out_max[i][j] == 2)
	    {
		out_max[i][j] = 0;
	    }
	}
    }
}

void pathtrace(
    std::vector<int> current_coord,
    int budget,
    std::vector<std::vector<int>>& state,
    const std::vector<std::vector<int>>& map,
    terrain::MovementType movement)
{
    for (std::vector<int> i : helper)
    {
        std::vector<int> next_coord = { current_coord[0] + i[0], current_coord[1] + i[1] };
        int x = next_coord[0];
        int y = next_coord[1];
        if (x < 0 || y < 0 || x >= state[0].size() || y >= state.size() || !terrain::can_enter(map[y][x], movement)) continue;
        int penalty = terrain::movement_cost(map[y][x], movement);
        int rem_budget = budget - penalty;
        if (rem_budget < 0) continue;
        if (state[y][x] < rem_budget) state[y][x] = rem_budget;
        else continue;
        pathtrace(next_coord, rem_budget, state, map, movement);
    }
}

std::vector<std::vector<int>> pathtrace(const std::vector<std::vector<int>>& map, std::vector<int> current_coord, Entity& unit)
{
    terrain::MovementType movement = unit.movement;
    std::vector<std::vector<int>> state = map;
    int budget = unit.stats.MOV;
    int x = current_coord[1];
    int y = current_coord[0];
    for (std::vector<int>& row : state) std::fill(row.begin(), row.end(), -1);
    state[x][y] = unit.stats.MOV;
    pathtrace(current_coord, budget, state, map, unit.movement);
    return state;
}

void locate_target(const std::vector<std::vector<int>>& traced, std::vector<int> current_coord, std::vector<std::vector<int>>& out)
{
    for (std::vector<int> i : helper)
    {
	int next_x = current_coord[0] + i[0];
	int next_y = current_coord[1] + i[1];
	if (next_x < 0 || next_y < 0 ||
	    next_y >= static_cast<int>(traced.size()) ||
	    next_x >= static_cast<int>(traced[0].size()))
	{
	    continue;
	}
	
	if (traced[current_coord[1]][current_coord[0]] < traced[next_y][next_x])
	{
	    out.push_back({next_x, next_y});
	    locate_target(traced, {next_x,next_y}, out);
	}
    }
}

std::vector<std::vector<int>> get_max_move(const std::vector<std::vector<int>>& map, std::vector<int> current_coord, Entity& unit)
{
    std::vector<std::vector<int>> out;
    std::vector<std::vector<int>> state = pathtrace(map, current_coord, unit);
    for (int i=0; i < state.size(); i++)
    {
	for (int j=0; j < state[0].size(); j++)
	{
	    if (state[i][j] == 0)
	    {
		out.push_back({j, i});
	    }
	}
    }
    return out;
}

double get_cartesian_distance(std::vector<int> target, std::vector<int> inp)
{
    int x = inp[0];
    int y = inp[1];
    int targ_x = target[0];
    int targ_y = target[1];
    int x_comp = targ_x - x;
    int y_comp = targ_y - y;

    return sqrt((x_comp * x_comp) + (y_comp * y_comp));
}

