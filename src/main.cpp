#include "integration.h"
#include "entity_animation.h"
#include "maps.h"
#include "placement_algo.h"
#include "click.h"

void setup_thumbnail_board(fe_tiles::AnimationRenderer& render, Environment& env, Environment::ConfigureEnv& config)
{
    PieceSet::set2(render, env, config);

    vector<Entity*> units;
    for (int member=0; member < env.guilds[0].members.size(); member++)
    {
        for (Guild& guild : env.guilds)
        {
            if (member < guild.members.size())
            {
                units.push_back(guild.members[member]);
            }
        }
    }

    const vector<vector<int>> terrain = env.map().get_map();
    const int height = terrain.size();
    const int width = terrain[0].size();
    const int columns = 7;
    const int rows = (units.size() + columns - 1) / columns;
    vector<vector<int>> occupied;

    for (int index=0; index < units.size(); index++)
    {
        Entity& unit = *units[index];
        const int column = index % columns;
        const int row = index / columns;
        const int desired_x = (column + 1) * width / (columns + 1);
        const int desired_y = (row + 1) * height / (rows + 1);
        vector<int> location;
        int shortest_distance = width + height;

        for (int y=0; y < height; y++)
        {
            for (int x=0; x < width; x++)
            {
                if (terrain[y][x] != TERRAIN_FOREST)
                {
                    continue;
                }

                bool already_occupied = false;
                for (const vector<int>& coord : occupied)
                {
                    if (coord[0] == x && coord[1] == y)
                    {
                        already_occupied = true;
                        break;
                    }
                }

                if (already_occupied)
                {
                    continue;
                }

                const int distance = (x > desired_x ? x - desired_x : desired_x - x)
                    + (y > desired_y ? y - desired_y : desired_y - y);
                if (distance < shortest_distance)
                {
                    shortest_distance = distance;
                    location = {x, y};
                }
            }
        }

        if (location.empty())
        {
            throw invalid_argument("Could not find a thumbnail location for a unit.");
        }

        config.configure_entity_location(unit, location);
        occupied.push_back(location);
    }

    render.sync_units(env.units().live_units());
}

int main()
{
    maps::MapRecipe recipe = maps::chapter_19();
    Environment env(recipe);
    Environment::ConfigureEnv config(env);
    fe_tiles::AnimationRenderer render;
    render.load_map(env.map());
    setup_guild(env, config, render, {"Red", "Green", "Blue"}, {fe_tiles::GuildColor::enemy(), fe_tiles::GuildColor::npc(), fe_tiles::GuildColor::player()});
    setup_thumbnail_board(render, env, config);
    run_click_game(env, render, recipe);
}
