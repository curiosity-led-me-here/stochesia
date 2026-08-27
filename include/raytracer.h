#ifndef GENERAL_PATHTRACING_H
#define GENERAL_PATHTRACING_H
#include <vector>
#include <iostream>
#include <iomanip>
template <typename T>
void print2DVector(const std::vector<std::vector<T>>& v)
{
    for (const auto& row : v)
    {
        for (const auto& x : row)
        {
            std::cout << std::setw(3) << x << " ";
        }
        std::cout << '\n';
    }
}
bool randomizer(double chance);
double cartesian_distance(std::vector<int> target, std::vector<int> inp);
void prioritize(std::vector<std::vector<int>>& state, std::vector<int> target);
std::vector<int> sorted(const std::vector<int>& v, int N);
struct PathResult
{
    int cost;
    std::vector<std::vector<int>> path;
};
bool if_in(std::vector<std::vector<int>> in, std::vector<int> lookup);
void locate_custom_target(const std::vector<std::vector<double>>& traced, std::vector<int> destination, std::vector<std::vector<int>>& out);
std::vector<std::vector<int>> find_edges(const std::vector<std::vector<double>>& inp);
void pathstart(const std::vector<int>& current_coord, double budget, const std::vector<std::vector<int>>& map, const std::vector<int>& target, std::vector<std::vector<double>>& state, bool& finished, const std::vector<std::vector<double>>& explored);
std::vector<std::vector<double>> pathstart(const std::vector<std::vector<int>>& map, const std::vector<int>& current_coord, const std::vector<int>& target, int MOV, bool& finished, const std::vector<std::vector<double>>& explored);
std::vector<std::vector<double>> normalize_state(std::vector<std::vector<double>>& state, double factor, std::vector<int> target);
std::vector<std::vector<double>> add(std::vector<std::vector<double>> A, std::vector<std::vector<double>> B);
void recurse(std::vector<int>& current_coord, const std::vector<int>& target, const std::vector<std::vector<int>>& map, std::vector<std::vector<double>>& state, std::vector<std::vector<int>>& route, int MOV, std::vector<std::vector<std::vector<int>>>& output, bool& finished, std::vector<std::vector<bool>>& visited, const std::vector<std::vector<double>>& explored);
std::vector<std::vector<int>> select_shortest(std::vector<std::vector<std::vector<int>>>& inp);
std::vector<std::vector<int>> recurse(std::vector<int>& current_coord, const std::vector<int>& target, const std::vector<std::vector<int>>& map, int MOV, const std::vector<std::vector<double>>& explored);
void heat(const std::vector<int>& current_coord, int budget, const std::vector<std::vector<int>>& map, std::vector<std::vector<double>>& state);
void heat(const std::vector<std::vector<int>>& map, std::vector<std::vector<int>>& inp, std::vector<std::vector<double>>& explored, const int heat_width, const std::vector<int>& target, const std::vector<int>& start, const double heat_decay_threshold, int initial_exemption);
std::vector<std::vector<std::vector<int>>> procedurally_generate(std::vector<int>& current_coord, const std::vector<int>& target, const std::vector<std::vector<int>>& map, int MOV, int heat_width, std::vector<std::vector<double>>& explored, int num_path, const double heat_decay_threshold, int initial_exemption);
void plot_paths(const std::vector<std::vector<int>>& map, const std::vector<std::vector<std::vector<int>>>& paths, const std::vector<int>& start);
void plot_path_slideshow(const std::vector<std::vector<int>>& map, const std::vector<std::vector<std::vector<int>>>& paths, const std::vector<int>& start);
#endif
