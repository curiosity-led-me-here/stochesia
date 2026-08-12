#pragma once
#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>
#include <random>
#include "game_data.h"
#include "pathfinder.h"

extern int seed;
extern std::vector<std::vector<int>> WeaponTriangle;

bool random_binary(double probability, int seed);

std::vector<int> WeaponTriangleAdv(
    const Weapon& A,
    const Weapon& B
);

int get_ATK_SPD(
    const Stats& As,
    const Weapon& Aw
);

CombatInfo info(
    const Entity& A,
    const Entity& B
);

std::vector<CombatInfo> interact(
    const Entity& A,
    const Entity& B
);

void attack_sequence(
    Entity& A,
    Entity& B,
    CombatInfo& A_perf,
    CombatInfo& B_perf
);

void entity_attack(
    Entity& A,
    Entity& B,
    CombatInfo& A_perf,
    CombatInfo& B_perf,
    bool A_first,
    bool db,
    Mapmaker& map
);

void battle(
    Entity& A,
    Entity& B,
    Mapmaker& map
);

void Heal(
    Entity& A,
    int invslot
);

void Heal(
    Entity& Caster,
    Entity& A,
    int id
);
