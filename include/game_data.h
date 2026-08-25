#pragma once
#include <vector>
#include <string>
#include "game_types.h"
#include "matrixlib.h"
using namespace std;

extern const vector<Rank> TierInfo;

extern const vector<Weapon> Armory;

extern const vector<Healer> HealingData;

template <typename T>
auto findbyid(const T& bank, int id)
{
    for (const auto& X : bank)
    {
	if (X.ID == id)
	{
	    return X;
	}
    }
    throw invalid_argument("No such item in the bank.");
}

extern Weapon get_weapon(const vector<Weapon>& Armory, int id);

extern Healer get_heal(const vector<Healer> HealingData, int id);

extern void get_next_rank(WeaponLevelExp& x);

bool is_weapon(ItemID item);
