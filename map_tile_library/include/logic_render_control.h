#pragma once

#include <optional>
#include <unordered_set>
#include <vector>

#include "occupancy_overlay.h"

// This is intentionally the only dependency on Ashu's game module. The
// renderer reads Entity state; it never calls path_trace(), prompt_attack(),
// move(), interact(), or battle().
#include "game_types.h"

namespace fe_tiles
{
using RenderGrid = std::vector<std::vector<int>>;

// This class is the public gameplay-to-renderer boundary.
//
// Call sequence from your logic:
//
//   map.path_trace(unit);
//   map.attack_range(unit);
//   renderer.show_action_state(unit);       // blue path, red-only attack
//   renderer.begin_move(unit, route);       // route from YOUR path logic
//   while (renderer.is_busy()) renderer.tick_60hz();
//   unit.location = destination;            // YOUR commit
//   renderer.sync_units(live_units);
//   ... resolve one strike in YOUR mechanics ...
//   renderer.begin_strike(attacker, defender, StrikeOutcome::Miss);
//
// The API deliberately does not receive Mapmaker. Your Mapmaker remains the
// sole owner of terrain, guild occupancy, movement legality, target prompts,
// combat, and death. This class only turns already-resolved game state into a
// stable frame model for a GUI renderer.
class LogicRenderControl
{
public:
    LogicRenderControl(int rows, int columns);

    // Link an entity ID to its FE map-sprite sheet. You normally do this once
    // after Registry::spawn(). Calling it again changes only the visual.
    void bind(Entity& entity, UnitVisual visual,
              GuildColor color = GuildColor::player());

    // Rebuild the renderer's private occupancy layer from the live Entity
    // locations. It does not modify Entity::location, Guild, Registry, or
    // Mapmaker occupancy. Dead units are excluded.
    void sync_units(const std::vector<Entity*>& live_entities);

    // Directly consumes the output of YOUR Mapmaker calls:
    //   unit.path         -> blue when value >= 0
    //   unit.attack_range -> red only when the same tile is not blue
    // Passing explicit grids supports a standing-only attack matrix too.
    void show_action_state(const Entity& unit);
    void show_action_state(const RenderGrid& movement, const RenderGrid& attack);
    void show_standing_attack(const RenderGrid& attack);
    void clear_action_state();

    const RenderGrid& movement_overlay() const;
    const RenderGrid& attack_overlay() const;
    const OccupancyGrid& occupancy() const;

    // `route` must be YOUR already-approved ordered tile route, in {x,y}
    // cells and starting at entity.location. It only starts the 60 Hz visual
    // movement; it never commits the gameplay move.
    bool begin_move(const Entity& entity, const std::vector<Cell>& route);

    // Convenience overload for your current route representation:
    // std::vector<std::vector<int>>{{x0,y0}, {x1,y1}, ...}.
    // It does no pathfinding or path validation beyond checking that each
    // coordinate really is an {x,y} pair; the Cell overload remains the
    // canonical presentation API.
    bool begin_move(const Entity& entity,
                    const std::vector<std::vector<int>>& route_xy);

    // Use this only after your own Mapmaker::move() has already committed its
    // occupancy and Entity::location. The route remains {{x,y}, ...} from the
    // old tile to the new tile; no gameplay state is changed or rewound.
    bool begin_committed_move(const Entity& entity,
                              const std::vector<std::vector<int>>& route_xy);
    std::optional<CompletedMove> take_completed_move();

    // One normal hit. A hit never starts a death fade; schedule begin_death()
    // yourself after the final resolved round has completed.
    void begin_attack(const Entity& attacker,
                      const Entity& defender);

    // The one-strike form used for an exact combat sequence. Your mechanics
    // decides whether this round hit or missed and calls it once per round in
    // order. That directly supports counters, doubles, and brave weapons,
    // without the renderer reconstructing attack order from final HP values.
    // A strike never infers or starts death. FE8 starts MU_StartDeathFade only
    // after the final round script has returned and MapAnimEnd has begun.
    void begin_strike(const Entity& attacker,
                      const Entity& defender,
                      StrikeOutcome outcome);

    // A standalone map-unit death fade. For combat, call this only after the
    // final hit/critical presentation and post-round pause; it is also used
    // for event death, scripted removal, and terrain hazards.
    // The renderer intentionally does not change Entity::alive or occupancy.
    void begin_death(const Entity& defeated);

    // Advance exactly one FE8 map frame. Your outer loop determines when this
    // is called, which means the caller fully controls pause, speed-up, and
    // playback. Call it at 60 Hz for faithful default timing.
    // Movement always advances once. Set `advance_combat` false to hold the
    // current lunge/MISS/hit/death frame while another frontend tick occurs.
    // MapMonitor uses this to slow battle effects without slowing movement.
    void tick_60hz(bool advance_combat = true);
    bool is_moving() const;
    bool is_presenting_attack() const;
    bool is_busy() const;

    // What a view draws each frame. This list is already deduplicated: a
    // moving/attacking entity does not also appear at a static tile. During
    // an attack it preserves the defender snapshot even if your battle() has
    // already removed that unit from live occupancy.
    std::vector<UnitPose> visible_unit_poses() const;
    const std::optional<MissEffect>& miss_effect() const;
    const std::optional<HitEffect>& hit_effect() const;
    const std::vector<DeathEffect>& death_effects() const;

private:
    OccupancyGrid occupancy_;
    SpriteLayer sprites_;
    CombatPresentation combat_;
    RenderGrid movement_;
    RenderGrid attack_;
    struct Binding
    {
        int entity_id = 0;
        UnitVisual visual = UnitVisual::Soldier;
        GuildColor color = GuildColor::player();
    };

    std::vector<Binding> bindings_;
    std::optional<UnitPose> held_attacker_pose_;
    std::optional<UnitPose> held_defender_pose_;
    std::unordered_set<int> hidden_after_death_ids_;

    void require_dimensions(const RenderGrid& grid, const char* name) const;
    std::optional<Binding> binding_for(int entity_id) const;
    std::optional<UnitPose> pose_for_entity(const Entity& entity) const;
};
}
