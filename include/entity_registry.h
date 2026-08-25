#pragma once
#include "entity_data.h"
#include <vector>
#include <iostream>
using namespace std;

class Registry
{
    private:
	vector<unique_ptr<Entity>> registry;

    public:
	Entity& spawn(Entity unit, int id);

	Entity& get_unit(int id);

	vector<Entity*> live_units();
};
