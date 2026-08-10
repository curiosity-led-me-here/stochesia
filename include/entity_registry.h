#pragma once
#include "entity_data.h"
#include <vector>
#include <iostream>

class Registry
{
    private:
	std::vector<std::unique_ptr<Entity>> registry;

    public:
	Entity& spawn(Entity unit, int id);

	Entity& get_unit(int id);
};
