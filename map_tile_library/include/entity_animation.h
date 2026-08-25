#pragma once

#include <memory>
#include <optional>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "logic_render_control.h"
using namespace std;

// Kept as a forward declaration so including this renderer header does not
// pull the entire sandbox pathfinder implementation into every GUI file.
class Mapmaker;

namespace fe_tiles
{
// A renderer-owned scene whose only game input is Mapmaker's already-built
// terrain grid and Entity data. It never calls path_trace(), attack_range(),
// move(), battle(), or changes Entity/Mapmaker state.
class AnimationRenderer;

// A lightweight per-Entity handle returned by AnimationRenderer::entity().
// It does not own the Entity or the scene. Keep the AnimationRenderer alive
// for as long as any EntityAnimation handle is in use.
class EntityAnimation
{
public:
    // Start 60 Hz visual movement on the route approved by your logic.
    // Each coordinate is {x, y}, including the entity's starting tile.
    bool move(const vector<vector<int>>& coords);

    // Play a route returned by your already-completed Mapmaker::move().
    // This is the compatible form for the sandbox's current synchronous move
    // implementation, which commits Entity::location before returning.
    bool play_committed_move(const vector<vector<int>>& coords);

    // Paint a supplied [y][x] grid into the respective overlay. Blue uses
    // values >= 0; red uses non-zero values. These names deliberately accept
    // any grid, so you can pass unit.path / unit.attack_range directly or
    // your own derived range grid.
    void paint_blue(const RenderGrid& path);
    void paint_red(const RenderGrid& path);

    // Convenient direct forms for the current Entity fields:
    // entity.path -> blue and entity.attack_range -> red.
    void paint_blue();
    void paint_red();
    void clear_paint();

    // FE-style completed-turn visual. This changes only the rendered sprite:
    // it does not alter Entity::turn, Guild, Registry, or map occupancy.
    // Call `turn_greyscale()` after your logic ends this unit's action, then
    // `turn_greyscale(false)` when its side receives a new phase.
    void turn_greyscale(bool enabled = true);
    bool is_turn_greyscale() const;

    // FE8 map-combat lunge. Direction is inferred from this entity to target:
    // left/right/up/down sprites are selected for cardinal and diagonal
    // targets; the lunge itself travels diagonally when both axes differ.
    void dash(Entity& target);
    // Same FE8 lunge, but stages the target's already-resolved HP change for
    // the hit frame instead of displaying it at battle start.
    void dash(Entity& target, int target_hp_after);
    void critical(Entity& target, int target_hp_after);
    void miss(Entity& target);
    // A non-attacking combat turn. The unit faces its opponent and holds
    // position for one map-battle beat.
    void wait(Entity& target);

    // Standalone FE8 map-unit white fade. Call it only after the final
    // normal/critical animation and its post-round pause. The caller still
    // owns alive state, Registry removal, Guild removal, and Mapmaker
    // occupancy.
    void death();

    Entity& entity() const;

private:
    friend class AnimationRenderer;
    EntityAnimation(AnimationRenderer& renderer, Entity& entity);

    AnimationRenderer* renderer_ = nullptr;
    Entity* entity_ = nullptr;
};

// One AnimationRenderer belongs to one loaded map. Create EntityAnimation
// handles from it for every unit you want to animate. This central ownership
// is what prevents the two-Eirikas-on-one-tile problem: there is one shared
// occupancy layer and one shared frame clock for the entire scene.
class AnimationRenderer
{
public:
    AnimationRenderer() = default;
    AnimationRenderer(const AnimationRenderer&) = delete;
    AnimationRenderer& operator=(const AnimationRenderer&) = delete;

    // Extracts Mapmaker::get_map() and builds a renderer grid of the same
    // [y][x] dimensions. The returned terrain IDs are available through
    // terrain_ids() for your map skin/TiledCanvas side; this class does not
    // invent a visual theme from gameplay IDs.
    void load_map(Mapmaker& map);
    bool has_map() const;
    const RenderGrid& terrain_ids() const;

    // Bind an entity to its exact FE8 map-unit sheet and make its per-unit
    // animation handle. Recalling this with the same Entity only changes its
    // visual type/colour; it never duplicates the unit. If a colour was
    // assigned to unit.group with set_guild_color(), that colour wins.
    EntityAnimation entity(Entity& unit, UnitVisual visual,
                           GuildColor color = GuildColor::player());

    // Convenience form for the canonical FE8 class IDs exported by
    // fe8_unit_data.h. It only chooses art; it does not alter your Entity's
    // class, statistics, weapons, or terrain rules.
    EntityAnimation entity_for_fe8_class(Entity& unit, int fe8_class_id,
                                         GuildColor color = GuildColor::player());

    // Renderer-only team colouring. These overloads never change Guild;
    // assign them before binding that Guild's units. A raw colour is 0xRRGGBB
    // and produces a shaded custom FE-style team palette.
    void set_guild_color(const Guild& guild, GuildColor color);
    void set_guild_color(const Guild& guild, uint32_t rgb_code);

    // Returns the colour registered for this Guild ID. If the renderer has
    // not been given one, it returns the same player-blue fallback used when
    // binding an Entity with no explicit guild palette.
    GuildColor guild_color(int guild_id) const;

    // Rebuilds the render-only occupancy layer from living units. Call after
    // your own game logic commits a movement, death, spawn, or removal.
    void sync_units(const vector<Entity*>& live_entities);

    // One GUI frame. Movement always advances at 60 Hz. `advance_combat`
    // lets a frontend slow only the battle presentation clock.
    void tick_60hz(bool advance_combat = true);
    bool is_busy() const;
    bool is_moving() const;
    bool is_presenting_combat() const;
    optional<CompletedMove> take_completed_move();

    // Draw data for a GUI frontend.
    const RenderGrid& blue_tiles() const;
    const RenderGrid& red_tiles() const;
    // Always safe: clears blue/red overlays even when no EntityAnimation is
    // selected and even when the overlays were already empty.
    void clear_paint();
    const OccupancyGrid& occupancy() const;
    vector<UnitPose> unit_poses() const;
    vector<HealthBar> health_bars() const;
    const optional<MissEffect>& miss_effect() const;
    const optional<HitEffect>& hit_effect() const;
    const vector<DeathEffect>& death_effects() const;

    // Read-only presentation state used by MapMonitor. It is deliberately
    // separate from Entity::turn: game logic remains the authority on turns.
    bool turn_greyscale_enabled(int entity_id) const;

private:
    friend class EntityAnimation;

    RenderGrid terrain_;
    RenderGrid blue_;
    RenderGrid red_;
    unique_ptr<LogicRenderControl> control_;
    unordered_map<int, GuildColor> guild_colours_;
    unordered_set<int> turn_greyscale_ids_;

    struct HealthState
    {
        double displayed_hp = 0.0;
        int current_hp = 0;
        int maximum_hp = 1;
    };
    unordered_map<int, HealthState> health_;

    LogicRenderControl& control();
    const LogicRenderControl& control() const;
    void set_turn_greyscale(int entity_id, bool enabled);
    void stage_health_at_impact(const Entity& target, int hp_after);
    void require_dimensions(const RenderGrid& grid, const char* name) const;
    void apply_paint();

    // Values captured by your resolved battle(), then released only when its
    // corresponding visual strike reaches FE8's impact frame.
    unordered_map<int, int> staged_impact_hp_;
};
}
