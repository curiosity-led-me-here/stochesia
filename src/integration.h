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
#include <map_ascii.h>
#include <sstream>

extern bool phase_done(Guild& team);

extern int all_dead(Guild& team);

extern bool game_over(std::vector<Guild> guilds);

extern Guild survivor(std::vector<Guild> guilds);

extern Command process_command(std::string name);

class Environment
{
    private:
	maps::MapRecipe map_recipe;
	Mapmaker board;
	Registry registry;
    public:
	std::vector<Guild> guilds;
	Environment(const maps::TerrainMap& recipe);
	Environment(const maps::MapRecipe& recipe);

	// These expose existing game state for a separate renderer. They do not
	// duplicate state or alter movement, combat, Registry, or Mapmaker logic.
	const maps::MapRecipe& map_data() const;
	Mapmaker& map();
	const Mapmaker& map() const;
	Registry& units();
	const Registry& units() const;
	class ConfigureEnv
	{
	    private:
		Environment& env;
	    public:
		ConfigureEnv(Environment& env);
		void add_guild(std::string name, int id);
		Entity& add_entity(Entity unit, int id);
		void join_guild(Entity& unit, Guild& guild);
		void place_unit(Entity& unit, const std::vector<int> location);
	    };
	    class Game
	    {
		private:
		    Environment& env;
		    ConfigureEnv config;
		public:
		    Game(Environment& env);
		    void start();
	    };
	};
