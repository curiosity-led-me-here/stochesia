# FE Map Builder API

This is a self-contained adapter from a sandbox integer terrain grid to a
tile-hash map and PNG. It contains its own consolidated FE8 tile asset pack;
running it does **not** require `Fire-Emblem-Tile-Map-Editor`.

```text
std::vector<std::vector<int>>  -> gameplay / visual terrain IDs
builder_tile_map.json          -> exact tile hashes
map.png                        -> rendered map
```

Internally, the compiler is deliberately a three-stage pipeline:

```text
integer gameplay grid
    -> visual plan (terrain family, N/E/S/W edges, diagonal corners)
    -> material plan (map theme -> terrain family -> connected chunk)
    -> themed FE8 sprite selection
    -> PNG
```

Your generator never has to emit `MOUNTAIN_NORTH_WEST`-style IDs. It emits a
connected region of `MOUNTAIN`; the visual plan derives the angles.

## C++ use

From the sandbox project root, compile with:

```sh
c++ -std=c++14 -Iinclude src/main.cpp -o game
```

Then include the project-facing header:

```cpp
#include "api.h"

fe_map_builder::TerrainMap map = {
    {96, 96, 96, 96}, // VISUAL_PLAIN
    {96, 99, 99, 96}, // VISUAL_ROAD
    {96, 96, 96, 96},
};

fe_map_builder::BuildOptions options(
    "/Users/ashu/Strategic-Procedural-Generation/fe_map_builder_api",
    "output/my_map",
    3,  // PNG scale
    42  // deterministic seed
);

fe_map_builder::BuildResult result = fe_map_builder::Build(map, options);
```

`result.success` is true on success. `result.png` and
`result.builderTileMapJson` contain the generated paths.

`BuildOptions` takes an optional sixth argument for an art-source preference:

```cpp
fe_map_builder::BuildOptions options(
    "/Users/ashu/Strategic-Procedural-Generation/fe_map_builder_api",
    "output/my_map", 3, 42, 20000,
    "auto" // auto, temperate, highland, coastal, desert, or fortress
);
```

`"auto"` is normally right: mountain cells bias toward the highland source
art, sea cells toward coastal art, and floor/wall cells toward fortress art.
An explicit theme changes only the sprite palette preference; it never changes
the terrain integers or their mechanics.

## Terrain vocabulary

`include/terrain_support.h` has two layers:

- IDs **0–64** are the stable tactical terrain vocabulary (`PLAINS`, `FORT`,
  `THRONE`, `DECK`, and so on). These drive movement, healing, avoid, and
  defence.
- IDs **65–116** are exact renderer vocabulary. They are named `VISUAL_*`
  and correspond one-to-one with bundled visual groups. Examples: `VISUAL_PLAIN
  = 96`, `VISUAL_ROAD = 99`, `VISUAL_FLOOR = 83`, `VISUAL_WALL = 113`,
  `VISUAL_DECK = 76`.

Use the first layer when your procedural generator cares only about gameplay.
Use the second when it needs direct visual control. The renderer always
accepts raw integers, so it can consume either layer.

`data/terrain_support.json` is the lookup table from those IDs to visual tile
groups. Edit that mapping—not your game’s combat data—when you want different
art assignments.

## Visual compiler

The renderer does not treat a gameplay integer as one fixed sprite. Before it
selects tile hashes it derives `visual_role_rows` and `visual_profile_rows`:

```text
MOUNTAIN + neighbouring MOUNTAIN/PEAK cells
    -> highland_06, highland_0e, highland_0f, ...
    -> orientation-aware candidate sprites
```

`visual_role_rows` uses `family_cardinal_diagonal`, where cardinal edges are
`north = 1`, `east = 2`, `south = 4`, `west = 8` and diagonal corners are
`north-east = 1`, `south-east = 2`, `south-west = 4`, `north-west = 8`.
For example, `highland_06_2` means “this highland cell touches highland east
and south, with a south-east diagonal corner.” The compiler selects FE8 tile
art with the same observed edge role, falling back to the closest role only
when the original asset set never contained the requested shape.

Every output also records `visual_family_rows` and `visual_profile_rows`,
alongside the selected tile hashes. The role/profile rules live in
`data/visual_profiles.json`; they are rendering metadata and do not alter
movement, combat, or AI.

The material-plan fields make the renderer inspectable:

- `map_material_origin`: the source palette selected for the overall map.
- `family_material_origins`: one source choice for each semantic family. All
  `MOUNTAIN`, `PEAK`, and `CLIFF` cells, for example, belong to `highland`.
- `visual_chunk_rows` and `visual_chunks`: 4-connected physical regions. A
  river stream, forest mass, or mountain range gets one material source before
  its individual edge/corner sprites are resolved.

An individual cell may fall back only when its chosen palette does not contain
the required rare angle. The metadata reports `palette_coverage`, so these
fallbacks are visible rather than silent.

## FE8 converted map recipes

`recipes/fe8/catalog.json` indexes every FE8 chapter-table map converted into
this project’s gameplay terrain IDs. Each chapter has a standalone JSON matrix
under `recipes/fe8/chapters/`; see `recipes/README.md` for its format.

## Direct Python use

```sh
python3 tools/fe_map_builder.py \
  --map output/my_map/terrain_input.json \
  --asset-root assets \
  --support data/terrain_support.json \
  --output-json output/my_map/builder_tile_map.json \
  --output-png output/my_map/map.png \
  --scale 3 --seed 42
```

## One-time asset import

The checked-in `assets/` folder is already populated. If you ever need to
rebuild it from a local Tile Map Editor clone, run:

```sh
python3 tools/extract_fe8_assets.py \
  --editor-root /path/to/Fire-Emblem-Tile-Map-Editor \
  --output assets
```

The asset pack contains FE-derived graphics. For a distributable game, replace
it with original or appropriately licensed artwork.

## Structure footprints

The renderer can solve local tile edges, but it cannot invent the complete
shape of a house, castle, or ship from one cell. Your generator should place a
multi-cell footprint using the relevant `VISUAL_*` ids; the renderer then
chooses compatible pieces and draws it.

The same applies to connected terrain: a single `ROAD` cell surrounded on all
four sides by plains is not a road shape that exists in the source tile data.
Generate a connected road path (or place a dedicated road-end/road-corner
footprint) and the renderer can resolve it. A three-cell horizontal road line
between plain rows is a valid minimal example.
