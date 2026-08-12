#pragma once

#include <vector>

namespace maps
{
// Every grid in maps:: is indexed [y][x]. Gameplay values are native FE8
// TerrainId integers; visual values use map_tile_library's stable integer
// vocabulary (class plus packed {subclass, orientation}).
using IntGrid = std::vector<std::vector<int>>;
using TerrainMap = IntGrid;
using VisualClassMap = IntGrid;
using VisualTileMap = IntGrid;

constexpr int VISUAL_ORIENTATION_STRIDE = 1000;

constexpr int make_visual_tile_code(int subclass, int orientation)
{
    return subclass * VISUAL_ORIENTATION_STRIDE + orientation;
}

constexpr int visual_subclass(int tile_code)
{
    return tile_code / VISUAL_ORIENTATION_STRIDE;
}

constexpr int visual_orientation(int tile_code)
{
    return tile_code % VISUAL_ORIENTATION_STRIDE;
}

// The complete handoff between procedural generation, gameplay, and the
// tile renderer. `terrain` is the only layer Mapmaker needs. `theme`,
// `classes`, and `tiles` retain the exact visual choice for TileCanvas.
struct MapRecipe
{
    TerrainMap terrain;
    int theme_id = 0;
    VisualClassMap classes;
    VisualTileMap tiles;

    bool has_visuals() const;
    int rows() const;
    int columns() const;

    // Keeps mechanics-only call sites source-compatible when they genuinely
    // need only the gameplay layer. New code should use `.terrain` explicitly
    // so the visual layers remain visible in the call site.
    operator const TerrainMap&() const { return terrain; }
};

// Use this when a generator has already chosen exact visual tiles. Gameplay
// terrain is derived from the visual class vocabulary with FE8 movement rules.
MapRecipe from_visual(int theme_id,
                       VisualClassMap classes,
                       VisualTileMap tiles);

// Use this when your generator independently chooses gameplay terrain and
// visuals. All three grids must have the same rectangular dimensions.
MapRecipe compose(int theme_id,
                  TerrainMap terrain,
                  VisualClassMap classes,
                  VisualTileMap tiles);

// Retains the raw TerrainMap pathway for a mechanics-only custom map.
MapRecipe gameplay_only(TerrainMap terrain);

// Each stock FE8 map now returns a complete recipe. Its `terrain` is the
// original FE8 TerrainId grid and its visual layers are reconstructed from the
// Tile Map Editor's literal tile-hash layout and map_tile_library catalogue.
MapRecipe prologue();
MapRecipe chapter_1();
MapRecipe chapter_2();
MapRecipe chapter_3();
MapRecipe chapter_4();
MapRecipe chapter_5x();
MapRecipe chapter_5();
MapRecipe chapter_6();
MapRecipe chapter_7();
MapRecipe chapter_8();

MapRecipe chapter_9_eirika();
MapRecipe chapter_10_eirika();
MapRecipe chapter_11_eirika();
MapRecipe chapter_12_eirika();
MapRecipe chapter_13_eirika();
MapRecipe chapter_14_eirika();

MapRecipe chapter_9_ephraim();
MapRecipe chapter_10_ephraim();
MapRecipe chapter_11_ephraim();
MapRecipe chapter_12_ephraim();
MapRecipe chapter_13_ephraim();
MapRecipe chapter_14_ephraim();

MapRecipe chapter_15();
MapRecipe chapter_16();
MapRecipe chapter_17();
MapRecipe chapter_18();
MapRecipe chapter_19();
MapRecipe chapter_20();
MapRecipe final_chapter_1();
MapRecipe final_chapter_2();

MapRecipe tower_of_valni_1();
MapRecipe tower_of_valni_2();
MapRecipe tower_of_valni_3();
MapRecipe tower_of_valni_4();
MapRecipe tower_of_valni_5();
MapRecipe tower_of_valni_6();
MapRecipe tower_of_valni_7();
MapRecipe tower_of_valni_8();

MapRecipe lagdou_ruins_1();
MapRecipe lagdou_ruins_2();
MapRecipe lagdou_ruins_3();
MapRecipe lagdou_ruins_4();
MapRecipe lagdou_ruins_5();
MapRecipe lagdou_ruins_6();
MapRecipe lagdou_ruins_7();
MapRecipe lagdou_ruins_8();
MapRecipe lagdou_ruins_9();
MapRecipe lagdou_ruins_10();
}
