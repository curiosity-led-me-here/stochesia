#pragma once
#include "general_pathtracing.h"
#include "terrain_data.h"
#include "game_types.h"
#include "cmath"
#include "integration.h"
#include "maps.h"
#include <cstdlib>
#include "map_monitor.h"
#include <vector>
using namespace std;

extern vector<vector<int>> helper;

void print(const vector<vector<int>>& matrix);

void path(
    int MOV,
    const vector<int>& start,
    vector<vector<int>>& out
);

void normalize_path(
    const vector<int>& start,
    vector<vector<int>>& out
);

void trace(
    vector<vector<int>>& out_min,
    vector<vector<int>>& out_max,
    const vector<int>& start,
    int MIN,
    int MAX
);

void pathtrace(
    vector<int> current_coord,
    int budget,
    vector<vector<int>>& state,
    const vector<vector<int>>& map,
    terrain::MovementType movement
);

vector<vector<int>> pathtrace(const vector<vector<int>>& map, vector<int> current_coord, Entity& unit);

void locate_target(const vector<vector<int>>& traced, vector<int> current_coord, vector<vector<int>>& out);

vector<vector<int>> get_max_move(const vector<vector<int>>& map, vector<int> current_coord, Entity& unit);

double get_cartesian_distance(vector<int> target, vector<int> inp);
