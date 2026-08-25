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
#include "piece_config.h"
using namespace std;


void resize_canvas(int width, int height);
void paint(int x, int y);
void clear_pixel(int x, int y);
void render();

vector<vector<int>>
create_circle(
    vector<int> startpoint,
    int army,
    int orientation,
    vector<int> delta
);

bool check_fit(
    vector<vector<int>> coord_list,
    Environment& env,
    int size
);

vector<vector<vector<int>>>
get_placements(
    int army,
    Environment& env,
    int inner
);

vector<vector<vector<int>>>
make_pairs(
    const vector<vector<int>>& a,
    const vector<vector<int>>& b
);

void process_coordinates(
    vector<vector<int>>& top,
    Environment& env
);

void process_coordinates(
    vector<vector<vector<int>>>& top,
    Environment& env
);

vector<int>
find_locus(
    const vector<vector<int>>& in,
    Environment& env
);

void view_path(
    vector<int> current_coord,
    vector<vector<int>>& state,
    const vector<vector<int>>& map,
    terrain::MovementType movement,
    vector<int> target,
    bool& result
);

bool view_path(
    const vector<vector<int>>& map,
    vector<int> current_coord,
    vector<int> target
);

vector<vector<vector<int>>>
pair_valid(
    vector<vector<vector<int>>> in,
    int orientation,
    Environment& env,
    int n
);

void print_teams(
    const vector<vector<vector<int>>>& teams
);

vector<vector<vector<int>>>
run_piece_placement_algorithm(
    Environment& env,
    int units_per_team,
    int depth
);

using PieceSetConfig = void (*) (fe_tiles::AnimationRenderer& render, Environment& env, Environment::ConfigureEnv& config);

void setup_board(PieceSetConfig set, fe_tiles::AnimationRenderer& render, Environment& env, Environment::ConfigureEnv& config);

void setup_guild(Environment& env, Environment::ConfigureEnv& config, fe_tiles::AnimationRenderer& render, vector<string> names, vector<fe_tiles::GuildColor> colors);
