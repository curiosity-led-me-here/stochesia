#include "game_data.h"
#include "integration.h"
#include "entity_data.h"
#include "entity_animation.h"
#include "map_monitor.h"
#include "pathfinder.h"
#include "maps.h"
#include <cstdlib>
#include <vector>
#include "general_pathtracing.h"
#include <iostream>
#include <cmath>
#include "placement_algo.h"
#include "piece_config.h"
using namespace std;

/*
int actual_army(int x)
{
    int len = x;
    int invariant = 2;
    int count = 0;
    int counter = 0;
    for (int i=0; i < len; i++)
    {
	for (int j=0; j < invariant; j++)
	{
	    for (int k=0; k < i+1; k++)
	    {
		if (counter == x) { return count+1; }
		counter++;
		//std::cout << count+1;
	    }
	    count++;
	}
    }
    return counter;
}
*/

vector<vector<char>> canvas;

void resize_canvas(int width, int height)
{
    canvas.assign(height, vector<char>(width, '.'));
}

void paint(int x, int y)
{
    if (y < 0 || y >= static_cast<int>(canvas.size()) ||
        x < 0 || x >= static_cast<int>(canvas[y].size()))
        return;

    canvas[y][x] = 'o';
}

void clear_pixel(int x, int y)
{
    if (y < 0 || y >= static_cast<int>(canvas.size()) ||
        x < 0 || x >= static_cast<int>(canvas[y].size()))
        return;

    canvas[y][x] = '.';
}

void render()
{
    for (const auto& row : canvas)
    {
        for (char pixel : row)
            cout << pixel << ' ';

        cout << '\n';
    }
}
	    
vector<vector<int>> create_circle(vector<int> startpoint,int army,int orientation, vector<int> delta)
{
    vector<vector<int>> out;
    if (army <= 0)
        return out;
    int rows = ceil(sqrt(army));
    int width = 2 * rows - 1;
    for (int row = 0; row < rows; row++)
    {
        int row_width = width - 2 * row;
        int offset = row;
        for (int col = 0; col < row_width; col++)
        {
            int x, y;
            if (orientation == 0) // TOP
            {
                x = startpoint[0] + offset + col;
                y = startpoint[1] + row;
            }
            else if (orientation == 1) // BOTTOM
            {
                x = startpoint[0] + offset + col;
                y = startpoint[1] - row;
            }
            else if (orientation == 2) // LEFT
            {
                x = startpoint[0] + row;
                y = startpoint[1] + offset + col;
            }
            else // RIGHT
            {
                x = startpoint[0] - row;
                y = startpoint[1] + offset + col;
            }
	    x += delta[0];
            y += delta[1];
            out.push_back({x, y});
        }
    }
    return out;
}

bool check_fit(vector<vector<int>> coord_list, Environment& env, int size) 
{
    int out = 0;
    vector<vector<int>> map = env.map().get_map();
    for (const auto& coord : coord_list)
    {
	int x = coord[0];
	int y = coord[1];

	if (x < 0 || x >= static_cast<int>(map[0].size()) ||
	y < 0 || y >= static_cast<int>(map.size()))
	{ return false; }

	if (terrain::can_enter(map[y][x], terrain::MovementType::CommonT1))
	++out;
    }
    if (out >= size) { return true;}
    else {return false;}
}

// returns specific placement coords which are valid in terms of placeablility of a particular orientation
vector<vector<vector<int>>> get_placements(int army,  Environment& env, int inner)
{
    int HEIGHT = env.map().get_map().size();
    int WIDTH  = env.map().get_map()[0].size();

    vector<vector<vector<int>>> out;
    int rows = ceil(sqrt(army));
    int span = 2 * rows - 1;
    for (int orientation=0; orientation < 4; orientation++)
    {
	if (orientation == 0 || orientation == 1) // TOP / BOTTOM
	{
	    if (orientation == 0)
	    {
		for (int offset = 0; offset <= WIDTH - span; offset++)
		{
		    vector<vector<int>> circle = create_circle({0, inner}, army, orientation, {offset, 0});
		    if (check_fit(circle, env, army))
		    {
			out.push_back(circle);
		    }
		}
	    }
	    else
	    {
		for (int offset = 0; offset <= WIDTH - span; offset++)
		{
		    vector<vector<int>> circle = create_circle({0, HEIGHT - 1 - inner}, army, orientation, {offset, 0});
		    if (check_fit(circle, env, army))
		    {
			out.push_back(circle);
		    }
		}
	    }
	}
	else if (orientation == 2 || orientation == 3) // LEFT / RIGHT
	{
	    if (orientation == 2)
	    {
		for (int offset = 0; offset <= HEIGHT - span; offset++)
		{
		    vector<vector<int>> circle =
                    create_circle({inner, 0}, army, orientation, {0, offset});

		    if (check_fit(circle, env, army))
		    {
			out.push_back(circle);
		    }
		}
	    }
	    else
	    {
		for (int offset = 0; offset <= HEIGHT - span; offset++)
		{
		    vector<vector<int>> circle =
                    create_circle({WIDTH-1-inner, 0}, army, orientation, {0, offset});

		    if (check_fit(circle, env, army))
		    {
			out.push_back(circle);
		    }
		}
	    }
	}
    }
    return out;
}

vector<vector<vector<int>>> make_pairs(const vector<vector<int>>& a, const vector<vector<int>>& b)
{
    vector<vector<vector<int>>> out;
    for (const auto& x : a) { for (const auto& y : b) { out.push_back({x, y}); } }
    return out;
}

void process_coordinates(vector<vector<int>>& top, Environment& env)
{ 
    for (auto it = top.begin(); it != top.end(); )
    {
	auto vec = *it;
	if (!(terrain::can_enter(env.map().get_map()[vec[1]][vec[0]], terrain::MovementType::CommonT1)))
	{
	    it = top.erase(it);
	}
	else
	{
	    ++it;
	}
    }
}

vector<int> find_locus(const vector<vector<int>>& in, Environment& env)
{
    if (in.empty())
    {
        throw invalid_argument("Cannot find the locus of an empty placement.");
    }
    vector<vector<int>> inter = in;
    process_coordinates(inter, env);
    if (inter.empty())
    {
	throw invalid_argument("find_locus(const vector<vector<int>>& in, Environment& env)");
    }
    int cum_x=0;
    int cum_y=0;
    
    for (const vector<int>& coord : inter)
    {
	cum_x += coord[0];
	cum_y += coord[1];
    }

    int x = cum_x / inter.size();
    int y = cum_y / inter.size();
    return {x, y};
}

void view_path(vector<int> current_coord, vector<vector<int>>& state, const vector<vector<int>>& map, terrain::MovementType movement, vector<int> target, bool& result)
{
    if (result) return;
    for (vector<int> i : helper)
    {
        vector<int> next_coord = { current_coord[0] + i[0], current_coord[1] + i[1] };
        int x = next_coord[0];
        int y = next_coord[1];
        if (x < 0 || y < 0 || x >= state[0].size() || y >= state.size()) continue;
	bool walkable = terrain::can_enter(map[y][x], movement);
	bool clearable = terrain::is_passable_with_action(map[y][x]);
	if (!walkable && !clearable) { continue; }
        if (state[y][x] != -1) continue;
	state[y][x] = 0;
	if (x == target[0] && y == target[1])
	{
	    result = true;
	    return;
	}
        view_path(next_coord, state, map, movement, target, result);
    }
}

bool view_path(const vector<vector<int>>& map, vector<int> current_coord, vector<int> target)
{
    terrain::MovementType movement = terrain::MovementType::CommonT1;
    vector<vector<int>> state = map;
    int x = current_coord[1];
    int y = current_coord[0];
    for (vector<int>& row : state) fill(row.begin(), row.end(), -1);
    bool result = false;
    view_path(current_coord, state, map, movement, target, result);
    return result;
}

// main algorithm
vector<vector<vector<int>>> pair_valid(vector<vector<vector<int>>> in, int orientation, Environment& env, int n)
{
    double max = 0.0;
    vector<vector<vector<int>>> out;
    for (int i=0; i < in.size(); i++)
    {
	for (int j=i+1; j < in.size(); j++)
	{
	    vector<vector<int>> pair1 = in[i];
	    vector<vector<int>> pair2 = in[j];
	    process_coordinates(pair1, env);
	    process_coordinates(pair2, env);
	    
	    bool valid = pair1.size() >= n && pair2.size() >= n;

	    for (const auto& vec1 : pair1) {
		bool reaches_opponent = false;

		for (const auto& vec2 : pair2) {
		    if (view_path(env.map().get_map(), vec1, vec2)) {
			reaches_opponent = true;
			break;
		    }
		}

		if (!reaches_opponent) {
		    valid = false;
		    break;
		}
	    }

	    if (valid) {
		for (const auto& vec2 : pair2) {
		    bool reaches_opponent = false;

		    for (const auto& vec1 : pair1) {
			if (view_path(env.map().get_map(), vec2, vec1)) {
			    reaches_opponent = true;
			    break;
			}
		    }

		    if (!reaches_opponent) {
			valid = false;
			break;
		    }
		}
	    }

	    if (valid)
	    {
		vector<int> vec1 = find_locus(pair1, env);
		vector<int> vec2 = find_locus(pair2, env);
		if (get_cartesian_distance(vec1, vec2) > max)
		{
		    max = get_cartesian_distance(vec1, vec2);
		    out.clear();
		    out.push_back(in[i]);
		    out.push_back(in[j]);
		}
	    }
	    else
	    {
		continue;
	    }
	}
    }
    cout << out.size();
    if (out.size() == 2)
    {
	return out;
    }
    else { throw invalid_argument("More than two teams!"); }
}

void print_teams(const vector<vector<vector<int>>>& teams)
{
    for (const auto& team : teams)
        for (const auto& coord : team)
            paint(coord[0], coord[1]);

    render();
}

void process_coordinates(vector<vector<vector<int>>>& top, Environment& env)
{
    
    for (auto& pair : top)
    {
	for (auto it = pair.begin(); it != pair.end(); )
	{
	    auto vec = *it;
	    if (!(terrain::can_enter(env.map().get_map()[vec[1]][vec[0]], terrain::MovementType::CommonT1)))
	    {
		it = pair.erase(it);
	    }
	    else
	    {
		++it;
	    }
	}
    }
}

vector<vector<vector<int>>> run_piece_placement_algorithm(Environment& env, int units_per_team, int depth)
{
    const vector<vector<int>> map = env.map().get_map();
    resize_canvas(static_cast<int>(map[0].size()), static_cast<int>(map.size()));

    vector<vector<vector<int>>> combined;
    for (int k=0; k < depth; k++)
    {
	vector<vector<vector<int>>> dump = get_placements(units_per_team, env, k);
	combined.insert(combined.end(), dump.begin(), dump.end());
    }
    vector<vector<vector<int>>> top = pair_valid(combined, 0, env, units_per_team);
    process_coordinates(top, env);
    cout << '\n' << "Total team locations processed: " << top.size();
    return top;
}

void setup_board(PieceSetConfig set, fe_tiles::AnimationRenderer& render, Environment& env, Environment::ConfigureEnv& config)
{
    set(render, env, config);
    int unit_count = env.guilds[0].members.size();
    vector<vector<vector<int>>> output_coord = run_piece_placement_algorithm(env, unit_count, 5);
    for (int team=0; team < env.guilds.size(); team++)
    {
	for (int i=0; i < unit_count; i++)
	{
	    Entity& unit = *env.guilds[team].members[i];
	    vector<int> loc = output_coord[team][i];
	    config.configure_entity_location(unit, loc);
	}
    }
    render.sync_units(env.units().live_units());
}

void setup_guild(Environment& env, Environment::ConfigureEnv& config, fe_tiles::AnimationRenderer& render, vector<string> names, vector<fe_tiles::GuildColor> colors)
{
    if (names.size() != colors.size())
    {
	throw invalid_argument("names and colors should match!");
    }
    for (int i=0; i < names.size(); i++)
    {
	config.add_guild(names[i], i+1);
	render.set_guild_color(env.guilds[i], colors[i]);
    }
}

void render_placement_overlay(
    const vector<vector<vector<int>>>& teams,
    Environment& env,
    const maps::MapRecipe& recipe,
    Entity& unit)
{
    const auto& map = env.map().get_map();
    const int height = static_cast<int>(map.size());
    const int width = static_cast<int>(map[0].size());

    fe_tiles::RenderGrid output_blue(
        height, vector<int>(width, -1));

    for (const auto& team : teams)
    {
        for (const auto& coord : team)
        {
            const int x = coord[0];
            const int y = coord[1];

            if (x >= 0 && x < width && y >= 0 && y < height)
                output_blue[y][x] = 0;
        }
    }

    fe_tiles::AnimationRenderer renderer;
    renderer.load_map(env.map());

    auto overlay_painter =
        renderer.entity(unit, fe_tiles::UnitVisual::Paladin);
    overlay_painter.paint_blue(output_blue);

    fe_tiles::MapMonitor::Options monitor_options;
    fe_tiles::MapMonitor monitor(recipe, renderer, monitor_options);
    monitor.run();
}


