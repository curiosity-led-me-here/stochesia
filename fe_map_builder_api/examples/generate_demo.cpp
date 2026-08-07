#include <iostream>

#include "fe_map_builder_api.h"

int main() {
    using namespace fe_map_builder;

    TerrainMap map(20, std::vector<int>(30, PLAINS));

    BuildOptions options(
        "/Users/ashu/Strategic-Procedural-Generation/fe_map_builder_api",
        "/Users/ashu/Strategic-Procedural-Generation/fe_map_builder_api/output/demo",
        2,  // scale
        42  // seed
    );

    const BuildResult result = Build(map, options);
    std::cout << result.message << '\n';
    if (result.success)
        std::cout << result.png << '\n';
    return result.success ? 0 : 1;
}
