#include "game_data.h"
#include "click.h"
#include "integration.h"
#include "entity_animation.h"
#include "map_monitor.h"
#include "maps.h"
#include <cstdlib>
#include "pathfinder.h"
using namespace std;

enum class ClickState
{
    Idle,
    UnitSelected,
    ChooseTarget,
    Animation,
    GameOver,
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
    int selected_weapon = -1;
    vector<sequence> sq;
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

    vector<vector<int>> move()
    {
	Entity& unit= env.units().get_unit(selected_id);
	vector<int> offset = {
	    env.cursor[0] - unit.location[0],
	    env.cursor[1] - unit.location[1]
	};
	vector<vector<int>> route = env.map().render_move(unit, offset);
	return route;
    }

    vector<sequence> attack(int hover_id)
    {
	Entity& unit = env.units().get_unit(selected_id);
	Entity& enemy= env.units().get_unit(hover_id);
	return battle(unit, enemy, env.map());
    }

    bool enemy_in_range(vector<vector<int>> standing_attack_range)
    {
	bool out = false;
	Entity& selected_unit = env.units().get_unit(selected_id);
	for (Entity* candidate : env.units().live_units())
	{
	    if (candidate == &selected_unit || candidate->group == selected_unit.group)
	    {
		continue;
	    }
	    const int x = candidate->location[0];
	    const int y = candidate->location[1];

	    if (standing_attack_range[y][x] == 1)
	    {
		out = true;
		return out;
	    }
	}
	return out;
    }

    bool enemy_in_range(vector<vector<int>> standing_attack_range, Entity& enemy)
    {
	bool out = false;
	Entity& selected_unit = env.units().get_unit(selected_id);
	Entity* candidate = &enemy;
	if (candidate == &selected_unit || candidate->group == selected_unit.group)
	{
	    return out;
	}
	const int x = candidate->location[0];
	const int y = candidate->location[1];

	if (standing_attack_range[y][x] == 1)
	{
	    out = true;
	    return out;
	}
	return out;
    }

    int eligible_weapon(int selected_id)
    {
	Entity& selected_unit = env.units().get_unit(selected_id);
	int new_slot = selected_unit.inventory.EquippedSlot;
	for (int offset=1; offset < 6; offset++)
	{
	    new_slot = (selected_unit.inventory.EquippedSlot+offset) % 5;
	    if (is_weapon(selected_unit.inventory.slot[new_slot].ID) && weapon_affinity(selected_unit, get_weapon(Armory, selected_unit.inventory.slot[new_slot].ID)) && enemy_in_range(env.map().standing_attack_range(selected_unit, get_weapon(Armory, selected_unit.inventory.slot[new_slot].ID))))
	    {
		return new_slot;
	    }
	}
	return new_slot;
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

	vector<CombatInfo> info = interact(unit, enemy, env.map());

	monitor.show_battle_forecast(
        unit,
        enemy,
        info[0],
        info[1]
	);
    }

    void update_inventory_at_cursor()
    {
	const int hover_id = env.map().entity_at(env.cursor);
	if (hover_id == 0)
	{
	    monitor.clear_inventory();
	    return;
	}
	if (selected_id != 0)
	{
	    Entity& selected_unit = env.units().get_unit(selected_id);
	    selected_unit.inventory.EquippedSlot = eligible_weapon(selected_id);
	}
	monitor.show_inventory(env.units().get_unit(hover_id));
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

	throw invalid_argument("Guild does not exist.");
    }

    int who_won()
    {
	for (Guild team : env.guilds)
	{
	    if (!all_dead(team))
	    {
		return team.guild_id;
	    }
	}
	return -2;
    }
    
    void end_phase()
    {
	if (check_end_phase())
	{
	    Guild& target = env.guilds[active_guild];
	    for (Entity* member : target.members)
	    {
		auto art = env.local_registry.at(member->entity_id);
		art.turn_greyscale(false);
		refresh();
	    }
	    active_guild = ((active_guild + 1) % env.guilds.size());
	    monitor.show_phase_intro(env.guilds[active_guild].name, render.guild_color(env.guilds[active_guild].guild_id));  
	}
    }

    void voluntary_end_phase()
    {
	render.clear_paint();
	monitor.request_redraw();
	Guild& target = env.guilds[active_guild];
	for (Entity* member : target.members)
	{
	    auto art = env.local_registry.at(member->entity_id);
	    if (art.is_turn_greyscale()) { art.turn_greyscale(false); }
	    refresh();
	}
	active_guild = ((active_guild + 1) % env.guilds.size());
	monitor.show_phase_intro(env.guilds[active_guild].name, render.guild_color(env.guilds[active_guild].guild_id)); 
    }

    void animate_battle(vector<sequence> sq, int i, Environment& env)
    {
	// 0: death, 1: hit, 2: crit, -1: miss
	sequence scene = sq[i];
	if (scene.turn == 0)
	{
	    return;
	}
	if (scene.turn == 2)
	{
	    env.local_registry.at(scene.unit.entity_id).critical(scene.opp, scene.opp_hp_after);
	}
	else if (scene.turn == 1)
	{
	    env.local_registry.at(scene.unit.entity_id).dash(scene.opp, scene.opp_hp_after);
	}
	else if (scene.turn == -2)
	{
	    env.local_registry.at(scene.unit.entity_id).wait(scene.opp);
	}
	else
	{
	    env.local_registry.at(scene.unit.entity_id).miss(scene.opp);
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
		update_inventory_at_cursor();
		env.map().update_attack_range(env.units().get_unit(selected_id));
		env.local_registry.at(selected_id).paint_red();
		state = ClickState::ChooseTarget;
	    }
	    monitor.request_redraw();
	}
	if (animation_state == 2)
	{
	    if (next_strike < static_cast<int>(sq.size()) &&
		next_strike != 0 &&
		inter_strike_pause < inter_strike_pause_frames)
	    {
		inter_strike_pause++;
		return;
	    }

	    if (next_strike < sq.size())
	    {
		const sequence& next_scene = sq[next_strike];
		if (next_scene.turn == 0)
		{
		    env.local_registry.at(next_scene.unit.entity_id).death();
		}
		else
		{
		    animate_battle(sq, next_strike, env);
		}
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
	    if (game_over())
	    {
		cout << "\nGame over\n";
		state = ClickState::GameOver;
	    }
	    return;
	}
    }

    void show_inventory()
    {
	int selected_id = env.map().entity_at(env.cursor);
	Entity& selected_unit = env.units().get_unit(selected_id);
	if (selected_id != 0 && belongs(selected_id) && !env.local_registry.at(selected_id).is_turn_greyscale())
	{
	    int new_slot = (selected_unit.inventory.EquippedSlot+1) % 5;
	    selected_unit.inventory.EquippedSlot = new_slot;
	    monitor.show_inventory(selected_unit);
	}
	else
	{
	    monitor.show_inventory(selected_unit);
	}
    }

    bool weapon_affinity(Entity& selected_unit, Weapon weapon)
    {
	bool out = false;
	for (WeaponCategory cat : selected_unit.type.UsableWeapons)
	{
	    if (cat == weapon.CAT)
	    {
		out = true;
		return out;
	    }
	}
	return out;
    }

    bool enemy_in_range()
    {
	bool out = false;
	Entity& selected_unit = env.units().get_unit(selected_id);
	for (Entity* candidate : env.units().live_units())
	{
	    if (candidate == &selected_unit || candidate->group == selected_unit.group)
	    {
		continue;
	    }
	    const int x = candidate->location[0];
	    const int y = candidate->location[1];

	    if (selected_unit.attack_range[y][x] == 1)
	    {
		out = true;
		return out;
	    }
	}
	return out;
    }
    
    void e()
    {
	Entity& selected_unit = env.units().get_unit(selected_id);
	selected_unit.inventory.EquippedSlot = eligible_weapon(selected_id);
	monitor.show_inventory(selected_unit);
	env.local_registry.at(selected_id).clear_paint();
	Weapon weapon = get_weapon(Armory, selected_unit.inventory.slot[selected_unit.inventory.EquippedSlot].ID);
	for (WeaponCategory cat : selected_unit.type.UsableWeapons)
	{
	    if (cat == weapon.CAT)
	    {
		selected_unit.attack_range = env.map().standing_attack_range(selected_unit, weapon);
		if (enemy_in_range())
		{
		    env.local_registry.at(selected_id).paint_red();
		}
		monitor.request_redraw();
	    }
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
			unit.inventory.EquippedSlot = eligible_weapon(selected_id);
			Entity& enemy = env.units().get_unit(enemy_idx);
			vector<CombatInfo> info = interact(unit, enemy, env.map());
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
			vector<vector<int>> route = move();
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
	    case ClickState::GameOver:
		{
		    return;
		}
	    
	    }
	    
	}
};

void wasd_functions(Environment& env, const fe_tiles::AnimationRenderer& render, fe_tiles::MapMonitor& monitor, ClickController click)
{
    monitor.set_cursor(env.cursor);
    click.update_inventory_at_cursor();
    click.update_forecast_at_cursor();
    if (env.map().entity_at(env.cursor) == 0)
    {
	monitor.show_terrain_stats(env.map().get_map()[env.cursor[1]][env.cursor[0]]);
    }
    if (click.state == ClickState::ChooseTarget)
    {
	monitor.show_inventory(env.units().get_unit(click.selected_id));
    }
}


void run_click_game(Environment& env, fe_tiles::AnimationRenderer& render, maps::MapRecipe& recipe)
{
    fe_tiles::MapMonitor monitor(recipe, render);
    monitor.show_phase_intro(env.guilds[0].name, render.guild_color(env.guilds[0].guild_id));
    monitor.set_battle_animation_speed(0.69);
    monitor.set_cursor(env.cursor);
    ClickController click(env, render, monitor);
    click.update_inventory_at_cursor();
    if (env.map().entity_at(env.cursor) == 0)
    {
	monitor.show_terrain_stats(env.map().get_map()[env.cursor[1]][env.cursor[0]]);
    }
    monitor.on_key([&](char key)
    {
	if (key == '\r')
	{
	    click.enter();
	}
	
	if (key == 'w')
	{
	    env.move_up();
	    wasd_functions(env, render, monitor, click);
	}
	if (key == 'a')
	{
	    env.move_left();
	    wasd_functions(env, render, monitor, click);
	}
	if (key == 's')
	{
	    env.move_down();
	    wasd_functions(env, render, monitor, click);
	}
	if (key == 'd')
	{
	    env.move_right();
	    wasd_functions(env, render, monitor, click);
	}
	if (key == 'e')
	{
	    if (click.state != ClickState::ChooseTarget)
	    {
		click.show_inventory();
	    }
	    else if (click.state == ClickState::ChooseTarget && env.map().entity_at(env.cursor) != 0 && env.map().entity_at(env.cursor) != click.selected_id)
	    {
		Entity& unit = env.units().get_unit(click.selected_id);
		const int slot = click.eligible_weapon(click.selected_id);
		if (slot < 0 || slot >= 5 || !is_weapon(unit.inventory.slot[slot].ID))
		{
		    monitor.clear_battle_forecast();
		    return;
		}
		if (click.weapon_affinity(unit, get_weapon(Armory, unit.inventory.slot[slot].ID)) &&  click.enemy_in_range(env.map().standing_attack_range(unit, get_weapon(Armory, unit.inventory.slot[slot].ID)), env.units().get_unit(env.map().entity_at(env.cursor))))
		{
		    int enemy_id = env.map().entity_at(env.cursor);
		    Entity& enemy = env.units().get_unit(enemy_id);
		    unit.inventory.EquippedSlot = slot;
		    vector<CombatInfo> info = interact(unit, enemy, env.map());
		    monitor.clear_battle_forecast();
		    monitor.show_battle_forecast(unit, enemy, info[0], info[1]);   
		}
	    }
	    
	    else if (click.state == ClickState::ChooseTarget && env.map().entity_at(env.cursor) == click.selected_id)
	    {
		Entity& unit = env.units().get_unit(click.selected_id);
		const int slot = unit.inventory.EquippedSlot;
		if (slot < 0 || slot >= 5 || !is_weapon(unit.inventory.slot[slot].ID))
		{
		    monitor.clear_battle_forecast();
		    return;
		}
		click.e();
	    }
	    else
	    {
		return;
	    }
	}
	if (key == 'p')
	{
	    click.voluntary_end_phase();
	}
    });
    
    monitor.on_frame([&click, &monitor]
    {
	click.animation_end();
    });
    monitor.run();
}
