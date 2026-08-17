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

CombatInfo info(const Entity& A,const Entity& B)
{
    Stats As = A.stats;
    Stats Bs = B.stats;
    
    if (A.inventory.EquippedSlot < 0)
    {
	return {As.HP, 0, 0, false, 0};
    }
    else
    {
	Weapon Aw = get_weapon(Armory, A.inventory.slot[A.inventory.EquippedSlot].ID);
	Weapon Bw = get_weapon(Armory, B.inventory.slot[B.inventory.EquippedSlot].ID);
	std::vector<int> WTA = WeaponTriangleAdv(Aw, Bw);
	/*
	if (WTA[0] > 0)
	{
	    std::cout << "Weapon triangle advantage to " << A.name;
	}
	else if (WTA[0] < 0)
	{
	    std::cout << "Weapon triangle advantage to " << B.name;
	}
	*/
	int AVD = (As.SPD * 2) + As.LUC; // Bonus to add later
	int HIT = (As.SKL * 2) + (0.5 * As.LUC) - AVD + Aw.HIT + WTA[1]; // Bonus to add later
	// Double mechanic to add
	int EFF_W_MT = (Aw.MT + WTA[0])*1; // effectiveness (*2) pending
	int MT = As.STR + EFF_W_MT - Bs.DEF; // Bonus pending
	int CRIT_EVADE_B = Bs.LUC + 0; // Bonus pending
	int CRIT = Aw.CRIT + (0.5 * As.SKL) - CRIT_EVADE_B + 0; // Bonus pending
	int ATK_SPD_A = get_ATK_SPD(As, Aw);
	int ATK_SPD_B = get_ATK_SPD(Bs, Bw);
	bool DB = false;
	if ((ATK_SPD_A - ATK_SPD_B) > 3)
	{
	    DB = true;
	}
	HIT = std::clamp(HIT, 0, 100);
	CRIT = std::clamp(CRIT, 0, 100);
	return {As.HP, MT, HIT, DB, CRIT, WTA[0]};
    }
}

std::vector<CombatInfo> interact(const Entity& A,const Entity& B)
{
    std::vector<CombatInfo> out;
    CombatInfo info_A = info(A, B);
    CombatInfo info_B = info(B, A);
    out.push_back(info_A);
    out.push_back(info_B);
    return out;
}

int attack_sequence(Entity& A, Entity& B, CombatInfo& A_perf, CombatInfo& B_perf)
{
    int out;
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
	    out = 1;
	}
    }
    else
    {
	// miss
	std::cout << A.name << "'s turn.....";
	std::cout << "Attack miss!" << "\n";
	A.inventory.slot[A.inventory.EquippedSlot].usesRemaining --;
	out = -1;
    }
    return out;
}

std::vector<sequence> entity_attack(Entity& A, Entity& B, CombatInfo& A_perf, CombatInfo& B_perf, bool A_first, bool db, Mapmaker& map) // Assume first entity attacks twice if db = True
{
    std::vector<sequence> out;
    int outcome;
    if (db)
    {
	if (A_first)
	{
	    outcome = attack_sequence(A, B, A_perf, B_perf);
	    out.push_back({A, outcome, B});
	    if (B.stats.HP > 0)
	    {
		outcome = attack_sequence(B, A, B_perf, A_perf);
		out.push_back({B, outcome, A});
		if (A.stats.HP > 0)
		{
		    outcome = attack_sequence(A, B, A_perf, B_perf);
		    out.push_back({A, outcome, B});
		    if (B.stats.HP <= 0)
		    {
			// B dies from A's second attack.
			map.death(B);
			out.push_back({B, 0, A});
		    }
		}
		else
		{
		    // A dies from B's retaliation.
		    map.death(A);
		    out.push_back({A, 0, B});
		}
	    }
	    else
	    {
		// B dies from A's first attack.
		map.death(B);
		out.push_back({B, 0, A});
	    }
	}
	else
	{
	    outcome = attack_sequence(B, A, B_perf, A_perf);
	    out.push_back({B, outcome, A});
	    if (A.stats.HP > 0)
	    {
		outcome = attack_sequence(A, B, A_perf, B_perf);
		out.push_back({A, outcome, B});
		if (B.stats.HP > 0)
		{
		    outcome = attack_sequence(A, B, A_perf, B_perf);
		    out.push_back({A, outcome, B});
		    if (B.stats.HP <= 0)
		    {
			// B dies from A's second attack.
			map.death(B);
			out.push_back({B, 0, A});
		    }
		}
		else
		{
		    // B dies from A's first attack.
		    map.death(B);
		    out.push_back({B, 0, A});
		}
	    }
	    else
	    {
		// A dies in first turn from B's attack.
		map.death(A);
		out.push_back({A, 0, B});
	    }	    
	}
    }
    else
    {
	outcome = attack_sequence(A, B, A_perf, B_perf);
	out.push_back({A, outcome, B});
	if (B.stats.HP > 0)
	{
	    outcome = attack_sequence(B, A, B_perf, A_perf);
	    out.push_back({B, outcome, A});
	    if (A.stats.HP <= 0)
	    {
		// A's death from retaliation from B.
		map.death(A);
		out.push_back({A, 0, B});
		
	    }
	}
	else
	{
	    // B's Death on A's attack
	    map.death(B);
	    out.push_back({B, 0, A});
	}
    }
    return out;
}

std::vector<sequence> battle(Entity& A, Entity& B, Mapmaker& map)
{
    std::vector<sequence> outcomes;
    std::vector<CombatInfo> out = interact(A, B);
    CombatInfo A_perf = out[0]; CombatInfo B_perf = out[1];
    if (A_perf.DB  &&  B_perf.DB)
    {
	throw std::invalid_argument("Both units cannot double! Some flaw in the logic code.");
    }
    if (A_perf.DB)
    {
	outcomes = entity_attack(A, B, A_perf, B_perf, true, A_perf.DB, map);
    }
    else if (B_perf.DB)
    {
	outcomes = entity_attack(A, B, A_perf, B_perf, false, B_perf.DB, map);
    }
    else
    {
	outcomes = entity_attack(A, B, A_perf, B_perf, true, false, map);
    }
    // if(doubles) --> if(hits) --> if(crits) --> (B.Hp - (A.MT*(1 || 3)*(1 || 2))) && (Aw.DUR - (1 || 2)) && (A.Hp - (B.MT*(1 || 3)*(1 || 2))) && (Bw.DUR - (1 || 2))
    return outcomes;
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
