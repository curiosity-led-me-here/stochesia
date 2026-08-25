#include "game_data.h"
#include "integration.h"
#include "entity_data.h"
#include "entity_animation.h"
#include <cstdlib>
using namespace std;

int get_dur(int id);
void setup(Entity& unit, vector<ItemID> id, Environment::ConfigureEnv& config, fe_tiles::AnimationRenderer& render, fe_tiles::UnitVisual visual);
namespace PieceSet
{
    void set1(fe_tiles::AnimationRenderer& render, Environment& env, Environment::ConfigureEnv& config);
    void set2(fe_tiles::AnimationRenderer& render, Environment& env, Environment::ConfigureEnv& config);
}
