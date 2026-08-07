#include <iostream>
#include "game_data.h"
#include "api.h"
#include "showcase_maps.h"


int main()
{
    fe_map_builder::TerrainMap map = showcase_maps::continental_campaign;


    fe_map_builder::BuildOptions options(
    "/Users/ashu/Strategic-Procedural-Generation/fe_map_builder_api",
    "output/my_map",
    3,
    42
    );

    auto result = fe_map_builder::Build(map, options);
    return 0;
}

