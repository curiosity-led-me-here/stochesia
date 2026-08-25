#include <vector>
#include "game_data.h"
using namespace std;

const vector<Weapon> Armory=
{
    // NAME, CAT_NAME, MIN_RNK, RNKEXP, ID, CAT, DUR, MT, WT, HIT, CRIT, MINRG, MAXRG

    {"Iron Sword", "Sword", "E",  1, IRON_SWORD, SWORD, 46, 5,  5, 90, 0, 1, 1},
    {"Iron Lance", "Lance", "E",  1, IRON_LANCE, LANCE, 45, 7,  8, 80, 0, 1, 1},
    {"Iron Axe",   "Axe",   "E",  1, IRON_AXE, AXE,   45, 8, 10, 75, 0, 1, 1},
    {"Iron Bow",   "Bow",   "E",  1, IRON_BOW, BOW,   45, 6,  5, 85, 0, 2, 2},

    {"Fire",       "Anima", "E",  1, FIRE, ANIMA, 40, 5,  4, 90, 0, 1, 2},
    {"Lightning",  "Light", "E",  1, LIGHTNING, LIGHT, 35, 4,  6, 95, 5, 1, 2},
    {"Flux",       "Dark",  "D", 31, FLUX, DARK,  45, 7,  8, 80, 0, 1, 2},

    {"Thunder",    "Anima", "D", 31, THUNDER, ANIMA, 35, 8,  6, 80, 5, 1, 2},
};

const vector<Rank> TierInfo=
{
    // ID, minexp, maxexp

    {E,   1,  30},
    {D,  31,  70},
    {C,  71, 120},
    {B, 121, 180},
    {A, 181, 250},
    {S, 251, 251},
};

// HEALHP = -1 means restore to full HP.
const vector<Healer> HealingData =
{
    // Name,         ID,          CAT,      RNKEXP, HEALHP
    {"Vulnerary",    VULNERARY,   NONETYPE, 0,      10},
    {"Elixir",       ELIXIR,      NONETYPE, 0,      -1},

    {"Heal Staff",   HEAL,        STAFF,    1,      10},
    {"Mend Staff",   MEND,        STAFF,    31,     20},
};

Weapon get_weapon(const vector<Weapon>& Armory, int id)
{
    return findbyid(Armory, id);
}

Healer get_heal(const vector<Healer> HealingData, int id)
{
    return findbyid(HealingData, id);
}

void get_next_rank(WeaponLevelExp& x)
{
    if (x.rank.ID != S)
    {
	if (x.current >= x.rank.maxexp)
	{
	    x.rank.ID = static_cast<alpharank>(x.rank.ID+1);
	    x.current = x.current - x.rank.maxexp;
	    Rank new_rank_stats = findbyid(TierInfo, x.rank.ID);
	    x.rank = new_rank_stats;
	    if (x.current >= x.rank.maxexp)
	    {
		get_next_rank(x);
	    }
	}
    }
}

void pathtrace(int limit, vector<int> current_coord, vector<int> target, int cost, vector<vector<int>>& map, vector<PathResult>& outs, vector<vector<bool>>& visited, vector<vector<int>>& current_path, int N, int variants)
{
    int cx = current_coord[0];
    int cy = current_coord[1];
    visited[cy][cx] = true;
    current_path.push_back(current_coord);
    
    for (vector<int> i : helping)
    {
        vector<int> next_coord = { current_coord[0] + i[0], current_coord[1] + i[1] };
        int x = next_coord[0];
        int y = next_coord[1];
	int left_limit = limit - 1;
        if (x < 0 || y < 0 || x >= map[0].size() || y >= map.size()) continue;
	if (visited[y][x]) { continue; }
	if (left_limit < 0) { continue; }
        int new_cost = cost + map[y][x];
	/*
	if (terrain::can_enter(map[y][x], movement))
	{
            int penalty = terrain::movement_cost(map[y][x], movement);
	}
	*/
	if (outs.size() >= N)
	{
	    vector<int> costs;
	    for (PathResult res : outs)
	    {
		costs.push_back(res.cost);
	    }
	    vector<int> best = sorted(costs, N);
	    if (new_cost >= costs[best.back()]) { continue; }
	}
        if (next_coord == target)
	{
	    vector<vector<int>> new_path = current_path;
	    new_path.push_back(next_coord);
	    outs.push_back({new_cost, new_path});
	    continue;
	    if (outs.size() == 5) { return; }
	}
        pathtrace(left_limit, next_coord, target, new_cost, map, outs, visited, current_path, N, variants);
    }
    current_path.pop_back();
    visited[cy][cx] = false;
}

vector<PathResult> pathtrace(vector<vector<int>>& map, vector<int> current_coord, vector<int> target, int N, int branch_depth, int variants)
{
    vector<PathResult> outs;
    vector<vector<bool>> visited(map.size(), vector<bool>(map[0].size(), false));
    vector<vector<int>> result;
    int cost=0;
    const int min_steps = abs(target[0] - current_coord[0]) + abs(target[1] - current_coord[1]);
    const int limit = min_steps + 10;
    vector<vector<int>> current_path;
    pathtrace(limit, current_coord, target, cost, map, outs, visited, current_path, N, variants);
    return outs;
}
