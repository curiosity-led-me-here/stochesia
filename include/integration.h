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
#include "entity_animation.h"
#include "map_monitor.h"
#include "maps.h"

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
	std::vector<int> cursor;
	std::vector<Guild> guilds;
	std::unordered_map<int, fe_tiles::EntityAnimation> local_registry;
	Environment(const maps::TerrainMap& recipe);
	Environment(const maps::MapRecipe& recipe);
	void move_up();
	void move_down();
	void move_right();
	void move_left();
	int detect_unit();
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
		Entity& configure_entity(Entity unit, int id, Guild& guild);
		void configure_entity_location(Entity& out_unit, const std::vector<int> location);
		void configure_render(Entity& unit, fe_tiles::AnimationRenderer& render, fe_tiles::UnitVisual);
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
