#pragma once
#include <vector>
#include <algorithm>
#include <iostream>
using namespace std;

struct PieceData
{
    int moves;
    int cost;
};

struct BoardState
{
    vector<vector<PieceData>> village;
    vector<vector<PieceData>> player;
    vector<vector<PieceData>> VxP;
    vector<vector<int>> village_locations;
    vector<vector<int>> player_locations;
};

enum BinaryStateSpace
{
    ClearOwn,
    Unfair,
    FullyIsolated,
    PartIsolated,
    Balanced,
    Unbalanced,
    NOTA
};

enum logic
{
    every,
    any,
    equality
};

template <typename Func>
bool check(logic q, vector<int> a, Func F)
{
    bool out_a = true;
    bool out_b = false;
    if (q == every) { for (int x : a) { out_a = out_a && F(x); } return out_a; }
    else if (q == any) { for (int x : a) { out_b = out_b || F(x); } return out_b; }
    else if (q == equality) { for (int i=1; i < a.size(); i++) { out_a = out_a && (a[i] == a[i-1]); } return out_a; }
    return false;
}

template <typename Func>
bool check(logic q, vector<vector<int>> a, Func F)
{
    bool out_a = true;
    bool out_b = false;
    if (q == every) { for (vector<int> x : a) { out_a = out_a && F(x); } return out_a; }
    else if (q == any) { for (vector<int> x : a) { out_b = out_b || F(x); } return out_b; }
    else if (q == equality) { for (int i=1; i < a.size(); i++) { out_a = out_a && (a[i] == a[i-1]); } return out_a; }
    return false;
}


class TerrainSeed
{
    private:
	vector<vector<int>> map;
	vector<vector<int>> cost_map;
	vector<vector<double>> explored;
	const int WIDTH;
	const int HEIGHT;
	
    public:
	TerrainSeed(vector<vector<int>> map);
	int draw_offset();
	void draw(vector<vector<int>> coord, int MOV);
	const int calculate_cost(const vector<vector<int>>& path);
	bool check_diagonality(const vector<int>& a, const vector<int>& b);
	void swap(vector<int>& out, const vector<int>& a, const vector<int>& b);
	void smoothen(vector<vector<int>>& path);
	vector<vector<int>>& get_map();
	vector<vector<double>>& get_explored();
	vector<vector<int>>& get_cost_map();
	int get_width();
	int get_height();
	double cart_dist(vector<int> target, vector<int> inp);
	vector<int> random_edge();
	vector<vector<int>> random_two_coords();
	int random_coord(bool row);
	vector<vector<int>> find_perps(const vector<int>& h1, const vector<int>& h2, int offset);
	vector<int> find_closest_edge(vector<int> coord);
	vector<vector<int>> render_river(vector<int> start, vector<vector<int>>& cost_grid, vector<vector<int>>& out);
	vector<vector<int>> find_separation(vector<int> x, const vector<int>& h1, const vector<int>& h2, int perp_dist);
	void optimize(vector<int> current_coord, const vector<vector<int>>& path, vector<vector<int>>& cost_grid, int expected_cost, int path_idx);
	void optimize(vector<vector<int>>& path);
	void preheat();
	vector<vector<int>> grow(vector<vector<int>> inp, int MOV, bool& success);
	vector<vector<int>> generate_river(const vector<int>& h1, const vector<int>& h2, int MOV);
	vector<vector<int>> generate_river(vector<int> x, const vector<int>& h1, const vector<int>& h2, int MOV, int perp_dist);
	vector<vector<vector<int>>> mutate_river(const vector<int>& h1, const vector<int>& h2, vector<vector<int>> river, int MOV);
	bool bridgable(vector<vector<int>> validation_pair, vector<int> river_tile, vector<vector<int>> mainstream);
	vector<vector<int>> bridgable(vector<vector<int>> river);
	vector<vector<int>> get_path(vector<int> current, vector<int> target, int MOV);
};


// DONT FORGET TO PREHEAT THE HEATMAP BEFORE RIVER GENERATION!!!!

class TerrainEngine
{
    private:
	TerrainSeed seed;
	BoardState board;
	BoardState board_init(const vector<vector<int>>& guilds, const vector<vector<int>>& villages, int MOV);
	bool spaced(const vector<vector<int>>& coords, const vector<int>& new_pt, int MIN_DIST, int MOV);
	vector<vector<int>> random_n_coords(int N, int MIN_DIST, int MOV);
	
    public:
	TerrainEngine(vector<vector<int>>& map, vector<vector<int>> guilds, vector<vector<int>> villages, int MOV);
	TerrainEngine(vector<vector<int>>& map, int Nobj, int MINDIST, int MOV);
	BoardState get_board();
	BoardState simulate(BoardState board, vector<vector<int>>& cost_grid, int MOV);
	vector<vector<int>> simulate(int MOV);
	void update(int MOV);
	BinaryStateSpace EvalBinaryState();
};
