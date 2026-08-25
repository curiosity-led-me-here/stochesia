#include <iostream>
#include <string>
#include <vector>

#include "map_tile_library.h"
using namespace std;

int main(int argc, const char* argv[])
{
    const string library_root = argc > 1
        ? argv[1]
        : "/Users/ashu/Strategic-Procedural-Generation/map_tile_library";

    constexpr int height = 12;
    constexpr int width = 18;

    constexpr fe_tiles::ThemeId theme = fe_tiles::THEME_CHAPTERS_01;

    // Layer 1: class. Layer 2: packed {subclass, orientation} within Ch. 1.
    fe_tiles::IntGrid classes(height, vector<int>(width, fe_tiles::PLAIN));
    fe_tiles::IntGrid tiles(height, vector<int>(width, fe_tiles::make_tile_code(0, 0)));

    // A literal map recipe: every integer is independently controllable.
    for (int y = 1; y <= 4; ++y)
    {
        for (int x = 1; x <= 4; ++x)
        {
            classes[y][x] = fe_tiles::FOREST;
        }
    }
    for (int y = 8; y <= 10; ++y)
    {
        for (int x = 12; x <= 16; ++x)
        {
            classes[y][x] = fe_tiles::PEAK;
        }
    }
    for (int y = 0; y < height; ++y)
    {
        classes[y][9] = fe_tiles::LAKE;
    }
    classes[5][8] = fe_tiles::HOUSE;
    tiles[5][8] = fe_tiles::make_tile_code(0, 4); // singleton: all orientations resolve here

    try
    {
        fe_tiles::TileCanvas canvas(height, width);
        canvas.draw(theme, classes, tiles, library_root);
        canvas.write_png("layered_example.png", 3);
        cout << "Wrote layered_example.png\n";
        cout << fe_tiles::describe_tile(theme, fe_tiles::PLAIN, 0, 0, library_root)
                  << '\n';
    }
    catch (const exception& error)
    {
        cerr << "Tile renderer error: " << error.what() << '\n';
        return 1;
    }
}
