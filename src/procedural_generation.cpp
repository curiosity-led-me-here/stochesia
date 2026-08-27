#include <vector>
#include "raytracer.h"
#include "procedural_generation.h"
using namespace std;

const int TerrainSeed::calculate_cost(const vector<vector<int>>& map, const vector<vector<int>>& path)
{
    int total_cost=0;
    for (const vector<int>& coord : path)
    {
	total_cost += map[coord[1]][coord[0]];
    }
    return total_cost;
}

TerrainSeed::TerrainSeed(vector<vector<int>> map) : map(map), cost_map(vector<vector<int>>(map.size(), vector<int>(map[0].size(), 1))) {};

vector<vector<int>> TerrainSeed::get_map()
{
    return map;
}

vector<vector<int>> TerrainSeed::get_cost_map()
{
    return cost_map;
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

void TerrainSeed::generate_road(vector<vector<int>>& path, int ROAD_TILE)
{
    for (vector<int>& coord : path)
    {
	map[coord[1]][coord[0]] = ROAD_TILE;
    }
}

void TerrainSeed::generate_river(vector<vector<int>>& path, int RIVER)

int main()
{
    int CANVAS_WIDTH = 20;
    int CANVAS_HEIGHT = 20;
    int MOV = 5;
    vector<vector<int>> map(CANVAS_HEIGHT, vector<int>(CANVAS_WIDTH, 1));
    MapVar X;
    X.guildA = {0, 0};
    X.village = {19, 19};
    vector<vector<double>> explored(CANVAS_HEIGHT, vector<double>(CANVAS_WIDTH, 0.0));
    vector<vector<int>> path = recurse(X.guildA, X.village, map, MOV, explored);
    TerrainSeed ts(map);
    ts.smoothen(path);
    plot_paths(map, {path}, X.guildA);
    return 0;
}
