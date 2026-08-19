#include "game_data.h"
#include "integration.h"
#include "entity_data.h"
#include "entity_animation.h"
#include "map_monitor.h"
#include "maps.h"
#include "placement_algo.h"
#include "click.h"
#include <string>

int main()
{
    maps::MapRecipe recipe = maps::chapter_1();
    Environment env(recipe);
    Environment::ConfigureEnv config(env);
    fe_tiles::AnimationRenderer render;
    render.load_map(env.map());
    setup_guild(env, config, render, {"Red", "Blue"}, {fe_tiles::GuildColor::enemy(), fe_tiles::GuildColor::player()});
    setup_board(render, env, config);
    run_click_game(env, render, recipe);
}
