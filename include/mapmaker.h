#pragma once

#include <vector>

#include "game_types.h"
using namespace std;

// Terminal render helpers for the sandbox.
void plot_points(
    const vector<vector<int>>& terrain_map,
    int min_x,
    int max_x,
    int min_y,
    int max_y,
    const vector<int>& start
);

void plot_state(
    const vector<vector<int>>& state,
    int min_x,
    int max_x,
    int min_y,
    int max_y,
    const vector<int>& start
);

void plot_path_arrows(
    const vector<vector<int>>& state,
    const vector<vector<int>>& terrain_map,
    int min_x,
    int max_x,
    int min_y,
    int max_y,
    const vector<int>& start
);

void plot_travel_history(
    const vector<vector<int>>& route,
    int current_index,
    int width,
    int height
);

class Mapmaker
{
private:
    vector<vector<int>> map;
    vector<int> dimensions;
    vector<vector<int>> occupancy;

    vector<vector<int>> generate_map(
        const vector<int>& dimensions
    );

    void pathtrace(
        vector<int> current_coord,
        int budget,
        vector<vector<int>>& state
    );

    void build_move_path(
        Entity& unit,
        vector<int>& coord,
        vector<vector<int>>& out
    );

public:
    Mapmaker(const vector<int>& dimensions, int units);

    vector<vector<int>> get_generate();
    vector<vector<int>> get_map();

    void place_unit(Entity& unit);
    void add_random_obstacles(int n, int m);
    void path_trace(Entity& unit);
    void move(Entity& unit, vector<int> destination);
};
