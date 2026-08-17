# FE8 map tile library

This is a self-contained, literal tile library for your map-generation work.
It is intentionally below the map-builder’s orientation/chunk inference layer:
your algorithm chooses the exact visual tile for every cell.

## Complete FE8 asset and unit-data snapshot

The library now also carries a local FE8 presentation/data snapshot. It is
separate from your sandbox mechanics: nothing here changes `Entity`,
`Mapmaker`, `Registry`, `Guild`, terrain rules, or your existing weapon data.

```text
assets/fe8/
  graphics/                 complete source graphics tree: maps, terrain,
                            map-unit sheets, battle art, portraits, UI, fonts,
                            palettes, effects, and world-map art
  sound/                    complete source sound tree
data/fe8/
  unit_visuals.tsv          107 literal map-unit movement-sheet variants
  class_visuals.tsv         FE8 class ID -> literal map-unit visual
  map_unit_motion.tsv       exact source movement/action frame programs
  assets.tsv                every copied raw graphics/audio file and byte size
  characters.tsv            89 canonical character/unit records
  classes.tsv               127 canonical class records
  source/                   copied source tables and motion definitions
include/
  fe8_unit_visuals.h        C++ visual catalogue and class->visual lookup
  fe8_unit_data.h           C++ character/class/stat catalogue
```

The ordinary map-unit renderer supports every visual in `UnitVisual` with
FE8's normal left/right/down/up/selected movement loop. `map_unit_motion.tsv`
and `data/fe8/source/data/const_data_unit_icon_move.s` also preserve each
literal extra action program (such as Dancer actions) for later renderer work.
All raw sprites and palettes remain unmodified.

Character and class records include FE8 bases, growths, caps, movement,
attributes, portrait IDs, promotions, and source symbols. They are a read-only
catalogue: **your Armory and weapon definitions are intentionally untouched.**
This is a complete local asset/data bundle, not a claim that every FE8 battle,
event, or menu system is already playable.

All 4,069 already-extracted FE8 16×16 map PNGs are sorted by their **original
FE8 source-map theme**. A tile shared by several chapters is placed in each
relevant chapter-theme, giving 7,663 themed references in total.

`reference_layouts/` also contains the original Tile Map Editor hash grids for
every stock FE8 chapter. The upgraded `maps::chapter_*()` loaders use them to
recover exact visual class/subclass/orientation layers without depending on a
separate Tile Map Editor checkout.

`reference_gameplay_maps/` contains the corresponding FE8 `.mar` layouts and
terrain configuration files. Together, the two reference folders make the
stock `maps::` recipes self-contained in this repository.

```text
assets/
  theme_001_chapters_01/
    class_01_plain/
      subclass_000/
        orientation_000.png
        orientation_001.png
  theme_003_chapters_02/
  theme_0xx_tower_of_valni_.../
```

The machine-readable catalogue is in `data/catalogue.tsv`:

```text
theme_id | class_id | subclass_id | orientation_id | tile_hash | relative_png
```

`data/themes.tsv` and `data/classes.tsv` are the compact integer vocabularies.
`data/adjacency.tsv` records every source-provided north/east/south/west link
between orientations in the same `theme/class/subclass`.

## Hierarchy

```text
theme      = a source FE8 map: Chapter 01, Chapter 15, Tower 04, etc.
class      = PLAIN, FOREST, PEAK, WALL, HOUSE, RIVER, ...
subclass   = one adjacency-connected visual tileset of that class
orientation= one exact raw 16×16 tile in that subclass
```

The Tile Map Editor metadata does not label tiles as “northwest”, “inner
corner”, and so on. It gives cardinal compatibility edges instead. Thus
`orientation` is a stable exact variant code, not a falsely inferred compass
label; use `adjacency.tsv` when you write your later orientation compiler.

If a subclass contains one raw tile, the API treats every requested orientation
as that same tile—so your orientation values `1`, `2`, `3`, and `4` all work
for a truly orientationless object. A subclass with several raw orientations is
strict: an unavailable orientation throws rather than silently selecting the
wrong visual.

`include/fe8_theme_ids.h` is generated from `themes.tsv`, so regular C++ can
write `fe_tiles::THEME_CHAPTERS_01` rather than a magic number.

## Two-layer API

Layer 1 is the visual **class**. Layer 2 packs `{subclass, orientation}` into
one integer, retaining your original two-grid format. Grids are always `[y][x]`.

```cpp
#include "map_tile_library.h"

const int height = 20;
const int width = 30;

fe_tiles::IntGrid classes(height, std::vector<int>(width, fe_tiles::PLAIN));
fe_tiles::IntGrid tiles(height, std::vector<int>(width, fe_tiles::make_tile_code(0, 0)));

classes[3][4] = fe_tiles::FOREST;
tiles[3][4] = fe_tiles::make_tile_code(0, 1); // subclass 0 / orientation 1

fe_tiles::TileCanvas canvas(height, width);
canvas.draw(fe_tiles::THEME_CHAPTERS_01, classes, tiles,
             "/Users/ashu/Stochesia/map_tile_library");
canvas.write_png("my_map.png", 3); // 48 pixels per logical FE map tile
```

Use `fe_tiles::subclass_count(theme, tile_class, root)` and
`fe_tiles::orientation_count(theme, tile_class, subclass, root)` before
choosing an exact visual. If a hierarchy combination does not exist, `draw()`
throws with its exact `{x,y}` coordinate rather than silently substituting art.

`EMPTY` (`0`) skips the cell, allowing a second `draw()` call to overlay a
layer—for example, a road, bridge, or building over an already-drawn ground
layer.

## Build the example

```sh
cd /Users/ashu/Stochesia/map_tile_library
./build_example.sh
```

It renders `layered_example.png`. The implementation is C++ API code in an
Objective-C++ `.mm` file solely to use macOS’s built-in CoreGraphics/ImageIO
PNG decoder and encoder; your call site remains ordinary C++.

## Live in-memory workbench

If you want to manipulate the two grids without writing a PNG at all, run:

```sh
cd /Users/ashu/Stochesia/map_tile_library
./viewer/run_workbench.sh

# Or create a canvas with an explicit rows × columns size:
./viewer/run_workbench.sh 20 30
```

This launches a native macOS window. It keeps the class grid and packed
`{subclass, orientation}` grid in memory, calls `TileCanvas::draw()`, and
presents `TileCanvas::rgba()` directly. `write_png()` is never called.

- Click a cell to paint the selected raw FE8 tile.
- Right-click a cell to sample its class/subclass/orientation integers.
- `1`–`5` choose plain, forest, peak, lake, or house.
- `[` / `]` cycle subclass; `-` / `=` cycle its literal orientation variant.
- Up/Down change the chapter/source-map theme; `R` fills the canvas.
- `Space` animates Eirika from the live occupancy layer as a 60 Hz test move.
- `E` exports the current map. Closing the last window or pressing Esc also
  exports automatically to `viewer/exported_recipe.cpp`.

The export is plain copyable C++ with three aligned `[y][x]` grids:

```cpp
maps::IntGrid terrain_grid; // gameplay Terrain IDs for Mapmaker
maps::IntGrid classes;      // visual TileClass IDs
maps::IntGrid tiles;        // packed {subclass, orientation} tile IDs
```

It ends with a ready-to-use `maps::compose(theme_id, terrain_grid, classes,
tiles)` call. The workbench derives `terrain_grid` through the same visual
class -> gameplay terrain mapping it uses for sandbox preview, so generated
forests, roads, rivers, walls, and so on retain consistent movement rules.

## Unit overlay from occupancy

`include/occupancy_overlay.h` is the separate presentation adapter for your
future `Mapmaker::occupancy` grid. It uses `occupancy[y][x] = entity_id` and a
small entity-ID-to-sprite registration table; it does not replace Registry,
Guild, terrain, pathfinding, or combat.

```cpp
fe_tiles::OccupancyGrid occupancy(height, std::vector<int>(width, 0));
occupancy[6][3] = 1; // Eirika's entity ID

fe_tiles::SpriteLayer units(occupancy);
units.register_unit(1, fe_tiles::UnitVisual::Eirika);
units.begin_move(1, {{3, 6}, {4, 6}, {5, 6}}); // route from your pathfinder
units.tick_fe_frame();                          // once per 60 Hz frame
if (auto arrival = units.take_completed_move())
{
    occupancy[arrival->from.y][arrival->from.x] = 0;
    occupancy[arrival->to.y][arrival->to.x] = arrival->entity_id;
}
```

`SpriteLayer::poses()` emits exactly one pose per entity. While an entity is
in transit it suppresses that entity's static occupancy pose and emits only
the fractional animated pose. That prevents the duplicated-unit glitch.
It never writes occupancy itself; the caller (eventually your Mapmaker bridge)
is the sole owner of gameplay state.

## Stock maps: one recipe for mechanics and rendering

`maps::chapter_1()` is no longer merely an integer terrain matrix. It now
returns `maps::MapRecipe`, which keeps four aligned `[y][x]` layers:

- `terrain` — the original FE8 terrain IDs consumed by `Mapmaker`.
- `theme_id` — the exact source-map/chapter theme in this tile library.
- `classes` — the visual class grid.
- `tiles` — packed `{subclass, orientation}` values for the literal source tiles.

That means your normal setup stays meaningful:

```cpp
Environment env(maps::chapter_1());
```

The `Environment` builds its `Mapmaker` from `recipe.terrain`, while retaining
the complete recipe for the renderer:

```cpp
const maps::MapRecipe& recipe = env.map_data();

fe_tiles::TileCanvas canvas(recipe.rows(), recipe.columns());
canvas.draw(recipe.theme_id, recipe.classes, recipe.tiles,
            "/Users/ashu/Stochesia/map_tile_library");
```

For procedural maps, use `maps::from_visual(theme, classes, tiles)` when the
visual class determines gameplay terrain, or `maps::compose(...)` when your
generator owns both grids independently. A mechanics-only custom terrain grid
can use `maps::gameplay_only(terrain)`.

`Environment::map()` and `Environment::units()` expose the existing board and
registry to a renderer without creating duplicate gameplay state.

## Complete monitor frontend

The library now includes the missing last layer of the pipeline:

```text
Game logic
    -> AnimationRenderer
    -> unit_poses(), blue_tiles(), red_tiles(), combat effects
    -> MapMonitor
    -> macOS window / monitor
```

`MapMonitor` is a native Cocoa view implementation. It renders the literal
`MapRecipe` tile canvas, then draws the following current `AnimationRenderer`
state on top of it every display frame:

- `blue_tiles()[y][x] >= 0` as blue legal-movement cells;
- `red_tiles()[y][x] != 0` only where the cell is not blue;
- `unit_poses()` as exact FE8 map-unit sprites;
- `miss_effect()` and `death_effects()` as the map-combat effects.

It ticks `AnimationRenderer::tick_60hz()` at FE8's 60 Hz by default. It never
calls `Mapmaker::path_trace()`, `Mapmaker::move()`, `battle()`, or mutates an
`Entity`, `Registry`, `Guild`, or occupancy grid. Your logic remains the sole
authority; the monitor only displays the current renderer frame.

The monitor uses the bundled literal map-unit sheets and palettes in
`assets/fe8/graphics/unit_icon/`, so it does not need the separate FE8
checkout at runtime.

### Guild colours and FE8 palettes

FE8 map-unit graphics are shared indexed art, not separately drawn blue and
red sprites. The game dynamically selects one of several 16-colour OBJ
palettes for the same graphics. The monitor now preserves that design.

Assign a colour to a whole Guild before binding its members:

```cpp
fe_tiles::AnimationRenderer render;
render.load_map(board);

render.set_guild_color(renais, fe_tiles::GuildColor::player()); // authentic blue
render.set_guild_color(enemy,  fe_tiles::GuildColor::enemy());  // authentic red
render.set_guild_color(npc,    fe_tiles::GuildColor::npc());    // authentic green
render.set_guild_color(team4,  fe_tiles::GuildColor::fourth()); // authentic purple

auto seth_art = render.entity(seth, fe_tiles::UnitVisual::Paladin);
auto foe_art  = render.entity(enemy_unit, fe_tiles::UnitVisual::Soldier);
```

`player()`, `enemy()`, `npc()`, and `fourth()` load the literal FE8 palette
data bundled in `assets/fe8/graphics/unit_icon/palette/`. For a project-specific guild, pass a
24-bit `0xRRGGBB` colour instead. The renderer shades only the palette entries
that FE8 changes for team affiliation, retaining skin, steel, white, and
transparency entries:

```cpp
render.set_guild_color(mercenaries, 0xE6813A); // custom orange guild

// Or override one unit instead of its entire guild:
auto boss_art = render.entity(boss, fe_tiles::UnitVisual::Paladin,
                              fe_tiles::GuildColor::custom(0x9B47D6));
```

Guild colours belong purely to the renderer. `set_guild_color()` does not add
a field to `Guild`, alter its members, or modify any game logic.

### Minimal game-to-monitor wiring

```cpp
#include "entity_animation.h"
#include "map_monitor.h"

maps::MapRecipe chapter = maps::chapter_1();
Environment env(chapter);

// Set up your Registry, Guilds, units, and board exactly as usual.
Mapmaker& board = env.map();

fe_tiles::AnimationRenderer render;
render.load_map(board);

auto seth_art = render.entity(seth, fe_tiles::UnitVisual::Paladin);
auto foe_art  = render.entity(enemy, fe_tiles::UnitVisual::Soldier);
render.sync_units({&seth, &enemy});

// These are your existing calculations. The monitor does not invent cells.
board.path_trace(seth);
board.attack_range(seth);
seth_art.paint_blue(); // reads seth.path
seth_art.paint_red();  // reads seth.attack_range

// `route` is the ordered {{x,y}, ...} result returned/reconstructed by YOUR
// movement logic. Starting it is visual only; your logic commits the move.
seth_art.move(route);

fe_tiles::MapMonitor monitor(chapter, render);
monitor.run(); // opens the native macOS window
```

All map grids use `[y][x]`; all unit coordinates and routes use `{x, y}`.
`Mapmaker::attack_range()` follows that same convention: it generates red
attack coverage from real movement origins and writes it as `[y][x]`.

### Build and run the supplied monitor proof

The proof app creates a normal Chapter 1 `Mapmaker`, places units through the
real board API, calls `path_trace()` and `attack_range()`, and then opens the
monitor. Its overlays are therefore real logic output, not a hardcoded demo
matrix.

```sh
cd /Users/ashu/Stochesia/map_tile_library
./monitor/build_monitor.sh
./monitor/build/fe8_tactical_monitor
```

Files for this final frontend layer:

```text
include/map_monitor.h        public monitor API
src/map_monitor.mm           native Cocoa view and 60 Hz frame loop
monitor/main.cpp             real-Mapmaker proof application
monitor/build_monitor.sh     build command
```

If another GUI framework eventually owns your event loop, use `monitor.open()`
and `monitor.request_redraw()` instead of `monitor.run()`. Set
`Options::tick_renderer_at_60hz = false` if your own frontend is already
calling `render.tick_60hz()`.

## Direct interface for your game logic

For actual sandbox integration, include **one** header instead of manually
wiring `SpriteLayer` and `CombatPresentation`:

```cpp
#include "logic_render_control.h"
```

`fe_tiles::LogicRenderControl` is intentionally a renderer-facing mirror of
your game. It does not include `Mapmaker`, select a target, calculate a range,
or invoke `battle()`. You call your logic exactly as you do now, then hand its
results to the renderer.

```cpp
// Initialise once, using map dimensions in [y][x] / rows×columns order.
fe_tiles::LogicRenderControl render(map_rows, map_columns);

render.bind(eirika, fe_tiles::UnitVisual::Eirika);
render.bind(seth,   fe_tiles::UnitVisual::Paladin);
render.bind(enemy,  fe_tiles::UnitVisual::Soldier);
render.sync_units({&eirika, &seth, &enemy});

// Your existing mechanics owns the range calculation.
board.path_trace(seth);
board.attack_range(seth);
render.show_action_state(seth); // path = blue; attack-only = red

// Your existing movement code reconstructs/approves this route.
// Every coordinate is {x, y}, including the start tile.
std::vector<std::vector<int>> route = {
    seth.location, {14, 8}, {15, 8}, {15, 7}
};
render.begin_move(seth, route); // visual only; does NOT move Seth in the game

// Your GUI owns this 60 Hz loop. Draw render.visible_unit_poses() each frame.
while (render.is_moving())
{
    render.tick_60hz();
    request_redraw();
}

// Your logic commits the move; the renderer does not.
seth.location = {15, 7}; // or your own non-interactive movement commit
render.sync_units({&eirika, &seth, &enemy});

// Your mechanics emits one result for EACH strike. The renderer does not try
// to infer a miss from final HP: a miss and zero damage are different events.
render.begin_strike(seth, enemy, fe_tiles::StrikeOutcome::Miss);
while (render.is_presenting_attack())
{
    render.tick_60hz();
    request_redraw();
}
render.sync_units({&eirika, &seth, &enemy});
```

Your view draws only these values:

- `render.movement_overlay()[y][x] >= 0` → blue movement tile.
- `render.attack_overlay()[y][x] != 0` and no blue tile → red attack-only tile.
- `render.visible_unit_poses()` → static or interpolated map-unit sprites.
- `render.miss_effect()` → draw the MISS popup at `{x, y}` while present.
- `render.death_effects()` → frozen white map-unit fade.

This split is deliberate: `LogicRenderControl` never changes `Entity`,
`Registry`, `Guild`, `Mapmaker`, game occupancy, turn state, damage, or alive
state. It returns a `CompletedMove` after the visual route ends so **your**
logic can decide whether and how to commit it.

Compile the portable presentation layer into any C++ frontend with:

```sh
clang++ -std=c++17 \
  -I/Users/ashu/Stochesia/map_tile_library/include \
  -I/Users/ashu/Stochesia/include \
  your_frontend.cpp \
  /Users/ashu/Stochesia/map_tile_library/src/occupancy_overlay.cpp \
  /Users/ashu/Stochesia/map_tile_library/src/logic_render_control.cpp \
  -o your_frontend
```

The Cocoa workbench is only one view that can draw this frame model. The API
above has no Cocoa/SDL/SFML dependency, so the same movement, attack lunge,
MISS popup, and death fade can be driven by whichever renderer you choose
later.

## Entity-specific animation façade

For the compact API you asked for, include this instead:

```cpp
#include "entity_animation.h"
```

`AnimationRenderer` owns **one shared scene** for a `Mapmaker`. Its
`EntityAnimation` handles are entity-specific, so the calls read naturally
without giving every entity a private map, occupancy layer, or frame clock.

```cpp
fe_tiles::AnimationRenderer render;
render.load_map(board);                   // Load map(Mapmaker&)

auto eirika_anim = render.entity(eirika, fe_tiles::UnitVisual::Eirika);
auto seth_anim   = render.entity(seth,   fe_tiles::UnitVisual::Paladin);
auto enemy_anim  = render.entity(enemy,  fe_tiles::UnitVisual::Soldier);
render.sync_units({&eirika, &seth, &enemy});

// Pathfinding and attack range are still entirely yours.
board.path_trace(seth);
board.attack_range(seth);
seth_anim.paint_blue();                   // reads seth.path
seth_anim.paint_red();                    // reads seth.attack_range

// Or give either overlay an explicit [y][x] integer grid.
seth_anim.paint_blue(my_movement_grid);
seth_anim.paint_red(my_attack_grid);

// Visual movement only. coords are your ordered {{x,y}, ...} route.
seth_anim.move(coords);

// Tick from the GUI's 60 Hz timer; then draw the getters below.
render.tick_60hz();
render.unit_poses();
render.blue_tiles();
render.red_tiles();

// Direction/facing is inferred from the entity locations. This handles
// left/right/up/down and diagonal targets; diagonal movement is real while
// the FE8 map sprite faces the same cardinal direction FE8 chooses.
seth_anim.dash(enemy);                    // normal hit lunge
seth_anim.miss(enemy);                    // lunge + target MISS popup
enemy_anim.death();                       // standalone white death fade
```

### Bind visuals and inspect data by canonical FE8 IDs

You do not need to hardcode `UnitVisual` if the unit has an FE8 class ID.
The catalogue resolves all 127 class records to their literal map-unit sheet:

```cpp
#include "fe8_unit_data.h"
#include "fe8_unit_visuals.h"

const fe_tiles::Fe8CharacterRecord* seth_data = fe_tiles::fe8_character(2);
const fe_tiles::Fe8ClassRecord* paladin_data  = fe_tiles::fe8_class(7);

// FE8 class ID 7 is Paladin. This selects artwork only; it does not overwrite
// your existing Seth Entity stats, inventory, terrain overrides, or weapons.
auto seth_anim = render.entity_for_fe8_class(seth, paladin_data->id);

// If you want the original FE8 stat line for reference:
const fe_tiles::Fe8StatBlock original =
    fe_tiles::resolved_base_stats(*seth_data, *paladin_data);
```

`all_unit_visuals()` exposes all 107 literal visual sheets. The matching TSV
files provide the source-relative raw sprite paths and exact animation frames.
For C++-side motion data, `map_unit_motion_program(visual, animation_id)`
returns the same literal `{ticks, frame}` program; IDs `0`–`4` are the normal
left/right/down/up/selected cycles and any remaining IDs are source-defined
actions. The FE8 data catalogue is intentionally not wired into your `Armory`; weapons
remain only the data you already authored.

`render.miss_effect()` and `render.death_effects()` are the extra frame data
for those two effects. `render.terrain_ids()` exposes the exact `[y][x]`
integer map copied from `Mapmaker::get_map()`; your `TileCanvas`/theme layer
can skin those gameplay IDs however you choose. The façade never commits
movement, marks an entity dead, updates occupancy, or invokes combat.

Compile the façade by adding its source beside the existing renderer files:

```sh
/Users/ashu/Stochesia/map_tile_library/src/entity_animation.cpp
```

## Map-unit attack and death presentation

`CombatPresentation` is the matching renderer-side adapter for the result of
your existing `battle()` call. It performs **no** hit, damage, weapon, or death
calculation. Your game creates `BattleWindow` from the resolved `Entity` and
`CombatInfo` values, captures the two current `UnitPose` values, and gives them
to the presentation layer:

```cpp
fe_tiles::CombatPresentation presentation;
presentation.begin(resolved_battle_window, attacker_pose, defender_pose);

// Draw presentation.attack_effect() over the normal unit layer.
// Draw presentation.miss_effect() whenever the optional has a value.
// Draw presentation.death_effects() over your unit layer.
presentation.tick_fe_frame(); // once per displayed 60 Hz frame
```

This intentionally renders no battle information window. Its map-unit attack
is taken directly from FE8's `ProcScr_MapAnimDefaultItemEffect`: the subject
faces the target, moves four times by `0x10` q4 (one original pixel per 60 Hz
frame), shows the outcome, then returns with four matching steps. For a miss,
the original calls `MapAnim_BeginMISSAnim(target)` at impact time; the same
target anchor is exposed through `miss_effect()`. If `attacker_defeated` or
`defender_defeated` is true, the frozen map sprite then uses FE8's actual
`MU_StartDeathFade`: a white palette flash and a 32-frame fade. The defeated
entity can already be gone from your Registry/occupancy—the snapshot is
intentionally independent so it cannot ghost or disappear early.

## Terminal-controlled movement preview

Run the workbench **from a terminal** (not Finder or `open`) so it owns that
terminal's standard input:

```sh
cd /Users/ashu/Stochesia/map_tile_library
./viewer/run_workbench.sh
```

Then type the same command grammar as your sandbox into that terminal:

```text
workbench> check 1
workbench> move 2 0
```

For visual verification of the presentation layer, the workbench also accepts:

```text
workbench> attack 1 3 1
```

This plays FE8's map-unit lunge for sample entities 1 and 3, then a death
effect for entity 3. `battle` remains an alias. It is only a renderer demo;
its sample result is not a replacement for your combat system. In the eventual
bridge, feed the exact result of your own `battle()` call into
`CombatPresentation::begin()` instead.

`check <unit id>` pauses the invariant demo, selects that occupancy ID, and
shows its blue visual movement range. `move <x offset> <y offset>` uses the
currently checked entity, creates an ordered cardinal route, and passes it to
the FE8 60 Hz sprite animator. On arrival, the workbench app commits its own
demonstration occupancy grid. The terrain canvas itself is not regenerated or
changed.

Terminal offsets use Cartesian directions: `+x` right, `+y` up. The renderer
converts this to its internal top-to-bottom grid row coordinate.

This is intentionally the renderer-side test harness. The eventual game
bridge will retain the two commands and the animation path, while replacing
the workbench's temporary range/route producer with your actual
`Mapmaker::path_trace()` and movement result.

## Rebuild the catalogue

The copied asset set and TSV files are already present. If you ever update the
map-builder extraction, regenerate them with:

```sh
python3 tools/build_catalog.py
```
