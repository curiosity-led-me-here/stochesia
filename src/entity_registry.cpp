#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "game_data.h"
#include "entity_registry.h"
using namespace std;


Entity& Registry::spawn(Entity unit, int id)
{
    unit.entity_id = id;
    registry.push_back(make_unique<Entity>(unit));
    return *registry.back();
}

Entity& Registry::get_unit(int id)
{
    for (unique_ptr<Entity>& i : registry)
    {
	if (id == (*i).entity_id)
	{
	    return *i;
	}
    }
    throw invalid_argument("Entity doesnt exist!");
}

vector<Entity*> Registry::live_units()
{
    vector<Entity*> out;
    for (unique_ptr<Entity>& unit : registry)
    {
	if (unit->alive)
	{
	    out.push_back(unit.get());
	}
    }
    return out;
}
