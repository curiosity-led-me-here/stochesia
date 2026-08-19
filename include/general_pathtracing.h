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

extern std::vector<std::vector<int>> helper;

void print(const std::vector<std::vector<int>>& matrix);

void path(
    int MOV,
    const std::vector<int>& start,
    std::vector<std::vector<int>>& out
);

void normalize_path(
    const std::vector<int>& start,
    std::vector<std::vector<int>>& out
);

void trace(
    std::vector<std::vector<int>>& out_min,
    std::vector<std::vector<int>>& out_max,
    const std::vector<int>& start,
    int MIN,
    int MAX
);

void pathtrace(
    std::vector<int> current_coord,
    int budget,
    std::vector<std::vector<int>>& state,
    const std::vector<std::vector<int>>& map,
    terrain::MovementType movement
);

std::vector<std::vector<int>> pathtrace(const std::vector<std::vector<int>>& map, std::vector<int> current_coord, Entity& unit);

void locate_target(const std::vector<std::vector<int>>& traced, std::vector<int> current_coord, std::vector<std::vector<int>>& out);

std::vector<std::vector<int>> get_max_move(const std::vector<std::vector<int>>& map, std::vector<int> current_coord, Entity& unit);

double get_cartesian_distance(std::vector<int> target, std::vector<int> inp);
