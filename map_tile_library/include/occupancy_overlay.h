#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "fe8_unit_visuals.h"

namespace fe_tiles
{
// Indexed [y][x], like TileCanvas's class and visual grids. 0 is empty;
// every positive value is an entity ID from your Registry/occupancy system.
using OccupancyGrid = std::vector<std::vector<int>>;

struct Cell
{
    int x = 0;
    int y = 0;
};

// UnitVisual is the generated full FE8 map-unit vocabulary declared in
// fe8_unit_visuals.h. The art lookup stays separate from entity IDs, so
// occupancy remains pure gameplay data: occupancy[y][x] = entity_id.

// A map unit uses one 16-colour OBJ palette.  The four named values select
// FE8's original palette files; Custom recolours only the indices that FE8
// itself changes between player/enemy/NPC/fourth-team palettes.  `rgb` is
// 0xRRGGBB and is used only for Custom.
struct GuildColor
{
    enum class Scheme
    {
        Player,
        Enemy,
        Npc,
        Fourth,
        Custom,
    };

    Scheme scheme = Scheme::Player;
    std::uint32_t rgb = 0x4A78E8;

    static constexpr GuildColor player() { return {Scheme::Player, 0x4A78E8}; }
    static constexpr GuildColor enemy()  { return {Scheme::Enemy,  0xD83232}; }
    static constexpr GuildColor npc()    { return {Scheme::Npc,    0x46A84A}; }
    static constexpr GuildColor fourth() { return {Scheme::Fourth, 0x9A5CD6}; }
    static constexpr GuildColor custom(std::uint32_t rgb_code)
    {
        return {Scheme::Custom, rgb_code & 0x00FFFFFF};
    }
};

struct UnitPose
{
    int entity_id = 0;
    UnitVisual visual = UnitVisual::Soldier;
    GuildColor color = GuildColor::player();
    double x = 0.0; // Logical tile coordinate; fractional while moving.
    double y = 0.0;
    int sheet_cell = 12;
    bool flip_x = false;
};

// Renderer-only state for the compact bar drawn above a map-unit sprite.
// `displayed_hp` moves toward `current_hp` once per renderer frame, while
// gameplay continues to own Entity::stats.HP and the timing of sync_units().
struct HealthBar
{
    int entity_id = 0;
    UnitVisual visual = UnitVisual::Soldier;
    GuildColor color = GuildColor::player();
    int sheet_cell = 12;
    double x = 0.0;
    double y = 0.0;
    double displayed_hp = 0.0;
    int current_hp = 0;
    int maximum_hp = 1;
};

struct CompletedMove
{
    int entity_id = 0;
    Cell from;
    Cell to;
};

// One resolved strike from your mechanics. A complete combat exchange can
// contain several of these (counterattack, doubles, brave weapons, etc.).
// The renderer never rolls hit chance: it only presents the outcome supplied
// by your combat code.
enum class StrikeOutcome
{
    Hit,
    Miss,
    Critical,
};

// Renderer input after your mechanics layer has resolved a battle. This is
// deliberately a record of the result, not a combat simulator: Mapmaker,
// Entity, and battle() remain the only authority on damage, hit, death, and
// occupancy.
struct BattleWindow
{
    int attacker_id = 0;
    int defender_id = 0;
    std::string attacker_name;
    std::string defender_name;
    std::string attacker_weapon;
    std::string defender_weapon;
    int attacker_hp_before = 0;
    int attacker_hp_after = 0;
    int attacker_hp_max = 0;
    int defender_hp_before = 0;
    int defender_hp_after = 0;
    int defender_hp_max = 0;
    int attacker_damage = 0;
    int attacker_hit = 0;
    int attacker_crit = 0;
    int attacker_attack_speed = -1;
    int defender_damage = 0;
    int defender_hit = 0;
    int defender_crit = 0;
    int defender_attack_speed = -1;
    StrikeOutcome strike_outcome = StrikeOutcome::Hit;
    bool attacker_defeated = false;
    bool defender_defeated = false;
};

// A frozen map-unit pose kept after the game object has removed the defeated
// entity. FE8's actual map death path (MU_StartDeathFade) freezes the unit
// and turns it white; this renderer then gives that frozen pose a readable
// monitor-friendly fade-out.
struct DeathEffect
{
    UnitPose pose;
    int tick = 0;
};

// The four-frame map-unit lunge used by FE8's map battle. The attacker moves
// one original pixel (1/16 of a terrain tile) toward the defender on each of
// the first four 60 Hz frames, holds briefly for the hit, then returns in
// four matching steps. `pose` is the only pose the renderer should draw for
// this entity while the effect exists.
struct AttackEffect
{
    UnitPose pose;
    double origin_x = 0.0;
    double origin_y = 0.0;
    double target_x = 0.0;
    double target_y = 0.0;
    int tick = 0;
};

// FE8 draws this independently of the lunge. The original engine calls
// MapAnim_BeginMISSAnim(target) when the current BattleHit has the MISS
// attribute. The anchor is the horizontal centre / bottom edge of the target
// map tile. A frontend can render the authentic MISS OBJ asset (or any
// equivalent popup) from this position and tick.
struct MissEffect
{
    double x = 0.0; // Logical target tile coordinate.
    double y = 0.0;
    int tick = 0;
};

// Normal FE8 map hits call StartMuHitFlash on the target. The original effect
// uses a temporary palette and restores it after 17 ticks. We retain the
// target pose and tick so a frontend can draw that flash independently from
// the attacker lunge.
struct HitEffect
{
    UnitPose pose;
    int tick = 0;
    bool critical = false;
};

// Pure presentation state for FE8-style map combat with battle animations
// disabled. Feed it a BattleWindow created from your own mechanics and tick
// it at 60 Hz. It emits the map-unit lunge, MISS popup, and map-unit death
// fade; it never reads or writes occupancy, Entity, Registry, Guild,
// Mapmaker, or combat state.
class CombatPresentation
{
public:
    void begin(BattleWindow result,
               const std::optional<UnitPose>& attacker_pose,
               const std::optional<UnitPose>& defender_pose);
    void begin_death(UnitPose pose);
    void tick_fe_frame();

    bool is_presenting() const;
    const std::optional<AttackEffect>& attack_effect() const;
    const std::optional<MissEffect>& miss_effect() const;
    const std::optional<HitEffect>& hit_effect() const;
    const std::vector<DeathEffect>& death_effects() const;

private:
    struct PendingDeath
    {
        UnitPose pose;
    };

    std::optional<AttackEffect> attack_effect_;
    std::optional<MissEffect> miss_effect_;
    std::optional<HitEffect> hit_effect_;
    bool pending_miss_ = false;
    bool pending_hit_ = false;
    bool pending_critical_ = false;
    std::optional<UnitPose> target_pose_;
    std::vector<PendingDeath> pending_deaths_;
    std::vector<DeathEffect> death_effects_;
};

// Renderer-side FE8 map-unit animation state. It has no knowledge of combat,
// movement cost, path finding, Guild, Registry, or terrain. Give begin_move()
// the route already approved by your Mapmaker logic.
//
// One entity can appear in occupancy only once. While that entity is moving,
// poses() deliberately suppresses its static occupancy pose and emits its
// fractional pose instead. This is the invariant that prevents ghost/doubled
// units on a tile.
class SpriteLayer
{
public:
    explicit SpriteLayer(OccupancyGrid& occupancy);
    ~SpriteLayer();

    void register_unit(int entity_id, UnitVisual visual,
                       GuildColor color = GuildColor::player());
    std::optional<Cell> location_of(int entity_id) const;

    // `route` must start at the entity's present occupancy cell, contain
    // cardinally adjacent cells, and end on an empty cell. This is entirely
    // presentation state: SpriteLayer never changes your occupancy grid.
    // Returns false if a move is already animating or the route is bad.
    bool begin_move(int entity_id, const std::vector<Cell>& route);

    // Same visual animation, for a gameplay move that was already committed.
    // The renderer currently contains the entity at route.back(); it briefly
    // presents it from route.front() and restores its private occupancy at the
    // destination when the animation completes.
    bool begin_committed_move(int entity_id, const std::vector<Cell>& route);

    // Call exactly once per displayed FE8 60 Hz frame.
    void tick_fe_frame();

    // Returns an arrival exactly once. The caller that owns game state should
    // then update occupancy (or call its own Mapmaker movement commit).
    std::optional<CompletedMove> take_completed_move();

    bool is_animating() const;
    std::vector<UnitPose> poses() const;

private:
    struct MotionState;

    OccupancyGrid& occupancy_;
    struct Definition
    {
        int entity_id = 0;
        UnitVisual visual = UnitVisual::Soldier;
        GuildColor color = GuildColor::player();
    };

    std::vector<Definition> definitions_;
    std::vector<MotionState> states_;
    std::optional<int> moving_entity_;
    std::optional<CompletedMove> completed_move_;
    bool committed_move_ = false;

    void validate_occupancy() const;
};
}
