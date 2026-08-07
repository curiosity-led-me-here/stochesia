#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>
#include <random>
#include "game_data.h"
#include <algorithm>
#include <utility>

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
	std::vector<std::vector<int>> current_coords;
	std::vector<std::vector<int>> generate_map(const std::vector<int>& dimensions)
	{
	    std::vector<std::vector<int>> out(dimensions[0], std::vector<int>(dimensions[1], 0));
	    return out;
	}

    public:
	Mapmaker(const std::vector<int>& dimensions, int units) : map(generate_map(dimensions)), dimensions(dimensions), current_coords(units, std::vector<int>({0,0})) {}

	std::vector<std::vector<int>> get_map()
	{
	    return map;
	}
	
	void pointer(const std::vector<int>& coords, int pointer_id)
	{
	    map[coords[0]][coords[1]]++;
	    current_coords[pointer_id] = {coords[0], coords[1]};
	}

	void pathtrace(const std::vector<int>& start, std::vector<std::vector<int>>& out, int MOV, std::vector<std::vector<int>>& state, std::vector<std::vector<int>>& map)
	{
	    std::vector<int> current_coord = start;
	    int budget = MOV;
	    for (std::vector<int> i : std::vector<std::vector<int>>({{0, 1},{0, -1},{1, 0},{-1, 0}}))
	    {
		current_coord[0] += i[0];
		current_coord[1] += i[1];
		if (current_coord[0] < 0 || current_coord[1] < 0 || current_coord[0] >= state[0].size() || current_coord[1] >= state.size() || !findbyid(base_topo, map[current_coord[1]][current_coord[0]]).PASSTHROUGH)              // Obstacle
		{
		    current_coord = start;
		    budget = MOV;
		    continue;
		}
		int tiletype = map[current_coord[1]][current_coord[0]];
		Terrain terraininfo = findbyid(base_topo, tiletype);
		int penalty = terraininfo.TRV;
		budget -= penalty;                              // Terrain manipulation
		if (!contains(out,current_coord))
		{
		    out.push_back(current_coord);
		    if (budget >= 0)
		    {
			state[current_coord[1]][current_coord[0]] = budget;
		    }
		    else
		    {
			state[current_coord[1]][current_coord[0]] = 0;
		    }
		}
		else
		{
		    if (state[current_coord[1]][current_coord[0]] < budget)
		    {
			if (budget >= 0)
			{
			    state[current_coord[1]][current_coord[0]] = budget;
			}
			else
			{
			    state[current_coord[1]][current_coord[0]] = 0;
			}
		    }
		    else
		    {
			current_coord = start;
			budget = MOV;
			continue;
		    }
		}
		if (budget <= 0)
		{
		    current_coord = start;
		    budget = MOV;
		    continue;
		}
		pathtrace(current_coord, out, budget, state, map);
		current_coord = start;
		budget = MOV;
	    }
	}
    };
    
int main()
{
    std::vector<int> coord = {6, 6};
    std::vector<int> prev;
    std::vector<std::vector<int>> out;
    out.push_back(coord);
    int MOV = 5;
    std::vector<std::vector<int>> state = generate_map({12,12});
    std::vector<std::vector<int>> intmap = state;
    add_random_obstacles(intmap, 10, 40);
    state[coord[1]][coord[0]] += MOV;
    pathtrace(coord, out, MOV, state, intmap);
    std::cout << '\n';
    std::cout << '\n';
    print(intmap);
    std::cout << '\n';
    std::cout << '\n';
    print(state);
    std::cout << '\n';
    std::cout << '\n';
    plot_points(out, 0, 15, 0, 15, coord);
    return 0;
}
