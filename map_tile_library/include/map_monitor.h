#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <utility>

#include "entity_animation.h"
#include "maps.h"

namespace fe_tiles
{
// Native macOS display frontend for the renderer frame model. It owns no game
// state: Mapmaker/Environment own terrain and occupancy; AnimationRenderer
// owns the presentation state; MapMonitor only paints the current frame.
class MapMonitor
{
public:
    struct Options
    {
        std::string library_root =
            "/Users/ashu/Stochesia/map_tile_library";
        std::string title = "FE8 Tactical Monitor";
        int width = 1280;
        int height = 820;

        // FE8 map sprites occupy a literal 32×32 canvas over 16×16 terrain
        // cells. Keep this at 2.0 for an integer, pixel-perfect scale.
        double unit_canvas_in_tiles = 2.0;
        bool tick_renderer_at_60hz = true;

        // FE8 map combat advances once per 60 Hz game frame.
        double battle_animation_speed = 1.0;
    };

    // `recipe` supplies exact visual tiles; `renderer` supplies unit poses and
    // overlays. Call renderer.load_map(your_mapmaker) before construction.
    MapMonitor(const maps::MapRecipe& recipe, AnimationRenderer& renderer);
    MapMonitor(const maps::MapRecipe& recipe,
               AnimationRenderer& renderer,
               Options options);
    ~MapMonitor();

    MapMonitor(const MapMonitor&) = delete;
    MapMonitor& operator=(const MapMonitor&) = delete;

    // Opens the window and enters the native application loop. It continually
    // draws renderer.unit_poses(), blue_tiles(), red_tiles(), miss_effect(),
    // and death_effects(). When enabled, it calls renderer.tick_60hz() once
    // per monitor frame; it never calls any Mapmaker or combat method.
    void run();

    // Use this if an application already owns the Cocoa event loop.
    void open();
    void close();
    bool is_open() const;

    // Rebuilds only the literal map canvas. The new recipe must have visual
    // layers matching the AnimationRenderer's loaded map dimensions.
    void set_map(const maps::MapRecipe& recipe);
    void request_redraw();

    // Cursor coordinates use your game convention: {x, y}. Passing an empty
    // vector hides it. Out-of-map coordinates throw instead of drawing in an
    // arbitrary place.
    void set_cursor(const std::vector<int>& coordinate);
    void clear_cursor();

    // Draws one ordered, presentation-only route on the map. Each coordinate
    // is {x, y}; an arrow on route[n] points toward route[n + 1]. The final
    // coordinate is drawn as a destination marker. This never changes an
    // Entity, occupancy, or Mapmaker state.
    void show_route_arrows(const std::vector<std::vector<int>>& route);

    // Draws several independent ordered routes at once. Each route gets its
    // own arrow colour; routes are never connected to one another.
    // routes[route_number][step_number] is an {x, y} coordinate.
    void show_route_arrows(
        const std::vector<std::vector<std::vector<int>>>& routes
    );
    void clear_route_arrows();
    bool route_arrows_visible() const;

    // Registers ordinary C++ game input. The callback receives the pressed
    // ASCII character (for example 'c', 'm', or ' '). It executes on Cocoa's
    // main thread while the monitor is open, so it may safely call your
    // Environment/Mapmaker methods and then request_redraw().
    void on_key(std::function<void(char)> callback);

    // Runs once after each of the monitor's 60 Hz renderer ticks. Use this
    // for game-owned timed work such as beginning the next already-resolved
    // combat strike after AnimationRenderer::is_busy() becomes false.
    // The monitor never interprets or retains your game state.
    void on_frame(std::function<void()> callback);

    // Changes only the lunge/MISS/hit/death playback rate. Valid values are
    // 0.05 through 1.0; movement timing is unaffected.
    void set_battle_animation_speed(double speed);
    double battle_animation_speed() const;

    // Displays a renderer-only battle forecast. `attacker_combat` and
    // `defender_combat` should be the two values returned by your existing
    // interact(attacker, defender) call. The monitor never calls interact()
    // or battle() itself.
    void show_battle_forecast(const Entity& attacker,
                              const Entity& defender,
                              const CombatInfo& attacker_combat,
                              const CombatInfo& defender_combat);

    // Removes the forecast panel. With no active forecast, the right side of
    // the monitor is intentionally blank.
    void clear_battle_forecast();
    bool battle_forecast_visible() const;

    // Displays a read-only inventory panel for `unit`: its five item slots,
    // remaining uses, and equipped slot. The monitor snapshots this data, so
    // it never retains or mutates the Entity. A battle forecast takes visual
    // precedence while it is shown.
    void show_inventory(const Entity& unit);
    void clear_inventory();
    bool inventory_visible() const;

    // Displays a read-only terrain panel for the supplied native FE8 terrain
    // ID. The monitor snapshots the terrain data, so the caller may use this
    // for the tile under the cursor without exposing a Mapmaker to the UI.
    // When an inventory panel is visible, terrain data appears below the unit
    // stats; otherwise it occupies its own sidebar card.
    void show_terrain_stats(int terrain_id);
    void clear_terrain_stats();
    bool terrain_stats_visible() const;

    // Plays FE8's 68-frame phase-change presentation over the map: diagonal
    // squares sweep in, the title enters after six frames, holds, then exits.
    // The original ROM contains only PLAYER/ENEMY/OTHER title art, so this
    // version uses the bundled FE8 bitmap font to render your dynamic guild
    // name as "<guild>'S PHASE" in that same sequence. This is presentation
    // only; it does not begin a turn or modify Guild/Entity state. The map
    // sweep ends after 68 frames, while the sidebar phase dialogue remains
    // until the next call or clear_phase_intro().
    void show_phase_intro(const std::string& guild_name,
                          GuildColor color = GuildColor::player());
    void clear_phase_intro();
    bool phase_intro_visible() const;

    // Persistent end-state presentation. It is intentionally louder than a
    // normal phase intro: dark map wash, crimson diagonal flashes, and an
    // oversized pulsing FE bitmap GAME OVER title. It never changes game
    // state; call this instead of close() when the player should see the end.
    void show_game_over();
    void clear_game_over();
    bool game_over_visible() const;

    // Implementation detail, public only so Objective-C++ NSView bridge code
    // can hold an opaque pointer. Its members remain private to .mm code.
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};
}
