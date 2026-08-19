#pragma once
#include "game_data.h"
#include "click.h"
#include "integration.h"
#include "entity_data.h"
#include "entity_animation.h"
#include "map_monitor.h"
#include "maps.h"
#include <cstdlib>

void run_click_game(Environment& env, fe_tiles::AnimationRenderer& render, maps::MapRecipe& recipe);
