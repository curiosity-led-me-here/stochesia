#pragma once

#include <vector>

extern std::vector<std::vector<int>> helper;

void print(const std::vector<std::vector<int>>& matrix);

void path(
    int MOV,
    const std::vector<int>& start,
    std::vector<std::vector<int>>& out
);

void normalize_path(
    const std::vector<int>& start,
    std::vector<std::vector<int>>& out
);

void trace(
    std::vector<std::vector<int>>& out_min,
    std::vector<std::vector<int>>& out_max,
    const std::vector<int>& start,
    int MIN,
    int MAX
);
