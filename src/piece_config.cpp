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

void setup(Entity& unit, std::vector<ItemID> id, Environment::ConfigureEnv& config, fe_tiles::AnimationRenderer& render, fe_tiles::UnitVisual visual)
{
    if (id.size() > 4)
    {
	throw std::invalid_argument("Too many weapons!");
    }
    for (int i=0; i < id.size(); i++)
    {
	unit.inventory.slot[i] = {id[i], get_dur(id[i])};
    }
    unit.inventory.slot[4] = {VULNERARY, 3};
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
	setup(P, {IRON_SWORD, IRON_LANCE, IRON_AXE}, config, render, fe_tiles::UnitVisual::Paladin);
	Entity& Arch = config.configure_entity(entities::neimi(), next_entity_id++, env.guilds[i]);
	setup(Arch, {IRON_SWORD, IRON_BOW, IRON_AXE}, config, render, fe_tiles::UnitVisual::Sniper);
    }
}

/*
void PieceSet::set2(fe_tiles::AnimationRenderer& render, Environment& env, Environment::ConfigureEnv& config)
{
    int guild_count = env.guilds.size();
    int next_entity_id = 1;
    for (int i=0; i < guild_count; i++)
    {
	Entity& P = config.configure_entity(entities::seth(), next_entity_id++, env.guilds[i]);
	setup(P, IRON_SWORD, config, render, fe_tiles::UnitVisual::Paladin);
	Entity& Q = config.configure_entity(entities::soldier(), next_entity_id++, env.guilds[i]);
	setup(Q, IRON_LANCE, config, render, fe_tiles::UnitVisual::Soldier);
	Entity& R = config.configure_entity(entities::neimi(), next_entity_id++, env.guilds[i]);
	setup(R, IRON_BOW, config, render, fe_tiles::UnitVisual::Sniper);
	Entity& S = config.configure_entity(entities::lute(), next_entity_id++, env.guilds[i]);
	setup(S, LIGHTNING, config, render, fe_tiles::UnitVisual::Sage);
	Entity& T = config.configure_entity(entities::lute(), next_entity_id++, env.guilds[i]);
	setup(T, THUNDER, config, render, fe_tiles::UnitVisual::Valkyrie);
	Entity& U = config.configure_entity(entities::vanessa(), next_entity_id++, env.guilds[i]);
	setup(U, IRON_LANCE, config, render, fe_tiles::UnitVisual::WyvernRider);
	Entity& V = config.configure_entity(entities::natasha(), next_entity_id++, env.guilds[i]);
	setup(V, LIGHTNING, config, render, fe_tiles::UnitVisual::Bishop);
	Entity& W = config.configure_entity(entities::joshua(), next_entity_id++, env.guilds[i]);
	setup(W, IRON_SWORD, config, render, fe_tiles::UnitVisual::Swordmaster);
	Entity& X = config.configure_entity(entities::garcia(), next_entity_id++, env.guilds[i]);
	setup(X, IRON_AXE, config, render, fe_tiles::UnitVisual::Berserker);
	Entity& Y = config.configure_entity(entities::colm(), next_entity_id++, env.guilds[i]);
	setup(Y, IRON_SWORD, config, render, fe_tiles::UnitVisual::Assassin);
	Entity& Z = config.configure_entity(entities::lute(), next_entity_id++, env.guilds[i]);
	setup(Z, FLUX, config, render, fe_tiles::UnitVisual::Druid);
	Entity& A = config.configure_entity(entities::artur(), next_entity_id++, env.guilds[i]);
	setup(A, FLUX, config, render, fe_tiles::UnitVisual::Summoner);
	Entity& B = config.configure_entity(entities::seth(), next_entity_id++, env.guilds[i]);
	setup(B, IRON_LANCE, config, render, fe_tiles::UnitVisual::EphraimLord);
	Entity& C = config.configure_entity(entities::joshua(), next_entity_id++, env.guilds[i]);
	setup(C, IRON_SWORD, config, render, fe_tiles::UnitVisual::Hero);
    }
}
*/
