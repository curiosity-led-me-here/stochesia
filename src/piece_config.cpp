#include "game_data.h"
#include "integration.h"
#include "entity_data.h"
#include "entity_animation.h"
#include "piece_config.h"
#include <cstdlib>

int get_dur(int id)
{
    return get_weapon(Armory, id).DUR;
}

void setup(Entity& unit, ItemID id, Environment::ConfigureEnv& config, fe_tiles::AnimationRenderer& render, fe_tiles::UnitVisual visual)
{
    unit.inventory.slot[0] = {id, get_dur(id)};
    unit.inventory.slot[1] = {VULNERARY, 3};
    unit.inventory.EquippedSlot = 0;
    config.configure_render(unit, render, visual);
}


void PieceSet::set1(fe_tiles::AnimationRenderer& render, Environment& env, Environment::ConfigureEnv& config)
{
    int guild_count = env.guilds.size();
    int next_entity_id = 1;
    for (int i=0; i < guild_count; i++)
    {
	Entity& P = config.configure_entity(entities::seth(), next_entity_id++, env.guilds[i]);
	setup(P, IRON_SWORD, config, render, fe_tiles::UnitVisual::Paladin);
	Entity& Soldier1 = config.configure_entity(entities::soldier(), next_entity_id++, env.guilds[i]);
	setup(Soldier1, IRON_LANCE, config, render, fe_tiles::UnitVisual::Soldier);
	Entity& Soldier2 = config.configure_entity(entities::soldier(), next_entity_id++, env.guilds[i]);
	setup(Soldier2, IRON_LANCE, config, render, fe_tiles::UnitVisual::Soldier);
	Entity& Soldier3 = config.configure_entity(entities::soldier(), next_entity_id++, env.guilds[i]);
	setup(Soldier3, IRON_LANCE, config, render, fe_tiles::UnitVisual::Soldier);
	Entity& Soldier4 = config.configure_entity(entities::soldier(), next_entity_id++, env.guilds[i]);
	setup(Soldier4, IRON_LANCE, config, render, fe_tiles::UnitVisual::Soldier);
	Entity& Soldier5 = config.configure_entity(entities::soldier(), next_entity_id++, env.guilds[i]);
	setup(Soldier5, IRON_LANCE, config, render, fe_tiles::UnitVisual::Soldier);
	Entity& Arch = config.configure_entity(entities::neimi(), next_entity_id++, env.guilds[i]);
	setup(Arch, IRON_BOW, config, render, fe_tiles::UnitVisual::Sniper);
    }
}
