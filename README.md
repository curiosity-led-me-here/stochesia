# FE8 map-data package

This directory is a self-contained local extraction for procedural-generation
research. Run the builder after changing map source data:

```sh
python3 map_data/build.py
python3 map_data/serve.py
```

Render a chapter map with its original art:

```sh
python3 map_data/render.py Ch1Map --scale 2
```

This writes `map_data/renders/Ch1Map.png`. To inspect a procedurally generated
grid, write either a rectangular JSON array of `terrain_id` values or an object
with a `terrain_rows` field, then run:

```sh
python3 map_data/render.py --terrain-grid my_generated_terrain.json \
  --tileset TileConfiguration1 --objects ObjectType1 --palette MapPalette1 --scale 2
```

That terrain-grid route selects the first compatible visual metatile for each
terrain. It is ideal for tactical prototyping; a later decorator can select
context-sensitive shorelines, road corners, and forest variants.

The API is available at `http://127.0.0.1:4173`:

- `GET /api/catalog` — counts and all routes
- `GET /api/terrains` — the stable 0–64 terrain vocabulary and all movement,
  combat, and healing tables
- `GET /api/tilesets` — each metatile id mapped to its gameplay terrain id
- `GET /api/layouts` — map ids and dimensions
- `GET /api/maps/{layout_id}` — rows of metatile ids and terrain rows for
  every chapter/tileset variant that uses the layout
- `GET /api/chapters` — chapter-to-layout, tileset, palette, object graphics,
  animation, and map-change bindings
- `GET /api/assets` — copied-source asset manifest with SHA-256 hashes
- `GET /assets/...` — the copied raw map graphics, palettes, layouts, changes,
  animations, and source references
- `GET /renders/...` — PNGs created with `render.py`, including the included
  `renders/Ch1Map.png` example

## The two integer spaces

`terrain_id` is the gameplay vocabulary. It is stable across the game:

```json
{ "id": 1, "symbol": "TERRAIN_PLAINS" }
```

`tile_configuration_entry_id` is what the original map layout stores. It is a
lossless visual-layout value. `metatile_id = tile_configuration_entry_id >> 2`
is visual and tileset-specific. To resolve one into gameplay data, select the
chapter's tile configuration:

```text
(layout id, x, y) -> tile_configuration_entry_id -> metatile_id
(tile_configuration, metatile_id) -> terrain_id
terrain_id -> movement costs, avoid/def/res, healing, tags
```

For procedural generation, generate `terrain_id` grids and objectives first.
Choose a compatible tileset/metatile afterward. This prevents the art encoding
from dictating your tactics algorithm, while retaining a lossless bridge to the
original visual assets.

The `assets/` directory is copied from this local FE8 source tree and includes
game-derived art. Keep it local for research or distribute only a patch where
appropriate; do not treat it as original redistributable artwork.
