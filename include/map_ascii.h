#pragma once

#include <string>
#include <vector>

#include "game_types.h"
using namespace std;

class Registry;

void plot_points(
    const vector<vector<int>>& terrain_map,
    int min_x,
    int max_x,
    int min_y,
    int max_y,
    const vector<int>& start
);

void plot_state(
    const Entity& unit,
    int min_x,
    int max_x,
    int min_y,
    int max_y
);

void plot_travel_history(
    const vector<vector<int>>& route,
    int current_index,
    int width,
    int height
);

string unit_icon(const Entity& unit);

void print_unit_stats(const Entity& unit);

void print_guild_status(const Guild& guild);

void print_attack_prompts(
    const vector<avl_for_atk>& prompts,
    const vector<vector<int>>& occupancy,
    Registry& registry
);
