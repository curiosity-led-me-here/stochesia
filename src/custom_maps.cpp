#include "maps.h"
#include "integration.h"
#include "terrain_data.h"
#include "map_tile_library.h"
#include "fe8_theme_ids.h"


maps::TerrainMap terrain_grid = {
    {TERRAIN_PLAINS, TERRAIN_PLAINS, TERRAIN_FOREST},
    {TERRAIN_PLAINS, TERRAIN_ROAD,   TERRAIN_PLAINS},
};

maps::VisualClassMap classes = {
    {fe_tiles::PLAIN, fe_tiles::PLAIN,  fe_tiles::FOREST},
    {fe_tiles::PLAIN, fe_tiles::ROAD,   fe_tiles::PLAIN},
};

maps::VisualTileMap tiles = {
    {maps::make_visual_tile_code(0, 0), maps::make_visual_tile_code(0, 0), maps::make_visual_tile_code(0, 0)},
    {maps::make_visual_tile_code(0, 0), maps::make_visual_tile_code(0, 0), maps::make_visual_tile_code(0, 0)},
};

maps::MapRecipe my_map = maps::compose(
    fe_tiles::THEME_CHAPTERS_01,
    terrain_grid,
    classes,
    tiles
);

Environment env(my_map);
