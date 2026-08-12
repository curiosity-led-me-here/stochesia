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

bool game_over(std::vector<Guild> guilds)
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

Guild survivor(std::vector<Guild> guilds)
{
    for (Guild team : guilds)
    {
	if (all_dead(team) == 1)
	{
	    return team;
	}
    }
    throw std::invalid_argument("No team survived!");
}

Command process_command(std::string name)
{
    std::cout << name << "'s phase > ";
    std::string raw_str;
    std::getline(std::cin >> std::ws, raw_str);

    std::istringstream stream(raw_str);
    std::vector<std::string> out;
    std::string token;

    while (stream >> token)
    {
        out.push_back(token);
    }

    if (out.size() == 2 && out[0] == "check")
    {
	try
	{
	    return {out[0], std::stoi(out[1]), {}};
	}
	catch (const std::invalid_argument& e)
	{
	    std::cout << "Command must be {move id dx dy} or {check id}." << '\n';
	    return process_command(name);
	}
	catch (const std::out_of_range& e)
	{
	    std::cout << "Command must be {move id dx dy} or {check id}." << '\n';
	    return process_command(name);
	}
    }

    if (out.size() < 4 || out.size() > 4)
    {
	std::cout << "Command must be {move id dx dy} or {check id}." << '\n';
        return process_command(name);
    }

    try
    {
        std::string name = out[0];
        int id = std::stoi(out[1]);
        int x = std::stoi(out[2]);
        int y = std::stoi(out[3]);

	if (name != "move")
	{
	    std::cout << "Command must be {move id dx dy} or {check id}." << '\n';
	    return process_command(name);
	}
	
        return {name, id, {x, y}};
    }
    catch (const std::invalid_argument& e)
    {
	std::cout << "Command must be {move id dx dy} or {check id}." << '\n';
        return process_command(name);
    }
    catch (const std::out_of_range& e)
    {
	std::cout << "Command must be {move id dx dy} or {check id}." << '\n';
        return process_command(name);
    }
}

Environment::Environment(const maps::TerrainMap& recipe)
    : map_recipe(maps::gameplay_only(recipe)), board(map_recipe.terrain), registry()
{};

Environment::Environment(const maps::MapRecipe& recipe)
    : map_recipe(recipe), board(map_recipe.terrain), registry()
{};

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

void Environment::ConfigureEnv::add_guild(std::string name, int id)
{
    assert(id != 0);
    env.guilds.push_back(Guild{});
    Guild& team = env.guilds.back();
    team.name = name;
    team.guild_id = id;
}

Entity& Environment::ConfigureEnv::add_entity(Entity unit, int id)
{
    assert(id != 0);
    return env.registry.spawn(unit, id);
}

void Environment::ConfigureEnv::join_guild(Entity& unit, Guild& guild)
{
    guild.add(unit);
}

void Environment::ConfigureEnv::place_unit(Entity& unit, const std::vector<int> location)
{
    unit.location = location;
    env.board.place_unit(unit);
    env.board.path_trace(unit);
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
	    std::string raw_str;
	    const Guild& current_guild = env.guilds[team_id];
	    env.board.plot_with_units(env.registry);
	    print_guild_status(current_guild);
	    std::cout << '\n';
	    std::cout << '\n';
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
		std::cout << current_guild.name << "'s phase. Select (1) alive units of (2) this guild only, (3) whose turn is still left."; 
	    }
	    Entity& us = env.registry.get_unit(cmd.id);
	    if (cmd.name == "move")
	    {
		env.board.move(us, cmd.coords, env.registry);
	    }
	}
	team_id++;
    }
    std::cout << "Game over! " << survivor(env.guilds).name << " WINS" << '\n';
}
