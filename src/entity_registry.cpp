#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "game_data.h"
#include "entity_registry.h"


Entity& Registry::spawn(Entity unit, int id)
{
    unit.entity_id = id;
    registry.push_back(std::make_unique<Entity>(unit));
    return *registry.back();
}

Entity& Registry::get_unit(int id)
{
    for (std::unique_ptr<Entity>& i : registry)
    {
	if (id == (*i).entity_id)
	{
	    return *i;
	}
    }
    throw std::invalid_argument("Entity doesnt exist!");
}
