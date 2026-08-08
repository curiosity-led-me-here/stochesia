#pragma once

#include "game_types.h"

namespace mechanics_ascii
{
void interact_window(
    const Entity& attacker,
    const Entity& defender,
    const CombatInfo& attacker_info,
    const CombatInfo& defender_info
);

void battle_window(const Entity& attacker, const Entity& defender);

//void heal_window(const Entity& healer, const Entity& target, int hp_before);
}
