#pragma once
#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>
#include <random>
#include "game_data.h"
#include <algorithm>
#include <utility>

extern void plot_points(const std::vector<std::vector<int>>& terrain_map, int min_x, int max_x, int min_y, int max_y, const std::vector<int>& start);

extern void plot_state(const std::vector<std::vector<int>>& state, int min_x, int max_x, int min_y, int max_y, const std::vector<int>& start);

extern void plot_travel_history(const std::vector<std::vector<int>>& route, int current_index, int width, int height);

extern std::vector<std::vector<int>> select_helper(std::vector<int> coord, std::vector<int> location);

extern bool check_edge(std::vector<int> coord, std::vector<std::vector<int>> path);

class Mapmaker
{
private:

    std::vector<std::vector<int>> map;
    std::vector<int> dimensions;
    std::vector<std::vector<int>> occupancy;
    std::vector<std::vector<int>> guilds;
    std::vector<std::vector<int>> generate_map(const std::vector<int>& dimensions);
    
    void pathtrace(
        std::vector<int> current_coord,
        int budget,
        std::vector<std::vector<int>>& state,
	int guild_id);

    void move(
        Entity& unit,
        std::vector<int>& coord,
        std::vector<std::vector<int>>& out);

public:
    Mapmaker(const std::vector<int>& dimensions, int units);
    std::vector<std::vector<int>> get_generate();
    std::vector<std::vector<int>> get_map();
    void place_unit(Entity& unit);
    void add_random_obstacles(int n, int m);
    void path_trace(Entity& unit);
    void move(Entity& unit, std::vector<int> coord);
    std::vector<std::vector<int>> attack_range(const Entity& unit, const Weapon& weapon);
    void attack_range(Entity& unit);
};
