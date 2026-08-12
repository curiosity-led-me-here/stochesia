#pragma once

#include <string>
#include <vector>

#include "game_types.h"

class Registry;

void plot_points(
    const std::vector<std::vector<int>>& terrain_map,
    int min_x,
    int max_x,
    int min_y,
    int max_y,
    const std::vector<int>& start
);

void plot_state(
    const Entity& unit,
    int min_x,
    int max_x,
    int min_y,
    int max_y
);

void plot_travel_history(
    const std::vector<std::vector<int>>& route,
    int current_index,
    int width,
    int height
);

std::string unit_icon(const Entity& unit);

void print_unit_stats(const Entity& unit);

void print_guild_status(const Guild& guild);

void print_attack_prompts(
    const std::vector<avl_for_atk>& prompts,
    const std::vector<std::vector<int>>& occupancy,
    Registry& registry
);
