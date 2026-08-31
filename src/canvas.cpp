#include "plotter.h"
#include <iostream>
#include <vector>

Canvas::Canvas(int w, int h)
    : width(w), height(h), grid(h, vector<int>(w, 0)) {}

void Canvas::plot(int x, int y, int color)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    grid[y][x] = color;
}

void Canvas::plot(const vector<vector<int>>& coords, int color)
{
    for (const auto& coord : coords)
        plot(coord[0], coord[1], color);
}

void Canvas::draw()
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            switch (grid[y][x])
            {
                case 0: cout << "⬛"; break;
                case 1: cout << "🟦"; break;
                case 2: cout << "🟥"; break;
                case 3: cout << "🟩"; break;
                case 4: cout << "🟨"; break;
                case 5: cout << "🟪"; break;
                case 6: cout << "🟧"; break;
                default: cout << "⬜"; break;
            }
        }

        cout << '\n';
    }
}
