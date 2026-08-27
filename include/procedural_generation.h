#pragma once
#include <vector>
#include <iostream>
using namespace std;

struct MapVar
{
    vector<int> guildA;
    vector<int> guildB;
    vector<int> village;
};

class TerrainSeed
{
    private:
	vector<vector<int>> map;
	vector<vector<int>> cost_map;
	int draw_offset();
	const int calculate_cost(const vector<vector<int>>& map, const vector<vector<int>>& path);
	bool check_diagonality(const vector<int>& a, const vector<int>& b);
	void swap(vector<int>& out, const vector<int>& a, const vector<int>& b);
	void smoothen(vector<vector<int>>& path);
	

    public:
	TerrainSeed(vector<vector<int>> map);
	vector<vector<int>> get_map();
	vector<vector<int>> get_cost_map();
	void generate_road(vector<vector<int>>& path, int ROAD_TILE);
	
};
