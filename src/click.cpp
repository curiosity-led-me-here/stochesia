#include "game_data.h"
#include "integration.h"
#include "entity_data.h"
#include "entity_animation.h"
#include "map_monitor.h"
#include "maps.h"
#include <cstdlib>

enum class ClickState
{
    Idle,
    UnitSelected,
    ChooseTarget,
    Animation,    
};

struct ClickController
{
    ClickController(Environment& environment,
    fe_tiles::AnimationRenderer& renderer,
    fe_tiles::MapMonitor& map_monitor)
    : env(environment), render(renderer), monitor(map_monitor) {}
    ClickState state = ClickState::Idle;
    int selected_id = 0;
    int animation_state = 0;
    int next_strike = 0;
    static constexpr int inter_strike_pause_frames = 0x14;
    int inter_strike_pause = 0;
    int active_guild=0;
    int previous_guild=-1;
    std::vector<sequence> sq;
    Environment& env;
    fe_tiles::AnimationRenderer& render;
    fe_tiles::MapMonitor& monitor;
 
    void refresh()
    {
	state = ClickState::Idle;
	selected_id = 0;
	animation_state = 0;
	next_strike = 0;
	inter_strike_pause = 0;
    }

    std::vector<std::vector<int>> move()
    {
	Entity& unit= env.units().get_unit(selected_id);
	std::vector<int> offset = {
	    env.cursor[0] - unit.location[0],
	    env.cursor[1] - unit.location[1]
	};
	std::vector<std::vector<int>> route = env.map().render_move(unit, offset, env.units());
	return route;
    }

    std::vector<sequence> attack(int hover_id)
    {
	Entity& unit = env.units().get_unit(selected_id);
	Entity& enemy= env.units().get_unit(hover_id);
	unit.inventory.EquippedSlot = 0;
	return battle(unit, enemy, env.map());
    }

    void update_forecast_at_cursor()
    {
	if (state != ClickState::ChooseTarget)
	{
            return;
	}

	int hover_id = env.map().entity_at(env.cursor);
	bool red = render.red_tiles()[env.cursor[1]][env.cursor[0]] != 0;

	if (hover_id == 0 || hover_id == selected_id || !red)
	{
            monitor.clear_battle_forecast();
            return;
	}

	Entity& unit = env.units().get_unit(selected_id);
	Entity& enemy = env.units().get_unit(hover_id);

	std::vector<CombatInfo> info = interact(unit, enemy);

	monitor.show_battle_forecast(
        unit,
        enemy,
        info[0],
        info[1]
	);
    }
    
    bool belongs(int selected_id)
    {
	bool is_true = false;
	Guild& target = env.guilds[active_guild];
	for (Entity* member : target.members)
	{
	    if (member->entity_id == selected_id) { is_true = true; }
	}
	return is_true;
    }

    bool check_end_phase()
    {
	bool is_end = true;
	Guild& target = env.guilds[active_guild];
	for (Entity* member : target.members)
	{
	    auto art = env.local_registry.at(member->entity_id);
	    is_end = is_end && art.is_turn_greyscale();
	}
	return is_end;
    }

    void end_turn(int id)
    {
	auto art = env.local_registry.at(id);
	art.turn_greyscale();
	refresh();
	end_phase();
    }

    bool game_over()
    {
	return ::game_over(env.guilds);
    }

    Guild& get_guild_by_id(int id)
    {
	for (Guild& guild : env.guilds)
	{
            if (guild.guild_id == id)
            {
		return guild;
            }
	}

	throw std::invalid_argument("Guild does not exist.");
    }

    int who_won()
    {
	for (Guild team : env.guilds)
	{
	    if (!all_dead(team))
	    {
		return team.guild_id;
	    }
	    else
	    {
		return -1;
	    }
	}
	return -2;
    }
    
    void end_phase()
    {
	if (check_end_phase())
	{
	    if (previous_guild == -1)
	    {
		previous_guild = active_guild;
		active_guild = ((active_guild + 1) % env.guilds.size());
	    }
	    else
	    {
		Guild& target = env.guilds[previous_guild];
		for (Entity* member : target.members)
		{
		    auto art = env.local_registry.at(member->entity_id);
		    art.turn_greyscale(false);
		    refresh();
		}
		previous_guild = active_guild;
		active_guild = ((active_guild + 1) % env.guilds.size());
	    }
	}
    }

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
		if (scene.turn == 2)
		{
		    env.local_registry.at(scene.unit.entity_id).critical(scene.opp, scene.opp_hp_after, false, true);
		}
		else if (scene.turn == 1)
		{
		    env.local_registry.at(scene.unit.entity_id).dash(scene.opp, scene.opp_hp_after, false, true);
		}
		else
		{
		    env.local_registry.at(scene.unit.entity_id).miss(scene.opp);
		}
	    }
	    else
	    {
		if (scene.turn == 2)
		{
		    env.local_registry.at(scene.unit.entity_id).critical(scene.opp, scene.opp_hp_after);
		}
		else if (scene.turn == 1)
		{
		    env.local_registry.at(scene.unit.entity_id).dash(scene.opp, scene.opp_hp_after);
		}
		else
		{
		    env.local_registry.at(scene.unit.entity_id).miss(scene.opp);

		}
	    }
	}
	else
	{
	    if (scene.turn == 2)
	    {
		env.local_registry.at(scene.unit.entity_id).critical(scene.opp, scene.opp_hp_after);
	    }
	    else if (scene.turn == 1)
	    {
		env.local_registry.at(scene.unit.entity_id).dash(scene.opp, scene.opp_hp_after);
	    }
	    else
	    {
		env.local_registry.at(scene.unit.entity_id).miss(scene.opp);
	    }
	}
    }
    
    void animation_end()
    {
	if (state != ClickState::Animation || render.is_busy())
	{
	    return;
	}
	if (animation_state == 1)
	{
	    if(env.map().prompt_attack(env.units().get_unit(selected_id)).empty())
	    {
		end_turn(selected_id);
	    }
	    else
	    {
		Entity& unit = env.units().get_unit(selected_id);
		int enemy_idx = env.map().entity_at(env.map().prompt_attack(env.units().get_unit(selected_id))[0].coords);
		
		Entity& enemy = env.units().get_unit(enemy_idx);
		std::vector<CombatInfo> info = interact(unit, enemy);
		monitor.show_battle_forecast(unit, enemy, info[0], info[1]);
		env.map().update_attack_range(env.units().get_unit(selected_id));
		env.local_registry.at(selected_id).paint_red();
		state = ClickState::ChooseTarget;
	    }
	    monitor.request_redraw();
	}
	if (animation_state == 2)
	{
	    // A terminal turn == 0 record only says that the preceding strike
	    // killed its target. Its death fade was already started at impact.
	    while (next_strike < static_cast<int>(sq.size()) &&
		   sq[next_strike].turn == 0)
	    {
		++next_strike;
	    }

	    // FE8's default map-combat script sleeps for 0x14 frames after a
	    // map-unit has returned from its strike, before the next round starts.
	    if (next_strike != 0 &&
		inter_strike_pause < inter_strike_pause_frames)
	    {
		inter_strike_pause++;
		return;
	    }

	    if (next_strike < sq.size())
	    {
		animate_battle(sq, next_strike, env);
		next_strike++;
		inter_strike_pause = 0;
		return;
	    }
	    else
	    {
		monitor.clear_battle_forecast();
		render.sync_units(env.units().live_units());
		end_turn(selected_id);
	    }
	    monitor.request_redraw();
	}
    }
    
    void enter()
    {
	bool blue;
	bool red;
	int hover_id = env.map().entity_at(env.cursor);

	if (render.blue_tiles()[env.cursor[1]][env.cursor[0]] == -1) { blue = false; }
	else { blue = true; }
	if (render.red_tiles()[env.cursor[1]][env.cursor[0]] == 1) { red = true; }
	else { red = false; }
	
	switch (state)
	{
	    case ClickState::Idle:
		{
		    if (hover_id == 0)
		    {
			return;
		    }
		    if (!env.local_registry.at(hover_id).is_turn_greyscale())
		    {
			selected_id = hover_id;
			state = ClickState::UnitSelected;
		    }
		    else {return; }
		    return;
		}

	    case ClickState::UnitSelected:
		{
		    if ((hover_id == selected_id && !blue && !red && !env.local_registry.at(hover_id).is_turn_greyscale()))
		    {
			int entity_id = env.map().entity_at(env.cursor);
			selected_id = entity_id;
			Entity& unit= env.units().get_unit(entity_id);
			env.map().path_trace(unit);
			env.map().attack_range(unit);
			env.local_registry.at(entity_id).paint_blue();
			env.local_registry.at(entity_id).paint_red();
			break;
		    }

		    else if (!env.map().prompt_attack(env.units().get_unit(selected_id)).empty() && hover_id == selected_id && belongs(selected_id) && !env.local_registry.at(selected_id).is_turn_greyscale())
		    {
			Entity& unit = env.units().get_unit(selected_id);
			int enemy_idx = env.map().entity_at(env.map().prompt_attack(env.units().get_unit(selected_id))[0].coords);
			
			Entity& enemy = env.units().get_unit(enemy_idx);
			std::vector<CombatInfo> info = interact(unit, enemy);
			monitor.show_battle_forecast(unit, enemy, info[0], info[1]);
			env.local_registry.at(selected_id).clear_paint();
			env.map().update_attack_range(env.units().get_unit(selected_id));
			env.local_registry.at(selected_id).paint_red();
			state = ClickState::ChooseTarget;
		    }

		    else if (blue && red && hover_id == selected_id && !env.local_registry.at(selected_id).is_turn_greyscale())
		    {
			env.local_registry.at(selected_id).clear_paint();
			refresh();
			return;
		    }

		    if (hover_id != 0 && hover_id != selected_id && !env.local_registry.at(hover_id).is_turn_greyscale())
		    {
			env.local_registry.at(selected_id).clear_paint();

			selected_id = hover_id;
			Entity& unit = env.units().get_unit(selected_id);

			env.map().path_trace(unit);
			env.map().attack_range(unit);

			env.local_registry.at(selected_id).paint_blue();
			env.local_registry.at(selected_id).paint_red();

			monitor.clear_battle_forecast();
			monitor.request_redraw();
			return;
		    }
		    
		    else if (hover_id == 0 && blue && belongs(selected_id) && !env.local_registry.at(selected_id).is_turn_greyscale())
		    {
			std::vector<std::vector<int>> route = move();
			render.sync_units(env.units().live_units());
			env.local_registry.at(selected_id).clear_paint();
			bool started = env.local_registry.at(selected_id).play_committed_move(route);
			animation_state = 1;
			state = ClickState::Animation;
			break;	
		    }
		    else
		    {
			return;
		    }		    
		}
		
	    case ClickState::ChooseTarget:
		{
		    if (hover_id != 0 && hover_id == selected_id)
		    {
			env.local_registry.at(selected_id).clear_paint();
			monitor.clear_battle_forecast();
			end_turn(hover_id);
			monitor.request_redraw();
	    }
	    if (hover_id != 0 && hover_id != selected_id && red && belongs(selected_id))
	    {
		// battle() resolves HP synchronously. Save the currently visible HP
		// before that happens; each sequence releases its own HP at impact.
		render.sync_units(env.units().live_units());
		sq = attack(hover_id);
		next_strike=0;
		inter_strike_pause=0;
		env.local_registry.at(selected_id).clear_paint();
			animation_state = 2;
			state = ClickState::Animation;
			break;
		    }
		    
		    else { return; }
		}
		
	    case ClickState::Animation:
		{
		    return;
		}
	    
	    }
	    
	}
};


int main()
{
    
    maps::MapRecipe recipe = maps::chapter_5();
    Environment env(recipe);
    Environment::ConfigureEnv config(env);

    config.add_guild("Blue", 1);
    config.add_guild("Red", 2);
    Entity& Seth = config.configure_entity(entities::seth(), 1, env.guilds[0], {2, 2});
    Entity& Joshua = config.configure_entity(entities::joshua(), 2, env.guilds[1], {3, 3});
    Entity& Billy = config.configure_entity(entities::garcia(), 3, env.guilds[1], {4, 3});
    Seth.inventory.slot[0] = {IRON_LANCE, 45};
    Joshua.inventory.slot[0] = {IRON_SWORD, 45};
    Billy.inventory.slot[0] = {IRON_AXE, 45};
    Seth.inventory.EquippedSlot = 0;
    Joshua.inventory.EquippedSlot = 0;
    Billy.inventory.EquippedSlot = 0;

    fe_tiles::AnimationRenderer render;
    render.load_map(env.map());
    render.set_guild_color(
    env.guilds[0],
    fe_tiles::GuildColor::player()
    );
    render.set_guild_color(
    env.guilds[1],
    fe_tiles::GuildColor::enemy()
    );
    render.set_guild_color(
    env.guilds[1],
    fe_tiles::GuildColor::enemy()
    );

    config.configure_render(Seth, render, fe_tiles::UnitVisual::WyvernLordF);
    config.configure_render(Joshua, render, fe_tiles::UnitVisual::SwordmasterF);
    config.configure_render(Billy, render, fe_tiles::UnitVisual::Berserker);
    render.sync_units(env.units().live_units());
    fe_tiles::MapMonitor monitor(recipe, render);
    monitor.set_battle_animation_speed(0.69);
    monitor.set_cursor(env.cursor);
    ClickController click(env, render, monitor);
    monitor.on_key([&](char key)
    {
	if (key == '\r')
	{
	    click.enter();
	}
	
	if (key == 'w')
	{
	    env.move_up();
	    monitor.set_cursor(env.cursor);
	    click.update_forecast_at_cursor();
	}
	if (key == 'a')
	{
	    env.move_left();
	    monitor.set_cursor(env.cursor);
	    click.update_forecast_at_cursor();
	}
	if (key == 's')
	{
	    env.move_down();
	    monitor.set_cursor(env.cursor);
	    click.update_forecast_at_cursor();
	}
	if (key == 'd')
	{
	    env.move_right();
	    monitor.set_cursor(env.cursor);
	    click.update_forecast_at_cursor();
	}
    });
    
    monitor.on_frame([&click]
    {
	click.animation_end();
    });
    
    monitor.run();
    if (click.game_over())
    {
	monitor.close();
	if (click.who_won() < 0)
	{
	    std::cout << "No one won! Tied.";
	}
	std::cout << click.get_guild_by_id(click.who_won()).name << " Won!";
    }
    return 0;
}
