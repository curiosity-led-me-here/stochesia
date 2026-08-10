#pragma once
#include <iostream>
#include <stdexcept>
#include <string>
#include "pathfinder.h"
#include "game_data.h"
#include "mechanics_ascii.h"
#include "mechanics.h"
#include "entity_registry.h"
#include "entity_data.h"
#include <cassert>
#include <vector>

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
    if (left == 1)
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

class Environment
{
    private:
	Mapmaker& board;
	Registry& registry;
	std::vector<Guild> guilds;
    public:
	Environment(const maps::TerrainMap& recipe);
	class ConfigureEnv
	{
	    private:
		Environment& env;
	    public:
		ConfigureEnv(Environment& env) : env(env) {};
		void add_guild(std::string name, int id)
		{
		    assert(id != 0);
		    env.guilds.push_back(Guild{});
		    Guild& team = env.guilds.back();
		    team.name = name;
		    team.guild_id = id;
		}
		
		Entity& add_entity(Entity unit, int id)
		{
		    assert(id != 0);
		    return env.registry.spawn(unit, id);
		}

		void place_unit(Entity& unit, const std::vector<int> location)
		{
		    unit.location = location;
		    env.board.place_unit(unit);
		}
	    };
	    class Game
	    {
		private:
		    Environment& env;
		    ConfigureEnv config;
		    bool game_over=false;
		public:
		    Game(Environment& env) : env(env), config(ConfigureEnv(env)) {};
		    void start()
		    {
			assert(env.guilds.size() > 1);
			int team_id = 0;
			while (!game_over)
			{
			    team_id = (team_id+1)%env.guilds.size();
			    std::cout << env.guilds[team_id+1].name << "'s PHASE" << '\n';
			    while (!phase_done(env.guilds[team_id]))
			    {
				//
			    }
			}
			std::cout << "Game over! " << survivor(env.guilds).name << " WINS" << '\n';
		    }
		    
	    };
	    
    
	};
