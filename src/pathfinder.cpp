#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>
#include <random>
#include "game_data.h"
#include <algorithm>
#include <utility>
#include <stdexcept>

// n = impassable terrain tiles
// m = passable forest tiles
void add_random_obstacles(
    std::vector<std::vector<int>>& map,
    int n,
    int m
)
{
    if (map.empty() || n < 0 || m < 0)
        return;

    std::vector<int> obstacle_ids;
    std::vector<std::pair<int, int>> open_tiles; // {x, y}

    // Build a list of all impassable terrain IDs.
    for (const Terrain& terrain : base_topo)
    {
        if (!terrain.PASSTHROUGH && terrain.ID != 11) // 11 = NONE / void
        {
            obstacle_ids.push_back(terrain.ID);
        }
    }

    // Find all currently passable cells.
    for (int y = 0; y < static_cast<int>(map.size()); y++)
    {
        for (int x = 0; x < static_cast<int>(map[y].size()); x++)
        {
            int terrain_id = map[y][x];

            if (terrain_id >= 0 &&
                terrain_id < static_cast<int>(base_topo.size()) &&
                base_topo[terrain_id].PASSTHROUGH)
            {
                open_tiles.push_back({x, y});
            }
        }
    }

    std::random_device seed;
    std::mt19937 rng(seed());

    std::shuffle(open_tiles.begin(), open_tiles.end(), rng);

    int blocker_count = std::min(n, static_cast<int>(open_tiles.size()));

    std::uniform_int_distribution<int> choose_obstacle(
        0,
        static_cast<int>(obstacle_ids.size()) - 1
    );

    // Place blockers first.
    for (int i = 0; i < blocker_count; i++)
    {
        int x = open_tiles[i].first;
        int y = open_tiles[i].second;

        map[y][x] = obstacle_ids[choose_obstacle(rng)];
    }

    // 2 = WOODS in your new integer terrain vocabulary.
    int forest_count = std::min(
        m,
        static_cast<int>(open_tiles.size()) - blocker_count
    );

    for (int i = 0; i < forest_count; i++)
    {
        int index = blocker_count + i;

        int x = open_tiles[index].first;
        int y = open_tiles[index].second;

        map[y][x] = 2;
    }
}


void plot_points(
    const std::vector<std::vector<int>>& points,
    int min_x,
    int max_x,
    int min_y,
    int max_y,
    const std::vector<int>& start
)
{
    for (int y = max_y; y >= min_y; y--)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            bool point_exists = false;

            for (const auto& point : points)
            {
                if (point[0] == x && point[1] == y)
                {
                    point_exists = true;
                    break;
                }
            }

            if (x == start[0] && y == start[1])
            {
                std::cout << "o ";
            }
            else if (point_exists)
            {
                std::cout << ". ";
            }
            else if (x == 0 && y == 0)
            {
                std::cout << "+ ";
            }
            else if (x == 0)
            {
                std::cout << "| ";
            }
            else if (y == 0)
            {
                std::cout << "- ";
            }
            else
            {
                std::cout << " ";
            }
        }

        std::cout << '\n';
    }
}

void print(const std::vector<int>& values)
{
    for (int i=0 ; i < values.size(); i++)
    {
	if (i == 0)
	{
	    std::cout << "[";
	}
	std::cout << values[i];
	if (i == values.size()-1)
	{
	    std::cout << "]";
	}
	else
	{
	    std::cout << ", ";	
	}
    }
}

void print(const std::vector<std::vector<int>>& out)
{
    for (int i=0; i < out[0].size(); i++)
    {	
	print(out[i]);
	if (i != out[0].size()-1)
	{
	    std::cout << "\n";
	}
    }
}


bool contains(const std::vector<std::vector<int>>& vec, std::vector<int> element)
{
    return std::find(vec.begin(), vec.end(), element) != vec.end();
}

int find_query(const std::vector<std::vector<int>>& vec, std::vector<int> element)
{
    auto it = std::find(vec.begin(), vec.end(), element);

    if (it != vec.end())
    {
	int location = it - vec.begin();
	return location;
    }
    else
    {
	return -1;
    }
}

class Mapmaker
{
    private:
	std::vector<std::vector<int>> map;
	std::vector<int> dimensions;
	std::vector<std::vector<int>> occupancy;
	std::vector<std::vector<int>> generate_map(const std::vector<int>& dimensions)
	{
	    std::vector<std::vector<int>> out(dimensions[0], std::vector<int>(dimensions[1], 0));
	    return out;
	}

	void pathtrace(std::vector<int> current_coord, int budget, std::vector<std::vector<int>> state)
	{
	    for (std::vector<int> i : std::vector<std::vector<int>>({{0, 1},{0, -1},{1, 0},{-1, 0}}))
	    {
		std::vector<int> next_coord =
		{
		    current_coord[0] += i[0],
		    current_coord[1] += i[1],
		};

		int x = next_coord[0];
		int y = next_coord[1];
		
		if (x < 0 || y < 0 || x >= state[0].size() || y >= state.size() || !findbyid(base_topo, map[y][x]).PASSTHROUGH)              // Obstacle
		{
		    continue;
		}
		int tiletype = map[y][x];
		Terrain terraininfo = findbyid(base_topo, tiletype);
		int penalty = terraininfo.TRV;
		int rem_budget = budget - penalty;                              // Terrain manipulation
		if (rem_budget < 0)
		{
		    continue;
		}
		else if (state[y][x] < rem_budget)
		{
		    state[y][x] = rem_budget;
		}
		else
		{
		    continue;
		}
		pathtrace(next_coord, rem_budget, state);
	    }
	}
	void move(Entity& unit, std::vector<int>& coord, std::vector<std::vector<int>>& out)
	{
	    std::vector<std::vector<int>> state = unit.path;
	    if (state[coord[1]][coord[0]] == -1)
	    {
		throw std::invalid_argument("Unreachable tile");
	    }
	    for (std::vector<int> i : std::vector<std::vector<int>>({{0, 1},{0, -1},{1, 0},{-1, 0}}))
	    {
		std::vector<int> next_coord =
		{
		    coord[0] + i[0],
		    coord[1] + i[1],
		};
		
		int old_x = coord[0];
		int old_y = coord[1];
		int new_x = next_coord[0];
		int new_y = next_coord[1];

		if (new_x >= map[0].size() || new_y >= map.size() || new_x < 0 || new_y < 0)
		{
		    continue;
		}
		
		if (state[new_y][new_x] != state[old_y][old_x]+findbyid(base_topo, map[old_y][old_x]).TRV)
		{
		    continue;
		}
		out.push_back(next_coord);
		move(unit, next_coord, out);
		return;
	    }
	}

    public:
	Mapmaker(const std::vector<int>& dimensions, int units) : map(generate_map(dimensions)), dimensions(dimensions) {}
	std::vector<std::vector<int>> get_map()
	{
	    return map;
	}

	void place_unit(const std::vector<int>& coords, Entity& unit)
	{
	    if (occupancy[coords[0]][coords[1]] == 0)
	    {
		occupancy[coords[0]][coords[1]] += unit.entity_id;
		unit.location = coords;
		unit.path = generate_map(dimensions);
	    }
	    else
	    {
		throw std::invalid_argument("Unit already placed here!");
	    }
	}

	void path_trace(Entity& unit)
	{
	    for (std::vector<int> row : unit.path)
	    {
		std::fill(row.begin(), row.end(), -1);
	    }

	    unit.path[unit.location[1]][unit.location[0]] = unit.stats.MOV;

	    pathtrace(unit.location, unit.stats.MOV, unit.path);
	}

	void move(Entity& unit, std::vector<int>& coord)
	{
	    std::vector<std::vector<int>> out;
	    out.push_back(coord);
	    move(unit, coord, out);
	    for (int i=static_cast<int>(out.size())-1; i >= 0; i--)
	    {
		unit.location = out[i];
	    }
	    path_trace(unit);
	}
    };
    
int main()
{
    return 0;
}
