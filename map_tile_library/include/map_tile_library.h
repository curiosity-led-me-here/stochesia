#pragma once

#include <string>
#include <vector>

#include "fe8_theme_categories.h"
#include "fe8_theme_ids.h"
using namespace std;

namespace fe_tiles
{
using IntGrid = vector<vector<int>>;

// Layer-1 values. They are visual classes, deliberately separate from the
// sandbox's Terrain IDs. PLAIN is the grass/base-ground class.
enum TileClass : int
{
    EMPTY = 0,
    PLAIN = 1,
    FOREST = 2,
    MOUNTAIN = 3,
    PEAK = 4,
    VALLEY = 5,
    CLIFF = 6,
    ROAD = 7,
    PLAIN_ROAD = 8,
    DESERT_GROUND = 9,
    SAND = 10,
    RIVER = 11,
    WATER = 12,
    LAKE = 13,
    SEA = 14,
    BRIDGE = 15,
    FORT = 16,
    VILLAGE = 17,
    HOUSE = 18,
    ARMORY = 19,
    VENDOR = 20,
    ARENA = 21,
    INN = 22,
    WALL = 23,
    WALL2 = 24,
    FLOOR = 25,
    STAIRS = 26,
    DOOR = 27,
    GATE = 28,
    FENCE = 29,
    FENCE_WALL = 30,
    FENCE_BRACE = 31,
    WALL_BRACE = 32,
    BRACE_WALL = 33,
    PILLAR = 34,
    THRONE = 35,
    CHEST = 36,
    ROOF = 37,
    RUINS = 38,
    THICKET = 39,
    SNAG = 40,
    BARREL = 41,
    BONE = 42,
    DARK = 43,
    DECK = 44,
    GUNNELS = 45,
    MAST = 46,
    BRACE = 47,
    FLAT = 48,
    DASHDASH = 49,
    UNDEFINED = 50,
    LAKE_CLIFF = 51,
    VILLAGE_HOUSE = 52,
};

// Layer 2 remains one integer grid even though a tile has two visual axes.
// This safely packs {subclass, orientation}; 1000 is larger than every FE8
// source-map subclass/orientation count in this catalogue.
constexpr int ORIENTATION_STRIDE = 1000;

constexpr int make_tile_code(int subclass, int orientation)
{
    return subclass * ORIENTATION_STRIDE + orientation;
}

constexpr int subclass_from_code(int tile_code)
{
    return tile_code / ORIENTATION_STRIDE;
}

constexpr int orientation_from_code(int tile_code)
{
    return tile_code % ORIENTATION_STRIDE;
}

// A literal pixel canvas. The constructor's order is {rows, columns}; an
// IntGrid is always indexed [row / y][column / x].
class TileCanvas
{
public:
    TileCanvas(int rows, int columns, int source_tile_pixels = 16);

    int rows() const;
    int columns() const;
    int tile_pixels() const;

    // Direct access for a renderer that wants to present the canvas without
    // first serialising it to a PNG. Pixels are premultiplied RGBA8 and stay
    // owned by this TileCanvas instance.
    int pixel_width() const;
    int pixel_height() const;
    const vector<unsigned char>& rgba() const;

    // Applies two equally sized layers to this canvas.
    //
    // class_layer[y][x] is a TileClass.
    // tile_layer[y][x] is make_tile_code(subclass, orientation) for that
    // class inside `theme`. Any requested orientation resolves to the sole
    // tile when a subclass contains exactly one orientation.
    // EMPTY skips the cell, so a later draw() call can be used as an overlay.
    void draw(int theme, const IntGrid& class_layer, const IntGrid& tile_layer,
              const string& library_root);

    // Writes the accumulated canvas as a PNG. scale=3 creates 48x48 pixels
    // per original 16x16 tile with nearest-neighbour pixel scaling.
    void write_png(const string& output_png, int scale = 1) const;

private:
    int rows_ = 0;
    int columns_ = 0;
    int tile_pixels_ = 16;
    vector<unsigned char> rgba_;
};

// Read data/catalogue.tsv to learn valid choices before filling tile_layer.
int subclass_count(int theme, int tile_class, const string& library_root);
int orientation_count(int theme, int tile_class, int subclass,
                      const string& library_root);

// A short human-readable string, including source-map theme and all three
// literal visual levels.
string describe_tile(int theme, int tile_class, int subclass, int orientation,
                          const string& library_root);
}
