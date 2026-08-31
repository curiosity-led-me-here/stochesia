#include <iostream>
#include <vector>
#include <random>
#include "plotter.h"
#include "raytracer.h"
#include "procedural_generation.h"
using namespace std;

vector<vector<int>> action_space({{0, 1}, {0, -1}, {1, 0}, {-1, 0}});

Canvas::Canvas(int w, int h)
    : width(w), height(h), grid(h, vector<int>(w, 0)) {}

void Canvas::plot(int x, int y, int color)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    grid[y][x] = color;
}

void Canvas::plot(const vector<vector<int>>& coords, int color)
{
    for (const auto& coord : coords)
        plot(coord[0], coord[1], color);
}

void Canvas::draw()
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            switch (grid[y][x])
            {
                case 0: cout << "⬛"; break;
                case 1: cout << "🟦"; break;
                case 2: cout << "🟥"; break;
                case 3: cout << "🟩"; break;
                case 4: cout << "🟨"; break;
                case 5: cout << "🟪"; break;
                case 6: cout << "🟧"; break;
                default: cout << "⬜"; break;
            }
        }

        cout << '\n';
    }
}

// (0, x) --> left
// (x, 0) --> up
// (max, x) --> right
// (x, max) --> down

vector<vector<int>> random_two_coords(int WIDTH, int HEIGHT)
{
    static random_device rd;
    static mt19937 rng(rd());

    uniform_int_distribution<int> xdist(2, WIDTH - 3);
    uniform_int_distribution<int> ydist(2, HEIGHT - 3);

    vector<int> a = {xdist(rng), ydist(rng)};
    vector<int> b;

    do
    {
        b = {xdist(rng), ydist(rng)};
    }
    while (b == a);

    return {a, b};
}

int random_coord(int WIDTH, int HEIGHT, bool row)
{
    vector<int> out;
    static random_device rd;
    static mt19937 rng(rd());
    static uniform_int_distribution<int> width_sample(0, WIDTH-1);
    static uniform_int_distribution<int> height_sample(0, HEIGHT-1);
    return (row) ? width_sample(rng) : height_sample(rng);
}

vector<int> random_edge(int WIDTH, int HEIGHT)
{
    vector<int> out;
    static random_device rd;
    static mt19937 rng(rd());
    static uniform_int_distribution<int> edge(0, 1);
    int edge_loc = edge(rng);
    int side_loc = edge(rng);
    static uniform_int_distribution<int> width_sample(0, WIDTH-1);
    static uniform_int_distribution<int> height_sample(0, HEIGHT-1);
    if (edge_loc == 0)
    {
	return {(WIDTH-1)*side_loc, height_sample(rng)};
    }
    else
    {
	return {width_sample(rng), (HEIGHT-1)*side_loc};
    }
}

double cart_dist(vector<int> target, vector<int> inp)
{
    int x = inp[0];
    int y = inp[1];
    int targ_x = target[0];
    int targ_y = target[1];
    int x_comp = targ_x - x;
    int y_comp = targ_y - y;

    return sqrt((x_comp * x_comp) + (y_comp * y_comp));
}

vector<vector<int>> find_perps(int WIDTH, int HEIGHT, const vector<int>& h1, const vector<int>& h2, int offset)
{
    int x1 = h1[0];
    int y1 = h1[1];
    int x2 = h2[0];
    int y2 = h2[1];
    int center_x = (x1 + x2) / 2;
    int center_y = (y1 + y2) / 2;
    int dx = x2 - x1;
    int dy = y2 - y1;
    double len = sqrt(dx * dx + dy * dy);
    int px = round((-dy / len) * offset);
    int py = round(( dx / len) * offset);
    if ((px > WIDTH) || (py > HEIGHT))
    {
	px = round((-dy / len));
	py = round(( dx / len));
    }
    if ((px > WIDTH) || (py > HEIGHT)) { throw invalid_argument("Invalid starting points!"); }
    return {{center_x + px, center_y + py}, {center_x - px, center_y - py}, {center_x, center_y}};
}

vector<int> find_closest_edge(int WIDTH, int HEIGHT, vector<int> coord)
{
    int dx = WIDTH - coord[0];
    int dy = HEIGHT - coord[1];
    int x = (dx < coord[0]) ? WIDTH-1 : 0;
    int y = (dy < coord[1]) ? HEIGHT-1 : 0;
    return {x, y};
}

vector<vector<int>> find_separation(vector<int> x, int WIDTH, int HEIGHT, const vector<int>& h1, const vector<int>& h2)
{
    vector<vector<int>> coords  = find_perps(WIDTH, HEIGHT, h1, h2, 3);
    bool farthest = cart_dist(x, coords[1]) > (cart_dist(x, coords[0]));
    vector<int> y = (farthest) ? find_closest_edge(WIDTH, HEIGHT, coords[1]) : find_closest_edge(WIDTH, HEIGHT, coords[0]);
    return (farthest) ? vector<vector<int>> {x, coords[0], coords[2], coords[1], y} : vector<vector<int>> {x, coords[1], coords[2], coords[0], y};
}

/* for mutation
vector<vector<int>> find_separation(int WIDTH, int HEIGHT, vector<int> seed, const vector<int>& h1, const vector<int>& h2)
{
    vector<vector<int>> coords  = find_perps(WIDTH, HEIGHT, h1, h2, 3);
    bool farthest = cart_dist(x, coords[1]) > (cart_dist(x, coords[0]));
    vector<int> y = (farthest) ? find_closest_edge(WIDTH, HEIGHT, coords[1]) : find_closest_edge(WIDTH, HEIGHT, coords[0]);
    return (farthest) ? vector<vector<int>> {x, coords[0], coords[2], coords[1], y} : vector<vector<int>> {x, coords[1], coords[2], coords[0], y};
}
*/

void optimize(vector<int> current_coord, const vector<vector<int>>& path, vector<vector<int>>& cost_grid, int expected_cost, int path_idx)
{
    for (int action_idx=0; action_idx < action_space.size(); action_idx++)
    {
	vector<int> action = action_space[action_idx];
	int next_x = current_coord[0] + action[0];
	int next_y = current_coord[1] + action[1];
	if (path_idx != path.size()-1)
	{
	    if (vector<int>{next_x, next_y} == path[path_idx+1])
	    {
		int new_expected_cost = expected_cost + 1;
		if (cost_grid[next_y][next_x] == 0 || new_expected_cost < cost_grid[next_y][next_x])
		{
		    cost_grid[next_y][next_x] = new_expected_cost;
		    optimize({next_x, next_y}, path, cost_grid, new_expected_cost, path_idx+1);
		}
		else { continue; }
	    }
	}
    }
}

vector<vector<int>> render_river(vector<int> start, vector<vector<int>>& cost_grid, const vector<vector<int>>& map, vector<vector<int>>& out)
{
    for (int action_idx=0; action_idx < action_space.size(); action_idx++)
    {
	vector<int> action = action_space[action_idx];
	int next_x = start[0] + action[0];
	int next_y = start[1] + action[1];
	if (next_x < 0 || next_y < 0 || next_x >= map.size() || next_y >= map[0].size()) { continue; }
	    if (cost_grid[next_y][next_x] == cost_grid[start[1]][start[0]] + 1)
	    {
		out.push_back({next_x, next_y});
		render_river({next_x, next_y}, cost_grid, map, out);
	    }
    }
    return out;
}

void optimize(vector<vector<int>>& path, const vector<vector<int>>& map)
{
    vector<vector<int>> cost_grid(map.size(), vector<int>(map[0].size(), 0));
    vector<int>  current_coord = path[0];
    optimize(current_coord, path, cost_grid, 0, 0);
    vector<vector<int>> out = { current_coord };
    path = render_river(current_coord, cost_grid, map, out);
}

vector<vector<int>> grow(vector<vector<int>> inp, vector<vector<int>> map, int MOV, bool& success)
{
    vector<vector<int>> old_map = map;
    vector<vector<double>> explored(map.size(), vector<double>(map[0].size(), 0.0));
    vector<vector<int>> helper = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0},          {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    };
    for (int i=0; i < map.size(); i++)
    {
	for (int j=0; j < map[0].size(); j++)
	{
	    if (old_map[i][j] >= MOV+1)
	    {
		for (vector<int> delta : helper)
		{
		    int new_x = i+delta[1];
		    int new_y = j+delta[0];
		    if (new_x < 0 || new_y < 0 || new_x >= map.size() || new_y >= map[0].size()) { continue; }
		    map[new_x][new_y] += MOV;
		}
	    }
	}
    }
    vector<vector<int>> out;
    int len = inp.size();
    for (int k=0, l=1; k < len-1 && l < len; k++, l++)
    {
	vector<vector<int>> outp = recurse(inp[k], inp[l], map, MOV, explored);
	if (outp.empty())
	{
	    success = false;
	    return {};
	}
	outp.pop_back();
	out.insert(out.end(), outp.begin(), outp.end());
    }
    out.push_back(inp[len-1]);
    success = true;
    return out;
}

vector<vector<int>> generate_river(const vector<int>& h1, const vector<int>& h2, vector<vector<int>>& map, int MOV)
{
    bool success = false;
    int WIDTH = map[0].size();
    int HEIGHT = map.size();
    vector<vector<int>> path;
    while (!success)
    {
	vector<int> x = random_edge(WIDTH, HEIGHT);
	vector<vector<int>> find_set = find_separation(x, WIDTH, HEIGHT, h1, h2);
	path = grow(find_set, map, MOV, success);
	optimize(path, map);
    }
    return (!path.empty()) ? path : throw invalid_argument("River cannot be constructed in this state!");
}

int main()
{
    int WIDTH = 20;
    int HEIGHT = 20;
    int MOV = 5;
    vector<vector<int>> map(HEIGHT, vector<int>(WIDTH, 1));
    vector<vector<int>> base_points = {{5, 5}, {14, 14}};
    vector<int> third_point = {8, 16};
    vector<vector<int>> mutation_points = {
	base_points[1], third_point};
    TerrainSeed terrain_seed(map);
    vector<vector<int>> river = terrain_seed.generate_river(base_points[0], base_points[1], MOV);
    vector<vector<vector<int>>> candidates = terrain_seed.mutate_river(mutation_points[0], mutation_points[1], river, MOV);

    if (candidates.empty())
    {
	Canvas canvas(WIDTH, HEIGHT);
	canvas.plot(river, 4);
	canvas.plot(base_points, 2);
	canvas.plot(vector<vector<int>>{third_point}, 3);
	cout << "No healthy mutation candidates\n";
	canvas.draw();
	return 0;
    }

    for (int i=0; i < candidates.size(); i++)
    {
	Canvas canvas(WIDTH, HEIGHT);
	canvas.plot(river, 4);
	canvas.plot(candidates[i], 1);
	canvas.plot(base_points, 2);
	canvas.plot(vector<vector<int>>{third_point}, 3);
	cout << "Candidate " << i+1 << "/" << candidates.size() << "  yellow=base blue=mutation red=base green=third\n";
	canvas.draw();
	if (i < candidates.size()-1)
	{
	    cout << "Press Return for the next candidate\n";
	    cin.get();
	}
    }
    return 0;
}
