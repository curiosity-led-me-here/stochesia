#include <iostream>
#include <stdexcept>
#include <string>
#include "pathfinder.h"
#include "game_data.h"
#include "mechanics_ascii.h"
#include "mechanics.h"
#include "entity_registry.h"
#include "integration.h"
#include "entity_data.h"
#include <cassert>
#include <vector>
#include <map_ascii.h>
#include <sstream>
using namespace std;

bool phase_done(Guild& team)
{
    for (int i=0; i < team.members.size(); i++)
    {
	Entity A = *team.members[i];
	if (A.turn)
	{
	    return false;
	}
    }
    return true;
}

void reset_phase(Guild& team)
{
    for (int i=0; i < team.members.size(); i++)
    {
	Entity& A = *team.members[i];
	A.turn = true;
    }
}

int all_dead(Guild& team)
{
    for (int i=0; i < team.members.size(); i++)
    {
	Entity A = *team.members[i];
	if (A.alive)
	{
	    return 1;
	}
    }
    return 0;
}

bool game_over(vector<Guild> guilds)
{
    int left = 0;
    for (Guild team : guilds)
    {
	left += all_dead(team);
    }
    if (left <= 1)
    {
	return true;
    }
    else
    {
	return false;
    }
}

Guild survivor(vector<Guild> guilds)
{
    for (Guild team : guilds)
    {
	if (all_dead(team) == 1)
	{
	    return team;
	}
    }
    throw invalid_argument("No team survived!");
}

Command process_command(string name)
{
    cout << name << "'s phase > ";
    string raw_str;
    getline(cin >> ws, raw_str);

    istringstream stream(raw_str);
    vector<string> out;
    string token;

    while (stream >> token)
    {
        out.push_back(token);
    }

    if (out.size() == 2 && out[0] == "check")
    {
	try
	{
	    return {out[0], stoi(out[1]), {}};
	}
	catch (const invalid_argument& e)
	{
	    cout << "Command must be {move id dx dy} or {check id}." << '\n';
	    return process_command(name);
	}
	catch (const out_of_range& e)
	{
	    cout << "Command must be {move id dx dy} or {check id}." << '\n';
	    return process_command(name);
	}
    }

    if (out.size() < 4 || out.size() > 4)
    {
	cout << "Command must be {move id dx dy} or {check id}." << '\n';
        return process_command(name);
    }

    try
    {
        string name = out[0];
        int id = stoi(out[1]);
        int x = stoi(out[2]);
        int y = stoi(out[3]);

	if (name != "move")
	{
	    cout << "Command must be {move id dx dy} or {check id}." << '\n';
	    return process_command(name);
	}
	
        return {name, id, {x, y}};
    }
    catch (const invalid_argument& e)
    {
	cout << "Command must be {move id dx dy} or {check id}." << '\n';
        return process_command(name);
    }
    catch (const out_of_range& e)
    {
	cout << "Command must be {move id dx dy} or {check id}." << '\n';
        return process_command(name);
    }
}

Environment::Environment(const maps::TerrainMap& recipe)
    : map_recipe(maps::gameplay_only(recipe)), board(map_recipe.terrain), registry(), cursor({static_cast<int>(board.get_map()[0].size()) / 2, static_cast<int>(board.get_map().size()) / 2})
{};

Environment::Environment(const maps::MapRecipe& recipe)
    : map_recipe(recipe), board(map_recipe.terrain), registry(), cursor({static_cast<int>(board.get_map()[0].size()) / 2, static_cast<int>(board.get_map().size()) / 2})
{};

void Environment::move_up()
{
    vector<int> new_coords = {cursor[0], cursor[1]-1};
    if (new_coords[0] < static_cast<int>(board.get_map()[0].size()) && new_coords[1] < static_cast<int>(board.get_map().size()) && new_coords[0] >= 0 && new_coords[1] >= 0) { cursor = new_coords; }
}

void Environment::move_down()
{
    vector<int> new_coords = {cursor[0], cursor[1]+1};
    if (new_coords[0] < static_cast<int>(board.get_map()[0].size()) && new_coords[1] < static_cast<int>(board.get_map().size()) && new_coords[0] >= 0 && new_coords[1] >= 0) { cursor = new_coords; }
}

void Environment::move_right()
{
    vector<int> new_coords = {cursor[0]+1, cursor[1]};
    if (new_coords[0] < static_cast<int>(board.get_map()[0].size()) && new_coords[1] < static_cast<int>(board.get_map().size()) && new_coords[0] >= 0 && new_coords[1] >= 0) { cursor = new_coords; }
}

void Environment::move_left()
{
    vector<int> new_coords = {cursor[0]-1, cursor[1]};
    if (new_coords[0] < static_cast<int>(board.get_map()[0].size()) && new_coords[1] < static_cast<int>(board.get_map().size()) && new_coords[0] >= 0 && new_coords[1] >= 0) { cursor = new_coords; }
}

int Environment::detect_unit()
{
    return board.entity_at({cursor[1], cursor[0]});
}

const maps::MapRecipe& Environment::map_data() const
{
    return map_recipe;
}

Mapmaker& Environment::map()
{
    return board;
}

const Mapmaker& Environment::map() const
{
    return board;
}

Registry& Environment::units()
{
    return registry;
}

const Registry& Environment::units() const
{
    return registry;
}

Environment::ConfigureEnv::ConfigureEnv(Environment& env) : env(env) {};

void Environment::ConfigureEnv::add_guild(string name, int id)
{
    assert(id != 0);
    env.guilds.push_back(Guild{});
    Guild& team = env.guilds.back();
    team.name = name;
    team.guild_id = id;
}

Entity& Environment::ConfigureEnv::configure_entity(Entity unit, int id, Guild& guild)
{
    Entity& out_unit = env.registry.spawn(unit, id);
    assert(id != 0);
    guild.add(out_unit);
    return out_unit;
}

void Environment::ConfigureEnv::configure_entity_location(Entity& out_unit, const vector<int> location)
{
    out_unit.location = location;
    out_unit.terrain_id = env.map().get_map()[location[1]][location[0]];
    if (terrain::can_enter(env.map().get_map()[location[1]][location[0]], out_unit.movement))
    {
	env.board.place_unit(out_unit);
	env.board.path_trace(out_unit);
        return;
    }
    throw invalid_argument("Entity could not be placed on a non-placeable tile!");
}

void Environment::ConfigureEnv::configure_render(Entity& unit, fe_tiles::AnimationRenderer& render, fe_tiles::UnitVisual visual)
{
    auto art = render.entity(unit, visual); 
    env.local_registry.emplace(unit.entity_id, art);
}

Environment::Game::Game(Environment& env) : env(env), config(ConfigureEnv(env)) {};

void Environment::Game::start()
{
    assert(env.guilds.size() > 1);
    int team_id = 0;
    while (!game_over(env.guilds))
    {
	team_id = (team_id)%env.guilds.size();
	reset_phase(env.guilds[team_id]);
	while (!phase_done(env.guilds[team_id]))
	{
	    string raw_str;
	    const Guild& current_guild = env.guilds[team_id];
	    env.board.plot_with_units(env.registry);
	    print_guild_status(current_guild);
	    cout << '\n';
	    cout << '\n';
	    Command cmd;
	    while (true)
	    {
		cmd = process_command(current_guild.name);
		Entity& us = env.registry.get_unit(cmd.id);
		if (us.group->guild_id == current_guild.guild_id && us.alive && us.turn)
		{
		    if (cmd.name == "check")
		    {
			env.board.path_trace(us);
			env.board.attack_range(us);
			print_unit_stats(us);
			plot_state(us, 0, static_cast<int>(env.board.get_map()[0].size()) - 1, 0, static_cast<int>(env.board.get_map().size()) - 1);
			continue;
		    }
		    print_unit_stats(us);
		    plot_state(us, 0, static_cast<int>(env.board.get_map()[0].size()) - 1, 0, static_cast<int>(env.board.get_map().size()) - 1);
		    break;
		}
		cout << current_guild.name << "'s phase. Select (1) alive units of (2) this guild only, (3) whose turn is still left."; 
	    }
	    Entity& us = env.registry.get_unit(cmd.id);
	    if (cmd.name == "move")
	    {
		env.board.move(us, cmd.coords);
	    }
	}
	team_id++;
    }
    cout << "Game over! " << survivor(env.guilds).name << " WINS" << '\n';
}
