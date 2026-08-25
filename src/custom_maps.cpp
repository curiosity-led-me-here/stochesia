#include <vector>
#include <iostream>
#include "stock_intmaps.h"
#include <iomanip>
#include <algorithm>
#include <numeric>
#include "game_data.h"
#include "terrain_data.h"
#include "map_monitor.h"
#include "map_tile_library.h"
#include "pathfinder.h"
#include <random>

using namespace std;

auto glob_factor = 1.0;
auto main_map = stock_intmaps::all_intmaps();

auto grid = stock_intmaps::intmap("chapter_1");

template <typename T>
void print2DVector(const vector<vector<T>>& v) {
    for (const auto& row : v) {
        for (const auto& x : row) {
            cout << setw(3) << x << " ";
        }
        cout << '\n';
    }
}

bool randomizer(double chance)
{
    random_device device;
    mt19937 rng(device());
    bernoulli_distribution dist(chance);
    return (dist(rng));
}

double cartesian_distance(vector<int> target, vector<int> inp)
{
    int x = inp[0];
    int y = inp[1];
    int targ_x = target[0];
    int targ_y = target[1];
    int x_comp = targ_x - x;
    int y_comp = targ_y - y;

    return sqrt((x_comp * x_comp) + (y_comp * y_comp));
}

void prioritize(vector<vector<int>>& state, vector<int> target)
{
    vector<double> distances;
    vector<vector<int>> looker = state;
    for (vector<int> i : state)
    {
	distances.push_back(cartesian_distance(i, target));
    }
    vector<int> idxs(distances.size());
    iota(idxs.begin(), idxs.end(), 0);
    sort(idxs.begin(), idxs.end(), [&](int a, int b)
    {
	return distances[a] < distances[b];
    });
    for (int i=0; i < idxs.size(); i++)
    {
	state[i] = looker[idxs[i]];
    }
}

vector<int> sorted(const vector<int>& v, int N)
{
    N = min(N, (int)v.size());

    vector<int> indices(v.size());

    for (int i = 0; i < v.size(); i++)
        indices[i] = i;

    partial_sort(
        indices.begin(),
        indices.begin() + N,
        indices.end(),
        [&](int a, int b)
        {
            return v[a] < v[b];
        }
    );

    indices.resize(N);

    return indices;
}

const auto decoded = stock_intmaps::decode(grid);

vector<vector<int>> helping({{0, 1}, {0, -1}, {1, 0}, {-1, 0}});

struct PathResult
{
    int cost;
    vector<vector<int>> path;
};

bool if_in(vector<vector<int>> in, vector<int> lookup)
{
    for (vector<int> i : in)
    {
	if (i == lookup)
	{
	    return true;
	}
    }
    return false;
}

void locate_custom_target(const vector<vector<double>>& traced, vector<int> destination, vector<vector<int>>& out)
{
    double best_value = traced[destination[1]][destination[0]];
    vector<int> predecessor;
    for (vector<int> i : helping)
    {
	int next_x = destination[0] + i[0];
	int next_y = destination[1] + i[1];
	if (next_x < 0 || next_y < 0 ||
	    next_y >= static_cast<int>(traced.size()) ||
	    next_x >= static_cast<int>(traced[0].size()))
	{
	    continue;
	}
	
	if (best_value < traced[next_y][next_x])
	{
	    best_value = traced[next_y][next_x];
	    predecessor = {next_x, next_y};
	}
    }
    if (predecessor.empty())
    {
	return;
    }
    out.push_back(predecessor);
    locate_custom_target(traced, predecessor, out);
}

vector<vector<int>> find_edges(const vector<vector<double>>& inp)
{
    vector<vector<int>> binary(inp.size(), vector<int>(inp[0].size(), 0));
    vector<vector<int>> edges;

    // Convert to binary
    for (int y = 0; y < inp.size(); y++)
    {
        for (int x = 0; x < inp[0].size(); x++)
        {
            if (inp[y][x] < 0.0)
            {
                binary[y][x] = 1;
            }
        }
    }

    // Run another gridsearch to find the edges by detecting when a helper looks up a -1
    for (int y = 0; y < binary.size(); y++)
    {
        for (int x = 0; x < binary[0].size(); x++)
        {
            if (binary[y][x] == 1)
            {
                continue;
            }

            for (const vector<int>& helper : helping)
            {
                const int next_x = x + helper[0];
                const int next_y = y + helper[1];

                if (next_x < 0 || next_y < 0 ||
                    next_y >= binary.size() ||
                    next_x >= binary[0].size() ||
                    binary[next_y][next_x] == 1)
                {
                    edges.push_back({x, y});
                    break;
                }
            }
        }
    }
    return edges;
}

void pathstart(const vector<int>& current_coord, double budget, const vector<vector<int>>& map, const vector<int>& target, vector<vector<double>>& state, bool& finished, const vector<vector<double>>& explored)
{
    for (vector<int> i : helping)
    {
        vector<int> next_coord = { current_coord[0] + i[0], current_coord[1] + i[1] };
        int x = next_coord[0];
        int y = next_coord[1];
	
        if (x < 0 || y < 0 || x >= map[0].size() || y >= map.size()) continue;

        double penalty = static_cast<double>(map[y][x]) + (explored[y][x]);
	double rem_budget = budget - penalty;
	if (rem_budget < 0.0) { continue; }
	if (state[y][x] >= rem_budget) { continue; }
	if (next_coord == target)
	{
	    state[y][x] = rem_budget;
	    finished = true;
	    continue;
	}
	state[y][x] = rem_budget;
        pathstart(next_coord, rem_budget, map, target, state, finished, explored);
    }
}

vector<vector<double>> pathstart(const vector<vector<int>>& map, const vector<int>& current_coord, const vector<int>& target, int MOV, bool& finished, const vector<vector<double>>& explored)
{
    vector<vector<double>> state(map.size(), vector<double>(map[0].size(), -1.0)); 
    double budget = MOV;
    int x = current_coord[1];
    int y = current_coord[0];
    state[x][y] = MOV;
    pathstart(current_coord, budget, map, target, state, finished, explored);
    return state;
}

vector<vector<double>> normalize_state(vector<vector<double>>& state, double factor, vector<int> target)
{
    int avg_x = 0;
    int avg_y = 0;
    int denom = 0;
    for (int i=0; i < state.size(); i++)
    {
	for (int j=0; j < state[0].size(); j++)
	{
	    if (state[i][j] > 0.0)
	    {
		avg_x += j;
		avg_y += i;
		denom++;
	    }
	}
    }
    
    avg_x /= denom;
    avg_y /= denom;
    
    vector<vector<double>> out(state.size(), vector<double>(state[0].size(), 0.0));
    for (int i=0; i < state.size(); i++)
    {
	for (int j=0; j < state[0].size(); j++)
	{
	    if (state[i][j] < 0)
	    {
		continue;
	    }
	    else
	    {
		out[i][j] += factor / static_cast<double>(cartesian_distance(target, {avg_x, avg_y}));
	    }
	}
    }
    return out;
}

vector<vector<double>> add(vector<vector<double>> A, vector<vector<double>> B)
{
    if (A.size() == B.size() && A[0].size() == B[0].size())
    {
	auto C = A;
	for (int i = 0; i < A.size(); i++)
	{
	    for (int j = 0; j < A[i].size(); j++)
	    {
		C[i][j] = A[i][j] + (B[i][j]);
	    }
	}
	return C;
    }
    throw invalid_argument("add(vector<vector<double>> A, vector<vector<double>> B) --> A.shape != B.shape");
}

void recurse(vector<int>& current_coord, const vector<int>& target, const vector<vector<int>>& map, vector<vector<double>>& state, vector<vector<int>>& route, int MOV, vector<vector<vector<int>>>& output, bool& finished, vector<vector<bool>>& visited, const vector<vector<double>>& explored)
{
    vector<vector<int>> all_coords = find_edges(state);
    prioritize(all_coords, target);
    for (vector<int> i : all_coords)
    {
	if (visited[i[1]][i[0]] || if_in(route, i)) {continue;}
	visited[i[1]][i[0]] = true;
	bool next_finished = false;
	vector<vector<double>> next_state = pathstart(map, i, target, MOV, next_finished, explored);
	vector<vector<int>> out = { i };
	locate_custom_target(state, i, out);
	out.pop_back();
	reverse(out.begin(), out.end());
	route.insert(route.end(), out.begin(), out.end());
	if (next_finished)
	{
	    vector<vector<int>> final_out = { target };
	    vector<vector<int>> final_route = route;
	    locate_custom_target(next_state, target, final_out);
	    final_out.pop_back();
	    reverse(final_out.begin(), final_out.end());
	    final_route.insert(final_route.end(), final_out.begin(), final_out.end());
	    output.push_back(final_route);
	    route.erase(route.end()-out.size(), route.end());
	    continue;
	}
	recurse(i, target, map, next_state, route, MOV, output, next_finished, visited, explored);
	route.erase(route.end()-out.size(), route.end());
	
    }
}

// State --> warmup Pathtrace grid 
// Map --> Movement cost 2D vector...

vector<vector<int>> select_shortest(vector<vector<vector<int>>>& inp)
{
    vector<vector<int>> lowest;
    for (vector<vector<int>> route : inp)
    {
	if (lowest.empty())
	{
	    lowest = route;
	}
	else if (route.size() < lowest.size())
	{
	    lowest = route;
	}
    }
    return lowest;
}

vector<vector<int>> recurse(vector<int>& current_coord, const vector<int>& target, const vector<vector<int>>& map, int MOV, const vector<vector<double>>& explored)
{
    vector<vector<vector<int>>> output;
    vector<vector<int>> route;
    route.push_back(current_coord);
    bool finished = false;
    vector<vector<double>> state = pathstart(map, current_coord, target, MOV, finished, explored);
    if (finished)
    {
	vector<vector<int>> final_out = { target };
	vector<vector<int>> final_route = route;
	locate_custom_target(state, target, final_out);
	final_out.pop_back();
	reverse(final_out.begin(), final_out.end());
	final_route.insert(final_route.end(), final_out.begin(), final_out.end());
	output.push_back(final_route);
	return select_shortest(output);
    }
    vector<vector<bool>> visited(map.size(), vector<bool>(map[0].size(), false));
    recurse(current_coord, target, map, state, route, MOV, output, finished, visited, explored);
    return select_shortest(output);
}

void heat(const vector<int>& current_coord, int budget, const vector<vector<int>>& map, vector<vector<double>>& state)
{
    for (vector<int> i : helping)
    {
        vector<int> next_coord = { current_coord[0] + i[0], current_coord[1] + i[1] };
        int x = next_coord[0];
        int y = next_coord[1];
	
        if (x < 0 || y < 0 || x >= map[0].size() || y >= map.size()) continue;

        int penalty = map[y][x];
	int rem_budget = budget - penalty;
	if (rem_budget < 0.0) { continue; }
	if (state[y][x] >= rem_budget) { continue; }
	state[y][x] = rem_budget;
        heat(next_coord, rem_budget, map, state);
    }
}

void heat(const vector<vector<int>>& map, vector<vector<int>>& inp, vector<vector<double>>& explored, const int heat_width, const vector<int>& target, const vector<int>& start, const double heat_decay_threshold, int initial_exemption)
{
    double budget = heat_width;
    double cutoff_D = heat_decay_threshold * cartesian_distance(target, start);
    vector<vector<double>> state(explored.size(), vector<double>(explored[0].size(), -1.0));

    // heating
    for (int coord_idx=0; coord_idx < inp.size(); coord_idx++)
    {
	if (coord_idx < initial_exemption)
	{
	    continue;
	}
	int x = inp[coord_idx][1];
	int y = inp[coord_idx][0];
	state[x][y] = heat_width;
	heat(inp[coord_idx], budget, map, state);
    }
    
    // normalization
    for (int x=0; x < state.size(); x++)
    {
	for (int y=0; y < state[0].size(); y++)
	{
	    if (state[x][y] < 0) { state[x][y] = 0; }
	    else
	    {
		double kernel = state[x][y] / heat_width;
		double D = cartesian_distance(target, {y, x});
		double target_scale = D / cutoff_D;
		double added_heat = (kernel * kernel * target_scale);
		state[x][y] = added_heat;
	    }
	}
    }

    // assignment
    for (int x=0; x < explored.size(); x++)
    {
	for (int y=0; y < explored[0].size(); y++)
	{
	    explored[x][y] += state[x][y];
	}
    }
}

// main algorithm
vector<vector<vector<int>>> procedurally_generate(vector<int>& current_coord, const vector<int>& target, const vector<vector<int>>& map, int MOV, int heat_width, vector<vector<double>>& explored, int num_path, const double heat_decay_threshold, int initial_exemption)
{
    vector<vector<vector<int>>> out;
    for (int count=0; count < num_path; count++)
    {
	vector<vector<int>> output = recurse(current_coord, target, map, MOV, explored);
	heat(map, output, explored, heat_width, target, current_coord, heat_decay_threshold, initial_exemption);
	out.push_back(output);
    }
    return out;
}

void plot_paths(const vector<vector<int>>& map,
                const vector<vector<vector<int>>>& paths,
                const vector<int>& start)
{
    maps::IntGrid tiles(map.size(), vector<int>(map[0].size(), 0));
    maps::TilePalette palette = {
        maps::named_tile("plain", fe_tiles::PLAIN, 0, 0, TERRAIN_PLAINS)
    };
    maps::MapRecipe recipe = maps::from_tile_ids(
        fe_tiles::THEME_CHAPTERS_01, tiles, palette);

    Mapmaker preview(recipe);
    fe_tiles::AnimationRenderer renderer;
    renderer.load_map(preview);

    fe_tiles::MapMonitor::Options options;
    options.title = "Custom Map Paths";
    fe_tiles::MapMonitor monitor(recipe, renderer, options);
    if (!paths.empty())
    {
        monitor.show_route_arrows(paths);
    }
    monitor.run();
}

int main()
{
    int CANVAS_WIDTH = 20;
    int CANVAS_HEIGHT = 20;
    vector<vector<int>> map(CANVAS_HEIGHT, vector<int>(CANVAS_WIDTH, 1));
    vector<int> current_coord = {0, 0};
    vector<int> target = {19, 19};
    int MOV = 10;
    int heat_width = 2;
    int num_paths = 20;
    // Entry points should not be blocked by heat. Therefore for first N steps no heat would be emitted.
    int initial_exemption = 5;
    // Heat would start to cool down as we approach n distance (beta * total distance) from target.
    double heat_decay_threshold = 0.35;
    vector<vector<double>> explored(CANVAS_HEIGHT, vector<double>(CANVAS_WIDTH, 0.0));
    vector<vector<vector<int>>> output = procedurally_generate(current_coord, target, map, MOV, heat_width, explored, num_paths, heat_decay_threshold, initial_exemption);
    plot_paths(map, output, current_coord);
}
