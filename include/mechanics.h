#pragma once
#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>
#include <random>
#include "game_data.h"
#include "pathfinder.h"
using namespace std;

extern int seed;
extern vector<vector<int>> WeaponTriangle;

bool random_binary(double probability, int seed);

vector<int> WeaponTriangleAdv(
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

vector<CombatInfo> interact(
    const Entity& A,
    const Entity& B,
    Mapmaker& map
);

int attack_sequence(
    Entity& A,
    Entity& B,
    CombatInfo& A_perf,
    CombatInfo& B_perf
);

vector<sequence> entity_attack(
    Entity& A,
    Entity& B,
    CombatInfo& A_perf,
    CombatInfo& B_perf,
    bool A_first,
    bool db,
    Mapmaker& map
);

vector<sequence> battle(
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
