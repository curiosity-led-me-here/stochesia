#include <vector>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <stdexcept>
#include "general_pathtracing.h"

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
