#pragma once
#include <vector>
#include <string>
#include "game_types.h"
#include "matrixlib.h"

extern const std::vector<Rank> TierInfo;

extern const std::vector<Weapon> Armory;

extern const std::vector<Healer> HealingData;

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
    throw std::invalid_argument("No such item in the bank.");
}

extern Weapon get_weapon(const std::vector<Weapon>& Armory, int id);

extern Healer get_heal(const std::vector<Healer> HealingData, int id);

extern void get_next_rank(WeaponLevelExp& x);
