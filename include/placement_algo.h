#pragma once
#include "game_data.h"
#include "pathfinder.h"
#include <vector>
#include "game_data.h"
#include "integration.h"
#include "entity_data.h"
#include "entity_animation.h"
#include "map_monitor.h"
#include "pathfinder.h"
#include "maps.h"
#include <cstdlib>
#include "general_pathtracing.h"
#include <iostream>
#include <cmath>


void resize_canvas(int width, int height);
void paint(int x, int y);
void clear_pixel(int x, int y);
void render();

std::vector<std::vector<int>>
create_circle(
    std::vector<int> startpoint,
    int army,
    int orientation,
    std::vector<int> delta
);

bool check_fit(
    std::vector<std::vector<int>> coord_list,
    Environment& env,
    int size
);

std::vector<std::vector<std::vector<int>>>
get_placements(
    int army,
    Environment& env,
    int inner
);

std::vector<std::vector<std::vector<int>>>
make_pairs(
    const std::vector<std::vector<int>>& a,
    const std::vector<std::vector<int>>& b
);

void process_coordinates(
    std::vector<std::vector<int>>& top,
    Environment& env
);

void process_coordinates(
    std::vector<std::vector<std::vector<int>>>& top,
    Environment& env
);

std::vector<int>
find_locus(
    const std::vector<std::vector<int>>& in,
    Environment& env
);

void view_path(
    std::vector<int> current_coord,
    std::vector<std::vector<int>>& state,
    const std::vector<std::vector<int>>& map,
    terrain::MovementType movement,
    std::vector<int> target,
    bool& result
);

bool view_path(
    const std::vector<std::vector<int>>& map,
    std::vector<int> current_coord,
    std::vector<int> target
);

std::vector<std::vector<std::vector<int>>>
pair_valid(
    std::vector<std::vector<std::vector<int>>> in,
    int orientation,
    Environment& env,
    int n
);

void print_teams(
    const std::vector<std::vector<std::vector<int>>>& teams
);

std::vector<std::vector<std::vector<int>>>
run_piece_placement_algorithm(
    Environment& env,
    int units_per_team,
    int depth
);

void setup_board(fe_tiles::AnimationRenderer& render, Environment& env, Environment::ConfigureEnv& config);

void setup_guild(Environment& env, Environment::ConfigureEnv& config, fe_tiles::AnimationRenderer& render, std::vector<std::string> names, std::vector<fe_tiles::GuildColor> colors);
