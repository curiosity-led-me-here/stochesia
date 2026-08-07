# FE8 integer map recipes

`fe8/catalog.json` indexes all 79 entries in FE8's chapter table. Each recipe
under `fe8/chapters/` has a `terrain_rows` integer matrix in the sandbox's
gameplay vocabulary (`PLAINS = 0`, `ROAD = 1`, `WOODS = 2`, and so on).

Rows are `y`; entries inside a row are `x`. Read `terrain_rows` directly into
`fe_map_builder::TerrainMap`, or use the JSON as a procedural-generation
corpus. `catalog.json` also records the raw FE8-to-sandbox ID conversion.

The recipe grids represent gameplay terrain, not exact tile-image layouts.
They retain FE8's terrain placement and structure footprints, while the map
builder assigns visual tiles.
