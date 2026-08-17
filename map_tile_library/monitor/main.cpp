#include "entity_animation.h"
#include "entity_data.h"
#include "map_monitor.h"
#include "maps.h"
#include "pathfinder.h"

// This is intentionally a display proof, not a second game implementation.
// Its blue/red cells come from Mapmaker::path_trace() and
// Mapmaker::attack_range(), never a fabricated test matrix.
int main()
{
    maps::MapRecipe chapter = maps::chapter_1();
    Mapmaker board(chapter);

    Entity eirika = entities::eirika();
    eirika.entity_id = 1;
    eirika.location = {3, 6};

    Entity seth = entities::seth();
    seth.entity_id = 2;
    seth.location = {12, 8};

    Entity soldier = entities::soldier();
    soldier.entity_id = 3;
    soldier.location = {6, 4};

    // Mapmaker's actual path tracing requires a guild and legal occupancy,
    // just as it does in your sandbox. These local teams exist only to make
    // this monitor proof call the real logic correctly.
    Guild renais{"Renais", 1};
    Guild enemy{"Enemy", 2};
    renais.add(eirika);
    renais.add(seth);
    enemy.add(soldier);
    board.place_unit(eirika);
    board.place_unit(seth);
    board.place_unit(soldier);

    fe_tiles::AnimationRenderer renderer;
    renderer.load_map(board);
    renderer.set_guild_color(renais, fe_tiles::GuildColor::player());
    renderer.set_guild_color(enemy, fe_tiles::GuildColor::enemy());
    auto eirika_art = renderer.entity(eirika, fe_tiles::UnitVisual::Eirika);
    auto seth_art = renderer.entity(seth, fe_tiles::UnitVisual::Paladin);
    renderer.entity(soldier, fe_tiles::UnitVisual::Soldier);
    renderer.sync_units({&eirika, &seth, &soldier});

    // Genuine visual state: board calculates both grids; the renderer merely
    // presents them. Eirika has an Iron Sword, so red means sword targets.
    eirika.inventory.slot[0] = {IRON_SWORD, 46};
    eirika.inventory.EquippedSlot = 0;
    board.path_trace(eirika);
    board.attack_range(eirika);
    eirika_art.paint_blue();
    eirika_art.paint_red();

    fe_tiles::MapMonitor monitor(chapter, renderer);
    monitor.run();
}
