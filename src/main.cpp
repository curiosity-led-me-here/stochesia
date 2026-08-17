#include "game_data.h"
#include "integration.h"
#include "entity_data.h"
#include "entity_animation.h"
#include "map_monitor.h"
#include "maps.h"

void animate_battle(std::vector<sequence> sq, int i, Environment& env)
{
    // 0: death, 1: hit, 2: crit, -1: miss
    sequence scene = sq[i];
    if (scene.turn == 0)
    {
	return;
    }
    if (i != sq.size()-1)
    {
	sequence next_seq = sq[i+1];
	if (next_seq.turn == 0)
	{
	    env.local_registry.at(scene.unit.entity_id).dash(scene.opp, false, true);
	}
	else
	{
	    if (scene.turn == 1 || scene.turn == 2)
	    {
		
		env.local_registry.at(scene.unit.entity_id).dash(scene.opp);
	    }
	    else
	    {
		env.local_registry.at(scene.unit.entity_id).miss(scene.opp);

	    }
	}
    }
    else
    {
	if (scene.turn == 1 || scene.turn == 2)
	{
	    env.local_registry.at(scene.unit.entity_id).dash(scene.opp);
	}
	else
	{
	    env.local_registry.at(scene.unit.entity_id).miss(scene.opp);
	}
    }
}

int main()
{
    maps::MapRecipe recipe = maps::chapter_1();
    Environment env(recipe);
    Environment::ConfigureEnv config(env);

    config.add_guild("Blue", 1);
    config.add_guild("Red", 2);
    Entity& Seth = config.configure_entity(entities::seth(), 1, env.guilds[0], {2, 2});
    Entity& Joshua = config.configure_entity(entities::joshua(), 2, env.guilds[1], {3, 3});
    Seth.inventory.slot[0] = {IRON_LANCE, 45};
    Joshua.inventory.slot[0] = {IRON_SWORD, 45};
    Seth.inventory.EquippedSlot = 0;
    Joshua.inventory.EquippedSlot = 0;

    fe_tiles::AnimationRenderer render;
    render.load_map(env.map());
    render.set_guild_color(
    env.guilds[0],
    0x48D848
    );
    render.set_guild_color(
    env.guilds[1],
    fe_tiles::GuildColor::enemy()
    );

    config.configure_render(Seth, render, fe_tiles::UnitVisual::Paladin);
    config.configure_render(Joshua, render, fe_tiles::UnitVisual::Myrmidon);
    render.sync_units({&Seth, &Joshua});
    fe_tiles::MapMonitor monitor(recipe, render);
    monitor.set_cursor(env.cursor);
    int selected_id = 0;
    bool battle_mode = false;
    bool attack_mode = false;
    bool flag = true;
    int next_strike = 0;
    std::vector<sequence> sq;
    bool path_selected = false;
    bool scout_mode = false;
    monitor.on_frame([&]
    {
	if (render.is_busy())
	{
            return;
	}

	if (next_strike < sq.size())
	{
            animate_battle(sq, next_strike, env);
            next_strike++;
            return;
	}

	if (!sq.empty())
	{
            sq.clear();
            next_strike = 0;
            battle_mode = false;
            selected_id = 0;
            render.sync_units({&Seth, &Joshua});
            monitor.request_redraw();
	}
    });

    monitor.on_key([&] (char key)
    {
	if (key == '\r')
	{
	    if (env.map().entity_at(env.cursor) == 0 && flag)
	    {
		//env.local_registry.at(selected_id).clear_paint();
		battle_mode = false;
		flag = true;
		attack_mode = false;
	    }
	    
	    else if (battle_mode)
	    {
		if (env.map().entity_at(env.cursor) != 0)
		{
		    int entity_id = env.map().entity_at(env.cursor);
		    if (selected_id == entity_id)
		    {
			env.local_registry.at(selected_id).clear_paint();
			battle_mode = false;
			selected_id = 0;
			scout_mode = false;
			attack_mode = false;
		    }
		    else
		    {
			Entity& unit = env.units().get_unit(selected_id);
			Entity& enemy= env.units().get_unit(entity_id);
			unit.inventory.EquippedSlot = 0;
			std::vector<CombatInfo> info = interact(unit, enemy);
			mechanics_ascii::interact_window(unit, enemy, info[0], info[1]);
			sq = battle(unit, enemy, env.map());
			next_strike=0;
			render.sync_units({&Seth, &Joshua});
			env.local_registry.at(selected_id).clear_paint();
			selected_id = 0;
			battle_mode = false;
		    }
		}
		else
		{
		    //env.local_registry.at(selected_id).clear_paint();
		}
	    }
	    
	    // select unit for scout
	    else if (env.map().entity_at(env.cursor) != 0 && !attack_mode)
	    {
		flag = false;
		scout_mode = true;
		int entity_id = env.map().entity_at(env.cursor);
		selected_id = entity_id;
		Entity& unit= env.units().get_unit(entity_id);
		env.map().path_trace(unit);
		env.map().attack_range(unit);
		env.local_registry.at(entity_id).paint_blue();
		env.local_registry.at(entity_id).paint_red();
		if (!env.map().prompt_attack(unit).empty()) { attack_mode = true; }
	    }
s
	    else if (env.map().entity_at(env.cursor) == selected_id && scout_mode)
	    {
		env.local_registry.at(selected_id).clear_paint();
		flag = true;
		scout_mode = false;
	    }
	    
	    else if (env.map().entity_at(env.cursor) != 0 && attack_mode)
	    {
		env.local_registry.at(selected_id).clear_paint();
		int entity_id = env.map().entity_at(env.cursor);
		Entity& unit= env.units().get_unit(entity_id);
		env.map().update_attack_range(unit);
		env.local_registry.at(entity_id).paint_red();
		battle_mode = true;
		attack_mode = false;
		scout_mode = false;
		monitor.request_redraw();
	    }
	    
	    // move
	    else if (selected_id != 0 && render.blue_tiles()[env.cursor[1]][env.cursor[0]] != -1)
	    {
		Entity& unit= env.units().get_unit(selected_id);
		std::vector<int> offset = {
		    env.cursor[0] - unit.location[0],
		    env.cursor[1] - unit.location[1]
		};
		std::vector<std::vector<int>> route = env.map().render_move(unit, offset, env.units());
		render.sync_units({&Seth, &Joshua});
		env.local_registry.at(selected_id).clear_paint();
		bool started = env.local_registry.at(selected_id).play_committed_move(route);
		std::vector<avl_for_atk> targets = env.map().prompt_attack(unit);
		std::cout << "targets: " << targets.size() << '\n';

		if (!targets.empty())
		{
		    env.map().update_attack_range(unit);
		    env.local_registry.at(selected_id).paint_red();
		    battle_mode = true;
		    monitor.request_redraw();
		}
		else
		{
		    monitor.request_redraw();
		    selected_id = 0;
		    scout_mode = false;
		    flag = true;
		}	
	    }
	}

	if (key == 'w')
	{
	    env.move_up();
	    monitor.set_cursor(env.cursor);
	}
	if (key == 'a')
	{
	    env.move_left();
	    monitor.set_cursor(env.cursor);
	}
	if (key == 's')
	{
	    env.move_down();
	    monitor.set_cursor(env.cursor);
	}
	if (key == 'd')
	{
	    env.move_right();
	    monitor.set_cursor(env.cursor);
	}
	
    });
    monitor.run();
    return 0;
}
