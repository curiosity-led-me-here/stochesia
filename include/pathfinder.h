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
#include "maps.h"
#include "entity_registry.h"

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
    std::vector<std::vector<int>> load_map(const maps::TerrainMap& recipe);
    std::vector<int>& get_dimensions();
    std::vector<std::vector<int>>& get_occ();
    std::vector<std::vector<int>>& get_guilds();
    
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
    Mapmaker(const std::vector<int>& dimensions);
    Mapmaker(maps::TerrainMap);
    std::vector<std::vector<int>> get_generate();
    std::vector<std::vector<int>> get_map();
    void place_unit(Entity& unit);
    void add_random_obstacles(int n, int m);
    void path_trace(Entity& unit);
    std::vector<std::vector<int>> consider_occupancy(const Entity& unit);
    std::vector<avl_for_atk> prompt_attack(Entity& unit);
    void move(Entity& unit, std::vector<int> coord);
    void plot_with_units(Registry& registry);
    std::vector<std::vector<int>> attack_range(const Entity& unit, const Weapon& weapon);
    void attack_range(Entity& unit);
};
