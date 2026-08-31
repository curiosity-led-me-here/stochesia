#include <algorithm>
#include <vector>
#include "raytracer.h"
#include "procedural_generation.h"
#include "general_pathtracing.h"
#include "plotter.h"
#include <iomanip>
using namespace std;

const int TerrainSeed::calculate_cost(const vector<vector<int>>& path)
{
    int total_cost=0;
    vector<vector<int>> actual_path = path;
    actual_path.erase(actual_path.begin());
    actual_path.pop_back();
    for (const vector<int>& coord : actual_path)
    {
	total_cost += cost_map[coord[1]][coord[0]];
    }
    return total_cost+1;
}

TerrainSeed::TerrainSeed(vector<vector<int>> map) :
    map(map), 
    cost_map(vector<vector<int>>(map.size(),vector<int>(map[0].size(), 1))),
    explored(vector<vector<double>>(map.size(), vector<double>(map[0].size(), 0.0))), WIDTH(map[0].size()), HEIGHT(map.size()) {};

vector<vector<int>>& TerrainSeed::get_map() { return map; }

vector<vector<int>>& TerrainSeed::get_cost_map() { return cost_map; }

vector<vector<double>>& TerrainSeed::get_explored() { return explored; }

int TerrainSeed::get_width() { return WIDTH; }

int TerrainSeed::get_height() { return HEIGHT; }

void TerrainSeed::draw(vector<vector<int>> coord, int COST)
{
    for (vector<int> loc : coord)
    {
	cost_map[loc[1]][loc[0]] = COST;
    }
    preheat();
}

bool TerrainSeed::check_diagonality(const vector<int>& a, const vector<int>& b)
{
    bool out = false;
    vector<vector<int>> helper = {{1,1}, {-1,-1}, {1,-1}, {-1, 1}};
    for (vector<int> help : helper)
    {
	if (a[0] + help[0] == b[0] && a[1] + help[1] == b[1]) { return true; }
    }
    return out;
}

void TerrainSeed::swap(vector<int>& out, const vector<int>& a, const vector<int>& b)
{
    vector<int> p1 = {a[0], b[1]};
    vector<int> p2 = {b[0], a[1]};
    (p1 == out) ? out=p2 : out=p1;
}

void TerrainSeed::smoothen(vector<vector<int>>& path)
{
    bool done = false;
    for (int i=2; i < path.size()-2; i++)
    {
	if (check_diagonality(path[i-2], path[i]) && check_diagonality(path[i], path[i+2]) && check_diagonality(path[i+1], path[i-1]))
	{
	    swap(path[i], path[i-1], path[i+1]);
	    done = true;
	}
    }
    if (done)
    {
	smoothen(path);
    }
}

vector<vector<int>> TerrainSeed::random_two_coords()
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

int TerrainSeed::random_coord(bool row)
{
    vector<int> out;
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int> width_sample(0, WIDTH-1);
    uniform_int_distribution<int> height_sample(0, HEIGHT-1);
    return (row) ? width_sample(rng) : height_sample(rng);
}

vector<int> TerrainSeed::random_edge()
{
    vector<int> out;
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int> edge(0, 1);
    int edge_loc = edge(rng);
    int side_loc = edge(rng);
    uniform_int_distribution<int> width_sample(0, WIDTH-1);
    uniform_int_distribution<int> height_sample(0, HEIGHT-1);
    if (edge_loc == 0)
    {
	return {(WIDTH-1)*side_loc, height_sample(rng)};
    }
    else
    {
	return {width_sample(rng), (HEIGHT-1)*side_loc};
    }
}

double TerrainSeed::cart_dist(vector<int> target, vector<int> inp)
{
    int x = inp[0];
    int y = inp[1];
    int targ_x = target[0];
    int targ_y = target[1];
    int x_comp = targ_x - x;
    int y_comp = targ_y - y;

    return sqrt((x_comp * x_comp) + (y_comp * y_comp));
}

vector<vector<int>> TerrainSeed::find_perps(const vector<int>& h1, const vector<int>& h2, int offset)
{
    if (h1 == h2) { throw invalid_argument("find_perps(const vector<int>& h1, const vector<int>& h2, int offset) : h1 cannot be == h2"); }
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
    int up_x = center_x + px;
    int up_y = center_y + py;
    int down_x = center_x - px;
    int down_y = center_y - py;
    if (up_x < 0 || up_y < 0 || down_x < 0 || down_y < 0 || up_x >= map[0].size() || down_y >= map.size() || down_x >= map[0].size() || up_y >= map.size()) { throw invalid_argument("Invalid starting points!"); }
    return {{center_x + px, center_y + py}, {center_x - px, center_y - py}, {center_x, center_y}};
}

vector<int> TerrainSeed::find_closest_edge(vector<int> coord)
{
    int dx = WIDTH - coord[0] - 1;
    int dy = HEIGHT - coord[1] - 1;
    int x = (dx < coord[0]) ? WIDTH-1 : 0;
    int y = (dy < coord[1]) ? HEIGHT-1 : 0;
    return {x, y};
}

vector<vector<int>> TerrainSeed::find_separation(vector<int> x, const vector<int>& h1, const vector<int>& h2, int perp_dist)
{
    vector<vector<int>> coords  = find_perps(h1, h2, perp_dist);
    bool farthest = cart_dist(x, coords[1]) > (cart_dist(x, coords[0]));
    vector<int> y = (farthest) ? find_closest_edge(coords[1]) : find_closest_edge(coords[0]);
    return (farthest) ? vector<vector<int>> {x, coords[0], coords[2], coords[1], y} : vector<vector<int>> {x, coords[1], coords[2], coords[0], y};
}

void TerrainSeed::optimize(vector<int> current_coord, const vector<vector<int>>& path, vector<vector<int>>& cost_grid, int expected_cost, int path_idx)
{
    for (int action_idx=0; action_idx < helper.size(); action_idx++)
    {
	vector<int> action = helper[action_idx];
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

vector<vector<int>> TerrainSeed::render_river(vector<int> start, vector<vector<int>>& cost_grid, vector<vector<int>>& out)
{
    for (int action_idx=0; action_idx < helper.size(); action_idx++)
    {
	vector<int> action = helper[action_idx];
	int next_x = start[0] + action[0];
	int next_y = start[1] + action[1];
	if (next_x < 0 || next_y < 0 || next_x >= map[0].size() || next_y >= map.size()) { continue; }
	    if (cost_grid[next_y][next_x] == cost_grid[start[1]][start[0]] + 1)
	    {
		out.push_back({next_x, next_y});
		render_river({next_x, next_y}, cost_grid, out);
	    }
    }
    return out;
}

void TerrainSeed::optimize(vector<vector<int>>& path)
{
    vector<vector<int>> cost_grid(map.size(), vector<int>(map[0].size(), 0));
    vector<int>  current_coord = path[0];
    optimize(current_coord, path, cost_grid, 0, 0);
    vector<vector<int>> out = { current_coord };
    path = render_river(current_coord, cost_grid, out);
}

void TerrainSeed::preheat()
{
    explored.assign(cost_map.size(), vector<double>(cost_map[0].size(), 0.0));
    vector<vector<int>> helper_ = {
	{-1, -1}, {0, -1}, {1, -1},
	{-1,  0},          {1,  0},
	{-1,  1}, {0,  1}, {1,  1}
    };
    for (int i=0; i < explored.size(); i++)
    {
	for (int j=0; j < cost_map[0].size(); j++)
	{
	    if (cost_map[i][j] > 1)
	    {
		for (vector<int> delta : helper_)
		{
		    int new_x = j+delta[0];
		    int new_y = i+delta[1];
		    if (new_x < 0 || new_y < 0 || new_x >= cost_map[0].size() || new_y >= cost_map.size()) { continue; }
		    explored[new_y][new_x] = cost_map[i][j];
		}
	    }
	}
    }
}

vector<vector<int>> TerrainSeed::grow(vector<vector<int>> inp, int MOV, bool& success)
{
    vector<vector<int>> out;
    int len = inp.size();
    for (int k=0, l=1; k < len-1 && l < len; k++, l++)
    {
	vector<vector<int>> outp = recurse(inp[k], inp[l], cost_map, MOV, explored);
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

vector<vector<int>> TerrainSeed::generate_river(const vector<int>& h1, const vector<int>& h2, int MOV)
{
    bool success = false;
    int WIDTH = map[0].size();
    int HEIGHT = map.size();
    vector<vector<int>> path;
    int retries = 0;
    while (!success && retries < 10)
    {
	vector<int> x = random_edge();
	vector<vector<int>> find_set = find_separation(x, h1, h2, 3);
	path = grow(find_set, MOV, success);
	if (!path.empty()) { optimize(path); }
	retries++;
    }
    return (!path.empty()) ? path : throw invalid_argument("River cannot be constructed in this state!");
}

vector<vector<int>> TerrainSeed::generate_river(vector<int> x, const vector<int>& h1, const vector<int>& h2, int MOV, int perp_dist)
{
    bool success = false;
    int WIDTH = map[0].size();
    int HEIGHT = map.size();
    vector<vector<int>> path;
    int retries = 0;
    while (!success && retries < 10)
    {
	vector<vector<int>> find_set = find_separation(x, h1, h2, perp_dist);
	path = grow(find_set, MOV, success);
	if (!path.empty()) { optimize(path); }
	retries++;
    }
    return (!path.empty()) ? path : throw invalid_argument("River cannot be constructed in this state!");
}

bool if_in_(vector<vector<int>> in, vector<int> lookup)
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

struct river_orientation
{
    vector<int> coordinates;
    vector<vector<int>> orientation;
    int cost;
    vector<int> new_orn_coord;
};

vector<vector<vector<int>>> _helper_ {{{-1, -1}, {0, -1}, {-1,  0}}, {{0, -1}, {1, -1}, {1,  0}}, {{-1,  0}, {-1,  1}, {0,  1}}, {{1,  0} ,{0,  1}, {1,  1}}};

bool twobytwo(vector<int> new_coord, vector<vector<int>> river)
{
    bool final_valid = true;
    int new_x = new_coord[0];
    int new_y = new_coord[1];
    for (vector<vector<int>> coords : _helper_)
    {
	bool valid = false;
	for (vector<int> coord : coords)
	{
	    if (!if_in_(river, {new_x + coord[0], new_y + coord[1]})) { valid = true; }
	    if (valid) { break; }
	}
	final_valid = final_valid && valid;
    }
    return final_valid;
}

vector<vector<vector<int>>> TerrainSeed::mutate_river(const vector<int>& h1, const vector<int>& h2, vector<vector<int>> river, int MOV)
{
    // validate perp
    vector<int> perp = find_perps(h1, h2, 0).back();
    vector<river_orientation> candidates;
    vector<vector<vector<int>>> final_candidates;
    for (vector<int> river_coord : river)
    {
	vector<vector<int>> possible_orientations;
	for (vector<int> help : helper)
	{
	    int new_x = river_coord[0] + help[0];
	    int new_y = river_coord[1] + help[1];
	    if (new_x < 0 || new_y < 0 || new_x >= map[0].size() || new_y >= map.size() || if_in_(river, {new_x, new_y})) { continue; }
	    bool final_valid = twobytwo({new_x, new_y}, river);
	    if (final_valid) { possible_orientations.push_back(help); }
	}
	if (possible_orientations.empty()) { continue; }
	candidates.push_back({river_coord, possible_orientations, 0});
    }
    for (int i=0; i < candidates.size();)
    {
	vector<vector<int>> orns;
	for (vector<int> orn : candidates[i].orientation)
	{
	    bool two_by_two = false;
	    int x = candidates[i].coordinates[0] + orn[0];
	    int y = candidates[i].coordinates[1] + orn[1];
	    vector<int> out_coord = {x, y};
	    vector<vector<int>> output = recurse(out_coord, perp, cost_map, MOV, explored);
	    if (output.empty()) { continue; }
	    vector<vector<int>> full_stream = river;
	    full_stream.insert(full_stream.end(), output.begin(), output.end());
	    for (vector<int> river_tile : output)
	    {
		if (twobytwo(river_tile, full_stream)) { continue; }
		else { two_by_two = true; }
	    }
	    if (!two_by_two) { orns.push_back(orn); }
	}
	if (orns.empty())
	{
	    candidates.erase(candidates.begin()+i);
	    continue;
	}
	candidates[i].orientation = orns;
	int new_orn_x = candidates[i].coordinates[0] + candidates[i].orientation[0][0];
	int new_orn_y = candidates[i].coordinates[1] + candidates[i].orientation[0][1];
	candidates[i].new_orn_coord = {new_orn_x, new_orn_y};
	candidates[i].cost = calculate_cost(recurse(candidates[i].new_orn_coord, perp, cost_map, MOV, explored));
	i++;
    }
    sort(candidates.begin(), candidates.end(),
    [](const river_orientation& a, const river_orientation& b)
    { return a.cost < b.cost; });
    
    for (int i=0; i < candidates.size(); i++)
    {
	bool two_by_two = false;
	bool in_same_river = false;
	vector<vector<int>> river_output = generate_river(candidates[i].new_orn_coord, h1, h2, MOV, 3);
	if (river_output.empty()) { continue; }
	vector<vector<int>> full_stream = river;
	full_stream.insert(full_stream.end(), river_output.begin(), river_output.end());
	for (vector<int> river_tile : river_output)
	{
	    if (if_in_(river, river_tile)) { in_same_river = true; }
	    if (twobytwo(river_tile, full_stream)) { continue; }
	    else { two_by_two = true; }
	}
	if (!two_by_two && !in_same_river) { final_candidates.push_back(river_output); }
    }
    return final_candidates;
}


bool TerrainSeed::bridgable(vector<vector<int>> validation_pair, vector<int> river_tile, vector<vector<int>> mainstream)
{
    bool out = true;
    for (vector<int> val_coord : validation_pair)
    {
	int new_x = river_tile[0] + val_coord[0];
	int new_y = river_tile[1] + val_coord[1];
	out = (out && if_in_(mainstream, {new_x, new_y}));
    }
    return out;
}

vector<vector<int>> TerrainSeed::bridgable(vector<vector<int>> river)
{
    vector<vector<int>> output;
    vector<vector<vector<int>>> helper = {{{0, -1}, {-1,  0}}, {{1,  0}, {0,  1}},  {{-1,  0}, {0,  1}}, {{0, -1}, {1,  0}}};
    for (vector<int> river_tiles : river)
    {
	bool out = false;
	for (vector<vector<int>> help : helper)
	{
	    out = (out || bridgable(help, river_tiles, river));
	}
	if (!out)
	{
	    output.push_back(river_tiles);
	}
    }
    return output;
}

vector<vector<int>> TerrainSeed::get_path(vector<int> current, vector<int> target, int MOV)
{
    return recurse(current, target, cost_map, MOV, explored);
}

TerrainEngine::TerrainEngine(vector<vector<int>>& map, vector<vector<int>> guilds, vector<vector<int>> villages, int MOV) : seed(map), board(board_init(guilds, villages, MOV)) {};

TerrainEngine::TerrainEngine(vector<vector<int>>& map, int Nobj, int MINDIST, int MOV) : seed(map)
{
    vector<vector<int>> random_n = random_n_coords(Nobj, MINDIST, MOV);
    int slicer = Nobj;
    vector<vector<int>> guilds(random_n.begin(), random_n.begin()+slicer);
    vector<vector<int>> players(random_n.begin()+slicer, random_n.end());
    if (guilds.size() == players.size())
    {
	board = board_init(guilds, players, MOV);
    }
};

BoardState TerrainEngine::get_board() { return board; } 

BoardState TerrainEngine::board_init(const vector<vector<int>>& guilds, const vector<vector<int>>& villages, int MOV)
{
    BoardState board;
    //villages.size() x villages.size()
    vector<vector<PieceData>> vlg(villages.size(), vector<PieceData>(villages.size(), {0, 0}));
    vector<vector<PieceData>> ply(guilds.size(), vector<PieceData>(guilds.size(), {0, 0}));
    vector<vector<PieceData>> vxp(villages.size(), vector<PieceData>(guilds.size(), {0, 0}));
    board.village = vlg;
    board.player = ply;
    board.VxP = vxp;
    board.village_locations = villages;
    board.player_locations = guilds;
    seed.draw(villages, MOV);
    seed.draw(guilds, MOV);
    return board;
}    



void TerrainEngine::update(int MOV)
{
    vector<vector<int>> v_locs = board.village_locations;
    vector<vector<int>> p_locs = board.player_locations;
    vector<vector<int>> cost_map = seed.get_cost_map();
    vector<vector<double>> explored(cost_map.size(), vector<double>(cost_map[0].size(), 0.0));
    for (int v=0; v < board.village_locations.size(); v++)
    {
	for (int p=0; p < board.player_locations.size(); p++)
	{
	vector<vector<int>> path = recurse(v_locs[v], p_locs[p], cost_map, MOV, explored);
	    if (path.empty()) { board.VxP[v][p] = {0, 0}; }
	    else
	    {
		int path_cost = seed.calculate_cost(path);
		int moves = ceil(static_cast<double>(path_cost) / static_cast<double>(MOV));
		board.VxP[v][p] = {moves, path_cost}; 
	    }
	}
    }

    for (int v1=0; v1 < board.village_locations.size(); v1++)
    {
	for (int v2=0; v2 < board.village_locations.size(); v2++)
	{
	    if (v1 == v2) { board.village[v1][v2] = {0, 0}; continue; }
	vector<vector<int>> path = recurse(v_locs[v1], v_locs[v2], cost_map, MOV, explored);
	    if (path.empty()) { board.village[v1][v2] = {0, 0}; }
	    else
	    {
		int path_cost = seed.calculate_cost(path);
		int moves = ceil(static_cast<double>(path_cost) / static_cast<double>(MOV));
		board.village[v1][v2] = {moves, path_cost};
	    }
	}
    }

    for (int p1=0; p1 < board.player_locations.size(); p1++)
    {
	for (int p2=0; p2 < board.player_locations.size(); p2++)
	{
	    if (p1 == p2) { board.player[p1][p2] = {0, 0}; continue; }
	vector<vector<int>> path = recurse(p_locs[p1], p_locs[p2], cost_map, MOV, explored);
	    if (path.empty()) { board.player[p1][p2] = {0, 0}; }
	    else
	    {
		int path_cost = seed.calculate_cost(path);
		int moves = ceil(static_cast<double>(path_cost) / static_cast<double>(MOV));
		board.player[p1][p2] = {moves, path_cost};
	    }
	}
    }
}

bool TerrainEngine::spaced(const vector<vector<int>>& coords, const vector<int>& new_pt, int MIN_DIST, int MOV)
{
    bool okay = true;
    if (coords.empty()) { return false; }
    for (vector<int> pt : coords)
    {
	okay = (okay && seed.get_path(pt, new_pt, MOV).size() > MIN_DIST+1);
    }
    return okay;
}

BoardState TerrainEngine::simulate(BoardState board, vector<vector<int>>& cost_grid, int MOV)
{
    vector<vector<int>> v_locs = board.village_locations;
    vector<vector<int>> p_locs = board.player_locations;
    vector<vector<double>> explored(cost_grid.size(), vector<double>(cost_grid[0].size(), 0.0));
    for (int v=0; v < board.village_locations.size(); v++)
    {
	for (int p=0; p < board.player_locations.size(); p++)
	{
	vector<vector<int>> path = recurse(v_locs[v], p_locs[p], cost_grid, MOV, explored);
	    if (path.empty()) { board.VxP[v][p] = {0, 0}; }
	    else
	    {
		int path_cost = seed.calculate_cost(path);
		int moves = ceil(static_cast<double>(path_cost) / static_cast<double>(MOV));
		board.VxP[v][p] = {moves, path_cost}; 
	    }
	}
    }

    for (int v1=0; v1 < board.village_locations.size(); v1++)
    {
	for (int v2=0; v2 < board.village_locations.size(); v2++)
	{
	    if (v1 == v2) { board.village[v1][v2] = {0, 0}; continue; }
	vector<vector<int>> path = recurse(v_locs[v1], v_locs[v2], cost_grid, MOV, explored);
	    if (path.empty()) { board.village[v1][v2] = {0, 0}; }
	    else
	    {
		int path_cost = seed.calculate_cost(path);
		int moves = ceil(static_cast<double>(path_cost) / static_cast<double>(MOV));
		board.village[v1][v2] = {moves, path_cost};
	    }
	}
    }

    for (int p1=0; p1 < board.player_locations.size(); p1++)
    {
	for (int p2=0; p2 < board.player_locations.size(); p2++)
	{
	    if (p1 == p2) { board.player[p1][p2] = {0, 0}; continue; }
	vector<vector<int>> path = recurse(p_locs[p1], p_locs[p2], cost_grid, MOV, explored);
	    if (path.empty()) { board.player[p1][p2] = {0, 0}; }
	    else
	    {
		int path_cost = seed.calculate_cost(path);
		int moves = ceil(static_cast<double>(path_cost) / static_cast<double>(MOV));
		board.player[p1][p2] = {moves, path_cost};
	    }
	}
    }
    return board;
}


vector<vector<int>> TerrainEngine::random_n_coords(int teams, int MIN_DIST, int MOV)
{
    int N = teams * 2;
    static random_device rd;
    static mt19937 rng(rd()); 
    uniform_int_distribution<int> xdist(2, seed.get_width() - 3);
    uniform_int_distribution<int> ydist(2, seed.get_height() - 3);
    vector<vector<int>> coords;
    for (int pts=0; pts < N; pts++)
    {
	if (!coords.empty())
	{
	    bool okay = false;
	    while (!okay)
	    {
		vector<int> new_pt = {xdist(rng), ydist(rng)};
		okay = spaced(coords, new_pt, MIN_DIST, MOV);
		if (okay) { coords.push_back(new_pt); }
	    }
	}
	else { coords.push_back({xdist(rng), ydist(rng)}); }
    }
    return coords;
}

void print_matrix(const string& name, const vector<vector<PieceData>>& matrix)
{
    cout << '\n' << name << '\n';

    for (const vector<PieceData>& row : matrix)
    {
        for (const PieceData& cell : row)
        {
            cout << '['
                 << setw(2) << cell.moves
                 << ", "
                 << setw(2) << cell.cost
                 << "] ";
        }
        cout << '\n';
    }
}

void print_board_state(const BoardState& board)
{
    print_matrix("Village -> Village  [turns, cost]", board.village);
    print_matrix("Player -> Player    [turns, cost]", board.player);
    print_matrix("Village -> Player   [turns, cost]", board.VxP);
}

bool check_block(BoardState board)
{
    vector<vector<PieceData>> vxp = board.VxP;
    bool out = true;
    for (vector<PieceData> row : vxp)
    {
	for (PieceData cell : row)
	{
	    bool OUT = (cell.moves == 0 && cell.cost == 0);
	    out = (out && OUT);
	}
    }
    return out;
}

BinaryStateSpace TerrainEngine::EvalBinaryState()
{
    const vector<vector<PieceData>>& vxp = board.VxP;
    if (vxp.empty())
    {
	return NOTA;
    }
    vector<vector<int>> bingrid(vxp.size(), vector<int>(vxp[0].size(), 0));
    for (int i=0; i < vxp.size(); i++)
    {
	for (int j=0; j < vxp[0].size(); j++)
	{
	    if (vxp[i][j].cost == 0 && vxp[i][j].moves == 0) { bingrid[i][j] = 0; }
	    else { bingrid[i][j] = 1; }
	}
    }

    int P = vxp[0].size();
    int V = vxp.size();
    vector<int> village_degree(V, 0);
    vector<int> player_degree(P, 0);
    vector<vector<int>> contenders(V);

    for (int player = 0; player < P; player++)
    {
	for (int village = 0; village < V; village++)
	{
            if (!bingrid[village][player]) { continue; }
            player_degree[player]++;
            village_degree[village]++;
            contenders[village].push_back(player);
	}
    }
    
    // ClearOwn
    if (check(every, village_degree, [](int a){ return (a == 1);}) && check(every, player_degree, [](int a){ return (a == 1);})) { return ClearOwn; }

    // Unfair
    if (check(every, village_degree, [](int a){ return (a == 1);}) && check(any, player_degree, [](int a){ return (a != 1);})) { return Unfair; }
    
    // FullyIsolated
    if (check(every, village_degree, [](int a){ return (a == 0);})) { return FullyIsolated; }
    
    // PartIsolated
    if (check(any, village_degree, [](int a){ return (a == 0);})) { return PartIsolated; }

    // Ownership Balance
    if (check(any, village_degree, [](int a){ return (a > 1);}))
    {
	vector<vector<int>> villages;
	for (int v=0; v < village_degree.size(); v++) if (village_degree[v] > 1) { villages.push_back(contenders[v]); }
	if (check(equality, villages, [](){})) { return Balanced; }
	else { return Unbalanced; }
    }
    return NOTA;
}

vector<vector<int>> TerrainEngine::simulate(int MOV)
{
    int max_tries = 10;
    BoardState new_board = board;
    vector<vector<int>> riva;
    while (!check_block(new_board) || max_tries < 10)
    {
	vector<vector<int>> riv = seed.generate_river(board.player_locations[0], board.village_locations[1], MOV);
	vector<vector<int>> sim_cost_grid = seed.get_cost_map();
	for (vector<int> riv_coord : riv)
	{
	    sim_cost_grid[riv_coord[1]][riv_coord[0]] = MOV+1;
	}
	new_board = simulate(board, sim_cost_grid, MOV);
	if (check_block(new_board)) { riva = riv; }
	max_tries++;
    }
    if (!riva.empty())
    {
	for (vector<int> loc : riva)
	{
	    seed.get_cost_map()[loc[1]][loc[0]] = MOV+1;
	}
    }
    return riva;
}

int main()
{
    // init
    int WIDTH = 15;
    int HEIGHT = 15;
    int MOV = 4;
    vector<vector<int>> map(HEIGHT, vector<int>(WIDTH, 1));
    TerrainEngine engine(map, 2, 5, MOV);
    engine.update(MOV);
    BoardState board = engine.get_board();
    print_board_state(board);
    // evolve map
    vector<vector<int>> riva = engine.simulate(MOV);
    Canvas canvas(WIDTH, HEIGHT);
    canvas.plot(board.village_locations, 2); // red
    canvas.plot(board.player_locations, 3);  // green
    if (!riva.empty()) { canvas.plot(riva, 1); }
    else { cout << "River not possible!" << '\n'; }
    canvas.draw();
    return 0;
}
