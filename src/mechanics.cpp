#include "game_data.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>
#include <random>
#include "mechanics_ascii.h"
#include "pathfinder.h"

long seed = 1289428362;
std::mt19937 generator(seed);
bool random_binary(double probability, int seed)
{
    std::bernoulli_distribution distribution(probability);
    return distribution(generator);
}


std::vector<std::vector<int>> WeaponTriangle
{
    // S   L   Ax  B   An  Li  D   St
    { 0,  1, -1,  0,  0,  0,  0,  0 }, // SWORD
    {-1,  0,  1,  0,  0,  0,  0,  0 }, // LANCE
    { 1, -1,  0,  0,  0,  0,  0,  0 }, // AXE
    { 0,  0,  0,  0,  0,  0,  0,  0 }, // BOW
    { 0,  0,  0,  0,  0, -1,  1,  0 }, // ANIMA
    { 0,  0,  0,  0,  1,  0, -1,  0 }, // LIGHT
    { 0,  0,  0,  0, -1,  1,  0,  0 }, // DARK
    { 0,  0,  0,  0,  0,  0,  0,  0 }, // STAFF
};

std::vector<int> WeaponTriangleAdv(const Weapon& A, const Weapon& B)
{
    if (A.CAT == -1 ||  B.CAT == -1)
    {
	return {0, 0};
    }
    int BONUS_MT=1; int BONUS_HIT=15;
    int Bonus =  WeaponTriangle[A.CAT][B.CAT];
    return {Bonus*BONUS_MT, Bonus*BONUS_HIT};
}

int get_ATK_SPD(const Stats& As, const Weapon& Aw)
{
    int ATK_SPD = 0;
    if (Aw.WT > As.CON)
    {
	ATK_SPD += As.SPD + (As.CON - Aw.WT);
    }
    else if (Aw.WT <= As.CON)
    {
	ATK_SPD += As.SPD;
    }
    return ATK_SPD;
}

bool usability(const Weapon& Aw, const Entity& A, const Entity& B, Mapmaker& map) // Weapon rank <-> Entity rank
{
    bool out = false;
    for (WeaponCategory cat : A.type.UsableWeapons)
    {
	if (cat == Aw.CAT)
	{
	    std::vector<std::vector<int>> attack_path = map.standing_attack_range(A, Aw);
	    if(attack_path[B.location[1]][B.location[0]] == 1)
	    {
		out = true;
		return out;
	    }
	}
    }
    return out;
}


CombatInfo info(const Entity& A,const Entity& B, Mapmaker& map)
{
    Stats As = A.stats;
    Stats Bs = B.stats;
    bool b_can_counter = B.inventory.EquippedSlot >= 0 && B.inventory.EquippedSlot < 5 &&
    B.inventory.slot[B.inventory.EquippedSlot].usesRemaining > 0 && is_weapon(B.inventory.slot[B.inventory.EquippedSlot].ID);

    if (b_can_counter)
    {
	Weapon Bw = get_weapon(Armory, B.inventory.slot[B.inventory.EquippedSlot].ID);
	if (!usability(Bw, B, A, map))
	{
	    b_can_counter = false;
	}
    }
    
    if (A.inventory.EquippedSlot < 0 || A.inventory.EquippedSlot >= 5 || !is_weapon(A.inventory.slot[A.inventory.EquippedSlot].ID))
    {
	return {As.HP, 0, 0, false, 0, 0, b_can_counter};
    }
    
    Weapon Aw = get_weapon(Armory, A.inventory.slot[A.inventory.EquippedSlot].ID);

    if (!usability(Aw, A, B, map))
    {
	return {As.HP, 0, 0, false, 0, 0, b_can_counter};
    }
    
    std::vector<int> WTA;
    Weapon Bw;

    if (B.inventory.EquippedSlot < 0 || B.inventory.EquippedSlot >= 5 || !is_weapon(B.inventory.slot[B.inventory.EquippedSlot].ID))
    {
	Bw.DUR = 0; Bw.MT = 0; Bw.HIT = 0; Bw.WT = 0; Bw.CRIT = 0; Bw.MINRG = 0; Bw.MAXRG = 0;
	WTA = {0, 0};
    }

    else
    {
	Bw = get_weapon(Armory, B.inventory.slot[B.inventory.EquippedSlot].ID);
	WTA = WeaponTriangleAdv(Aw, Bw);
    }

    const bool magical =
    Aw.CAT == ANIMA || Aw.CAT == LIGHT || Aw.CAT == DARK;

    const int STR = magical ? As.MAG : As.STR;
    const int DEF = magical ? Bs.RES : Bs.DEF;
    
    // CALCULAIONS:
    
    int AVD = (Bs.SPD * 2) + Bs.LUC + terrain::get(B.terrain_id).avoid_bonus; // Bonus to add later
    int HIT = (As.SKL * 2) + (0.5 * As.LUC) - AVD + Aw.HIT + WTA[1]; // Bonus to add later
    int EFF_W_MT = (Aw.MT + WTA[0])*1; // effectiveness (*2) pending
    int MT = STR + EFF_W_MT - (DEF + terrain::get(B.terrain_id).defense_bonus); // Bonus pending
    int CRIT_EVADE_B = Bs.LUC + 0; // Bonus pending
    int CRIT = Aw.CRIT + (0.5 * As.SKL) - CRIT_EVADE_B + 0; // Bonus pending
    int ATK_SPD_A = get_ATK_SPD(As, Aw);
    int ATK_SPD_B = get_ATK_SPD(Bs, Bw);
    bool DB = (ATK_SPD_A - ATK_SPD_B) > 3 ? true : false;
    MT = (MT < 0)? 0 : MT;
    HIT = std::clamp(HIT, 0, 100);
    CRIT = std::clamp(CRIT, 0, 100);
    return {As.HP, MT, HIT, DB, CRIT, WTA[0], b_can_counter};
}

std::vector<CombatInfo> interact(const Entity& A,const Entity& B, Mapmaker& map)
{
    std::vector<CombatInfo> out;
    CombatInfo info_A = info(A, B, map);
    CombatInfo info_B = info(B, A, map);
    out.push_back(info_A);
    out.push_back(info_B);
    return out;
}

int attack_sequence(Entity& A, Entity& B, CombatInfo& A_perf, CombatInfo& B_perf)
{
    int out;
    if (A.inventory.EquippedSlot >= 0 && A.inventory.EquippedSlot < 5)
    {
	if (random_binary(A_perf.HIT / 100.0, seed))
	{
	    if (random_binary(A_perf.CRIT / 100.0, seed))
	    {
		std::cout << A.name << "'s turn.....";
		std::cout << "Critical hit! ";
		// (1) B Hp reduced (2) A weapon dur -1
		int temp = B.stats.HP;
		B.stats.HP -= (A_perf.MT*3);
		if (B.stats.HP < 0)
		{
		    B.stats.HP = 0;
		}
		if (B.stats.HP != temp)
		{
		    std::cout << "HP reduced from " << temp << " to " << B.stats.HP << "\n";
		}
		else
		{
		    std::cout << "No damage!" << "\n";
		}
		A.inventory.slot[A.inventory.EquippedSlot].usesRemaining --;
		if (A.inventory.slot[A.inventory.EquippedSlot].usesRemaining <= 0)
		{
		    A.inventory.EquippedSlot = -1;
		    std::cout << "Item broke!" << "\n";
		}
		out = 2;
		// exp calculation pending.
	    }
	    else
	    {
		// normal hit
		std::cout << A.name << "'s turn.....";
		std::cout << "Attack hit! ";
		// (1) B Hp reduced (2) A weapon dur -1
		int temp = B.stats.HP;
		B.stats.HP -= (A_perf.MT);
		if (B.stats.HP < 0)
		{
		    B.stats.HP = 0;
		}
		if (B.stats.HP != temp)
		{
		    std::cout << "HP reduced from " << temp << " to " << B.stats.HP << "\n";
		}
		else
		{
		    std::cout << "No damage!" << "\n";
		}
		A.inventory.slot[A.inventory.EquippedSlot].usesRemaining --;
		if (A.inventory.slot[A.inventory.EquippedSlot].usesRemaining <= 0)
		{
		    A.inventory.EquippedSlot = -1;
		    std::cout << "Item broke!" << "\n";
		}
		out = 1;
	    }
	}
	else
	{
	    // miss
	    std::cout << A.name << "'s turn.....";
	    std::cout << "Attack miss!" << "\n";
	    A.inventory.slot[A.inventory.EquippedSlot].usesRemaining --;
	    if (A.inventory.slot[A.inventory.EquippedSlot].usesRemaining <= 0)
	    {
		A.inventory.slot[A.inventory.EquippedSlot].ID = NO_ITEM;
		A.inventory.slot[A.inventory.EquippedSlot].usesRemaining = 0;
		A.inventory.EquippedSlot = -1;
		std::cout << "Item broke! Nothing selected" << "\n";
	    }
	    out = -1;
	}
    }
    else
    {
	out = -2;
    }
    return out;
    // 1 - Normal hit | 2 - crit | -1 - Miss | -2 - Nothing (unarmed) 
}

Entity* follow_up_attack(Entity& A, Entity& B, CombatInfo& A_perf,  CombatInfo& B_perf)
{
    if (A_perf.DB && B_perf.counter)
    {
	return &A;
    }
    else if (B_perf.DB && A_perf.counter)
    {
	return &B;
    }
    throw std::invalid_argument("Wrong filteration! follow_up_attack(Entity& A, Entity& B, CombatInfo& A_perf,  CombatInfo& B_perf) yet no one doubles!");
}

std::vector<sequence> battle(Entity& actor, Entity& defender, Mapmaker& map)
{
    std::vector<sequence> outcomes;
    std::vector<CombatInfo> out = interact(actor, defender, map);
    CombatInfo A_perf = out[0]; CombatInfo B_perf = out[1];
    if (!B_perf.counter)
    {
	return {};
    }
    outcomes.push_back({actor, attack_sequence(actor, defender, A_perf, B_perf), defender});
    if (defender.stats.HP <= 0)
    {
	map.death(defender);
	outcomes.push_back({defender, 0, actor});
    }
    if (actor.alive && defender.alive && A_perf.counter)
    {
	outcomes.push_back({defender, attack_sequence(defender, actor, B_perf, A_perf), actor});
	if (actor.stats.HP <= 0)
	{
	    map.death(actor);
	    outcomes.push_back({actor, 0, defender});
	}
    }
    if ((actor.alive && defender.alive) && (A_perf.DB || B_perf.DB))
    {
	Entity* follow_up = follow_up_attack(actor, defender, A_perf, B_perf);
	Entity& new_defender = (follow_up == &actor ? defender : actor);
	int outcome = attack_sequence(*follow_up, new_defender, (follow_up == &actor ? A_perf : B_perf), (follow_up == &actor ? B_perf : A_perf));
	outcomes.push_back({*follow_up, outcome, (follow_up == &actor ? defender : actor)});
	if (new_defender.stats.HP <= 0)
	{
	    map.death(new_defender);
	    outcomes.push_back({new_defender, 0, *follow_up});
	}
    }
    
    return outcomes;
    // if(doubles) --> if(hits) --> if(crits) --> (B.Hp - (A.MT*(1 || 3)*(1 || 2))) && (Aw.DUR - (1 || 2)) && (A.Hp - (B.MT*(1 || 3)*(1 || 2))) && (Bw.DUR - (1 || 2))
}

/*

void Heal(Entity& A, const int invslot)
{
    int item = A.inventory.slot[invslot].ID;
    Healer elixir = get_heal(HealingData, item);
    if (elixir.CAT == STAFF)
    {
	throw std::invalid_argument("Use Heal(Entity& Caster, Entity& A, const int id) instead");
    }
    if  (elixir.HEALHP == -1)
    {
	A.stats.HP = A.ogstats.HP;
    }
    else
    {
	A.stats.HP += elixir.HEALHP;
	if (A.stats.HP > A.ogstats.HP)
	{
	    A.stats.HP = A.ogstats.HP;
	}
    }
    A.inventory.slot[invslot].usesRemaining --;
}

void Heal(Entity& Caster, Entity& A, const int id)
{
    Healer staff = get_heal(HealingData, id);
    if (staff.CAT == NONETYPE)
    {
	Heal(A, id);
    }
    A.stats.HP += staff.HEALHP;
    if (A.stats.HP > A.ogstats.HP)
    {
	A.stats.HP = A.ogstats.HP;
    }
    Caster.inventory.slot[Caster.inventory.EquippedSlot].usesRemaining --;
    if (Caster.inventory.slot[Caster.inventory.EquippedSlot].usesRemaining == 0)
    {
	Caster.inventory.slot[Caster.inventory.EquippedSlot].ID = NO_ITEM;
	Caster.inventory.slot[Caster.inventory.EquippedSlot].usesRemaining = 0;
    }
}

*/
