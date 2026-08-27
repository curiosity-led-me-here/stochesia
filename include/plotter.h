#pragma once
#include <iostream>
#include <vector>
using namespace std;

class Canvas
{
private:
    int width;
    int height;
    vector<vector<int>> grid;

public:
    Canvas(int w, int h);

    void plot(int x, int y, int color);
    void plot(const vector<vector<int>>& coords, int color);
    void draw();
};
