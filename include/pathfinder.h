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
using namespace std;

extern vector<vector<int>> select_helper(vector<int> coord, vector<int> location);

extern bool check_edge(vector<int> coord, vector<vector<int>> path);

extern bool check_valid_coords(vector<int> coords, vector<avl_for_atk> prompts);

extern vector<int> retry(vector<avl_for_atk> prompts);

class Mapmaker
{
private:

    vector<vector<int>> map;
    vector<int> dimensions;
    vector<vector<int>> occupancy;
    vector<vector<int>> guilds;
    vector<vector<int>> generate_map(const vector<int>& dimensions);
    vector<vector<int>> load_map(const maps::TerrainMap& recipe);
    vector<int>& get_dimensions();
    vector<vector<int>>& get_occ();
    vector<vector<int>>& get_guilds();
    
    void pathtrace(
        vector<int> current_coord,
        int budget,
        vector<vector<int>>& state,
	int guild_id,
        terrain::MovementType movement);

    void move(
        Entity& unit,
        vector<int>& coord,
        vector<vector<int>>& out);

public:
    Mapmaker(const vector<int>& dimensions);
    Mapmaker(maps::TerrainMap);
    Mapmaker(const maps::MapRecipe& recipe);
    int entity_at(vector<int> coordinates);
    vector<vector<int>> get_generate();
    vector<vector<int>> get_map();
    void place_unit(Entity& unit);
    void add_random_obstacles(int n, int m);
    void path_trace(Entity& unit);
    void death(Entity& unit);
    vector<vector<int>> consider_occupancy(const Entity& unit);
    vector<vector<int>> render_move(Entity& unit, vector<int> delta_coord);
    vector<avl_for_atk> prompt_attack(Entity& unit);
    void update_attack_range(Entity& unit);
    vector<vector<int>> move(Entity& unit, vector<int> coord);
    void plot_with_units(Registry& registry);
    void plot_movement_frame(Registry& registry, const Entity& moving_unit, int delay_ms = 140);
    vector<vector<int>> standing_attack_range(const Entity& unit, const Weapon& weapon);
    vector<vector<int>> attack_range(const Entity& unit, const Weapon& weapon);
    void attack_range(Entity& unit);
};
