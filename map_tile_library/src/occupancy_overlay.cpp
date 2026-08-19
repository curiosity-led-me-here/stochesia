#include "occupancy_overlay.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace
{
constexpr int kQ4PerTile = 16 * 16;
constexpr int kFastWalkSpeedQ4 = 32; // FE8 default: 2 original pixels / tick.
constexpr int kStrikeImpactTick = 6;
constexpr int kMissPopupLifetime = 20;
constexpr int kFastLungeLifetime = 20;
// The lethal map hit already owns FE8's visible 17-frame white palette flash.
// Keep the frozen death pose for its birth frame only, so it hides the unit
// immediately without adding a second, lingering post-hit fade.
constexpr int kDeathFadeLifetime = 1;

enum class Facing
{
    Left = 0,
    Right = 1,
    Down = 2,
    Up = 3,
    Selected = 4,
};

struct MotionFrame
{
    int sheet_cell;
    bool flip_x;
    int duration_ticks;
};

// Exact FE8 moving-unit programs. Right uses the original left cells flipped.
constexpr std::array<std::array<MotionFrame, 4>, 5> kMotion = {{
    {{{0, false, 13}, {1, false, 6}, {2, false, 13}, {3, false, 6}}},
    {{{0, true,  13}, {1, true,  6}, {2, true,  13}, {3, true,  6}}},
    {{{4, false, 13}, {5, false, 6}, {6, false, 13}, {7, false, 6}}},
    {{{8, false, 13}, {9, false, 6}, {10, false, 13}, {11, false, 6}}},
    {{{12, false, 20}, {13, false, 4}, {14, false, 20}, {13, false, 4}}},
}};

Facing facing_from_delta(int dx, int dy)
{
    if (dx < 0) return Facing::Left;
    if (dx > 0) return Facing::Right;
    if (dy > 0) return Facing::Down;
    return Facing::Up;
}

Facing facing_toward(double from_x, double from_y, double to_x, double to_y)
{
    const double dx = to_x - from_x;
    const double dy = to_y - from_y;

    // Exact decision shape from FE8's GetFacingDirection(). This matters for
    // a long-range target that is not on one cardinal axis.
    if (std::abs(dx) * 2.0 < std::abs(dy))
    {
        return dy > 0.0 ? Facing::Down : Facing::Up;
    }
    return dx > 0.0 ? Facing::Right : Facing::Left;
}

void update_attack_pose(fe_tiles::AttackEffect& effect)
{
    const int dx = (effect.target_x > effect.origin_x) -
                   (effect.target_x < effect.origin_x);
    const int dy = (effect.target_y > effect.origin_y) -
                   (effect.target_y < effect.origin_y);
    int steps = 0;
    if (effect.tick <= 4)
    {
        steps = effect.tick;
    }
    else if (effect.tick <= 12)
    {
        steps = 4;
    }
    else if (effect.tick <= 16)
    {
        steps = 16 - effect.tick;
    }

    // MU positions are q4 (1/16 physical-pixel) values. Each map battle
    // lunge call moves q4 by 0x10: one original pixel, or 1/16 terrain tile.
    effect.pose.x = effect.origin_x + static_cast<double>(dx * steps) / 16.0;
    effect.pose.y = effect.origin_y + static_cast<double>(dy * steps) / 16.0;

    const Facing facing = facing_toward(
        effect.origin_x, effect.origin_y, effect.target_x, effect.target_y
    );
    // StartMuSpeedUpAnim changes the AP frame interval from 0x100 to 0x40.
    // The normal 13,6,13,6 map-unit program therefore runs at quarter time:
    // 3.25,1.5,3.25,1.5 frames. Preserve the fractional schedule instead of
    // reducing a combat lunge to two static pictures.
    constexpr int frame_duration_q6[] = {13 * 64, 6 * 64, 13 * 64, 6 * 64};
    constexpr int cycle_q6 = (13 + 6 + 13 + 6) * 64;
    int phase_q6 = (effect.tick * 64) % cycle_q6;
    int entry = 0;
    while (entry < 3 && phase_q6 >= frame_duration_q6[entry])
    {
        phase_q6 -= frame_duration_q6[entry];
        ++entry;
    }
    switch (facing)
    {
        case Facing::Left:
            effect.pose.sheet_cell = entry;
            effect.pose.flip_x = false;
            break;
        case Facing::Right:
            effect.pose.sheet_cell = entry;
            effect.pose.flip_x = true;
            break;
        case Facing::Down:
            effect.pose.sheet_cell = 4 + entry;
            effect.pose.flip_x = false;
            break;
        case Facing::Up:
            effect.pose.sheet_cell = 8 + entry;
            effect.pose.flip_x = false;
            break;
        default:
            break;
    }
}
}

namespace fe_tiles
{
void CombatPresentation::begin(BattleWindow result,
                               const std::optional<UnitPose>& attacker_pose,
                               const std::optional<UnitPose>& defender_pose)
{
    if (is_presenting())
    {
        throw std::logic_error("A map battle presentation is already active.");
    }

    pending_deaths_.clear();
    death_effects_.clear();
    miss_effect_.reset();
    hit_effect_.reset();
    pending_miss_ = false;
    pending_hit_ = false;
    pending_critical_ = false;
    target_pose_.reset();

    if (result.attacker_defeated && attacker_pose.has_value())
    {
        pending_deaths_.push_back({*attacker_pose});
    }
    if (result.defender_defeated && defender_pose.has_value())
    {
        pending_deaths_.push_back({*defender_pose});
    }

    if (attacker_pose.has_value() && defender_pose.has_value())
    {
        attack_effect_ = {
            *attacker_pose,
            attacker_pose->x,
            attacker_pose->y,
            defender_pose->x,
            defender_pose->y,
            0,
        };
        update_attack_pose(*attack_effect_);
        pending_miss_ = result.strike_outcome == StrikeOutcome::Miss;
        pending_hit_ = result.strike_outcome == StrikeOutcome::Hit ||
                       result.strike_outcome == StrikeOutcome::Critical;
        pending_critical_ = result.strike_outcome == StrikeOutcome::Critical;
        target_pose_ = defender_pose;
    }
}

void CombatPresentation::begin_death(UnitPose pose)
{
    if (is_presenting())
    {
        throw std::logic_error("A map battle presentation is already active.");
    }
    attack_effect_.reset();
    miss_effect_.reset();
    hit_effect_.reset();
    pending_miss_ = false;
    pending_hit_ = false;
    pending_critical_ = false;
    target_pose_.reset();
    pending_deaths_.clear();
    death_effects_.clear();
    death_effects_.push_back({pose, 0});
}

void CombatPresentation::tick_fe_frame()
{
    bool started_deaths_this_frame = false;
    if (attack_effect_.has_value())
    {
        ++attack_effect_->tick;

        // FE8 first performs four MapAnim_MoveSubjectsTowardsTarget calls,
        // waits for its brief camera delay, then invokes
        // MapAnim_BeginRoundSpecificAnims. A miss calls
        // MapAnim_BeginMISSAnim(target) at that impact point. This compact
        // presenter shows the popup after its four inward steps plus that
        // two-frame gap, while the attacker is still held at the lunge peak.
        if (pending_miss_ && attack_effect_->tick >= kStrikeImpactTick)
        {
            miss_effect_ = {
                attack_effect_->target_x,
                attack_effect_->target_y,
                0,
            };
            pending_miss_ = false;
        }
        if (pending_hit_ && attack_effect_->tick >= kStrikeImpactTick &&
            target_pose_.has_value())
        {
            hit_effect_ = {*target_pose_, 0, pending_critical_};
            pending_hit_ = false;
            pending_critical_ = false;
        }

        // A lethal result begins its fade on the impact frame, not after the
        // attacker has completed the return part of the lunge.
        if (attack_effect_->tick >= kStrikeImpactTick && !pending_deaths_.empty())
        {
            for (const PendingDeath& pending : pending_deaths_)
            {
                death_effects_.push_back({pending.pose, 0});
            }
            pending_deaths_.clear();
            started_deaths_this_frame = true;
        }

        if (attack_effect_->tick >= kFastLungeLifetime)
        {
            attack_effect_.reset();
        }
        else
        {
            update_attack_pose(*attack_effect_);
        }
    }

    if (miss_effect_.has_value())
    {
        ++miss_effect_->tick;
        if (miss_effect_->tick >= kMissPopupLifetime)
        {
            miss_effect_.reset();
        }
    }

    if (hit_effect_.has_value())
    {
        ++hit_effect_->tick;
        // FE8's ordinary hit palette restores after 17 ticks. Criticals
        // alternate palettes first, shake for 12 ticks, then finish their
        // 17-tick fade-back phase.
        const int lifetime = hit_effect_->critical ? 29 : 17;
        if (hit_effect_->tick >= lifetime)
        {
            hit_effect_.reset();
        }
    }

    if (!attack_effect_.has_value() && !pending_deaths_.empty())
    {
        for (const PendingDeath& pending : pending_deaths_)
        {
            death_effects_.push_back({pending.pose, 0});
        }
        pending_deaths_.clear();
        started_deaths_this_frame = true;
    }

    if (started_deaths_this_frame)
    {
        return;
    }

    for (DeathEffect& effect : death_effects_)
    {
        ++effect.tick;
    }
    death_effects_.erase(
        std::remove_if(death_effects_.begin(), death_effects_.end(),
            [](const DeathEffect& effect)
            {
                return effect.tick >= kDeathFadeLifetime;
            }),
        death_effects_.end()
    );
}

bool CombatPresentation::is_presenting() const
{
    return attack_effect_.has_value() || pending_miss_ || pending_hit_ || miss_effect_.has_value() ||
           hit_effect_.has_value() || !pending_deaths_.empty() || !death_effects_.empty();
}

const std::optional<AttackEffect>& CombatPresentation::attack_effect() const
{
    return attack_effect_;
}

const std::optional<MissEffect>& CombatPresentation::miss_effect() const
{
    return miss_effect_;
}

const std::optional<HitEffect>& CombatPresentation::hit_effect() const
{
    return hit_effect_;
}

const std::vector<DeathEffect>& CombatPresentation::death_effects() const
{
    return death_effects_;
}

struct SpriteLayer::MotionState
{
    int entity_id = 0;
    UnitVisual visual = UnitVisual::Soldier;
    Facing facing = Facing::Selected;
    int frame_entry = 0;
    int frame_ticks = 0;
    bool moving = false;
    std::vector<Cell> route;
    std::size_t route_step = 0;
    int x_q4 = 0;
    int y_q4 = 0;
};

SpriteLayer::SpriteLayer(OccupancyGrid& occupancy)
    : occupancy_(occupancy)
{
    validate_occupancy();
}

SpriteLayer::~SpriteLayer() = default;

void SpriteLayer::validate_occupancy() const
{
    if (occupancy_.empty() || occupancy_.front().empty())
    {
        throw std::invalid_argument("Occupancy grid must be non-empty.");
    }
    const std::size_t width = occupancy_.front().size();
    std::unordered_set<int> seen;
    for (const std::vector<int>& row : occupancy_)
    {
        if (row.size() != width)
        {
            throw std::invalid_argument("Occupancy grid must be rectangular.");
        }
        for (const int entity_id : row)
        {
            if (entity_id < 0)
            {
                throw std::invalid_argument("Occupancy entity IDs cannot be negative.");
            }
            if (entity_id > 0 && !seen.insert(entity_id).second)
            {
                throw std::invalid_argument(
                    "Each entity ID may appear only once in occupancy."
                );
            }
        }
    }
}

void SpriteLayer::register_unit(int entity_id, UnitVisual visual, GuildColor color)
{
    if (entity_id <= 0)
    {
        throw std::invalid_argument("A unit sprite needs a positive entity ID.");
    }
    const auto existing = std::find_if(definitions_.begin(), definitions_.end(),
        [entity_id](const Definition& unit) { return unit.entity_id == entity_id; });
    if (existing != definitions_.end())
    {
        existing->visual = visual;
        existing->color = color;
        return;
    }
    definitions_.push_back({entity_id, visual, color});
    MotionState state;
    state.entity_id = entity_id;
    state.visual = visual;
    states_.push_back(state);
}

std::optional<Cell> SpriteLayer::location_of(int entity_id) const
{
    for (std::size_t y = 0; y < occupancy_.size(); ++y)
    {
        for (std::size_t x = 0; x < occupancy_[y].size(); ++x)
        {
            if (occupancy_[y][x] == entity_id)
            {
                return Cell{static_cast<int>(x), static_cast<int>(y)};
            }
        }
    }
    return std::nullopt;
}

bool SpriteLayer::begin_move(int entity_id, const std::vector<Cell>& route)
{
    validate_occupancy();
    if (moving_entity_.has_value() || route.size() < 2)
    {
        return false;
    }
    const auto source = location_of(entity_id);
    if (!source.has_value() || route.front().x != source->x || route.front().y != source->y)
    {
        return false;
    }
    const int height = static_cast<int>(occupancy_.size());
    const int width = static_cast<int>(occupancy_.front().size());
    for (std::size_t index = 1; index < route.size(); ++index)
    {
        const Cell& previous = route[index - 1];
        const Cell& current = route[index];
        if (current.x < 0 || current.x >= width || current.y < 0 || current.y >= height ||
            std::abs(current.x - previous.x) + std::abs(current.y - previous.y) != 1 ||
            occupancy_[current.y][current.x] != 0)
        {
            return false;
        }
    }
    auto state = std::find_if(states_.begin(), states_.end(),
        [entity_id](const MotionState& value) { return value.entity_id == entity_id; });
    if (state == states_.end())
    {
        return false; // A game entity needs an explicit art mapping to render.
    }

    state->moving = true;
    state->route = route;
    state->route_step = 0;
    state->x_q4 = source->x * kQ4PerTile;
    state->y_q4 = source->y * kQ4PerTile;
    state->facing = facing_from_delta(route[1].x - route[0].x, route[1].y - route[0].y);
    state->frame_entry = 0;
    state->frame_ticks = 0;
    moving_entity_ = entity_id;
    committed_move_ = false;
    return true;
}

bool SpriteLayer::begin_committed_move(int entity_id, const std::vector<Cell>& route)
{
    validate_occupancy();
    if (moving_entity_.has_value() || route.size() < 2)
    {
        return false;
    }

    const auto destination = location_of(entity_id);
    if (!destination.has_value() || route.back().x != destination->x || route.back().y != destination->y)
    {
        return false;
    }

    const int height = static_cast<int>(occupancy_.size());
    const int width = static_cast<int>(occupancy_.front().size());
    for (std::size_t index = 1; index < route.size(); ++index)
    {
        const Cell& previous = route[index - 1];
        const Cell& current = route[index];
        if (previous.x < 0 || previous.x >= width || previous.y < 0 || previous.y >= height ||
            current.x < 0 || current.x >= width || current.y < 0 || current.y >= height ||
            std::abs(current.x - previous.x) + std::abs(current.y - previous.y) != 1)
        {
            return false;
        }
    }

    auto state = std::find_if(states_.begin(), states_.end(),
        [entity_id](const MotionState& value) { return value.entity_id == entity_id; });
    if (state == states_.end() || occupancy_[route.front().y][route.front().x] != 0)
    {
        return false;
    }

    occupancy_[destination->y][destination->x] = 0;
    occupancy_[route.front().y][route.front().x] = entity_id;
    state->moving = true;
    state->route = route;
    state->route_step = 0;
    state->x_q4 = route.front().x * kQ4PerTile;
    state->y_q4 = route.front().y * kQ4PerTile;
    state->facing = facing_from_delta(route[1].x - route[0].x, route[1].y - route[0].y);
    state->frame_entry = 0;
    state->frame_ticks = 0;
    moving_entity_ = entity_id;
    committed_move_ = true;
    return true;
}

bool SpriteLayer::is_animating() const
{
    return moving_entity_.has_value();
}

void SpriteLayer::tick_fe_frame()
{
    for (MotionState& state : states_)
    {
        const MotionFrame& frame = kMotion[static_cast<int>(state.facing)][state.frame_entry];
        ++state.frame_ticks;
        if (state.frame_ticks >= frame.duration_ticks)
        {
            state.frame_ticks = 0;
            state.frame_entry = (state.frame_entry + 1) % 4;
        }

        if (!state.moving)
        {
            continue;
        }

        const Cell& target = state.route[state.route_step + 1];
        const int target_x_q4 = target.x * kQ4PerTile;
        const int target_y_q4 = target.y * kQ4PerTile;
        const int dx = (target_x_q4 > state.x_q4) - (target_x_q4 < state.x_q4);
        const int dy = (target_y_q4 > state.y_q4) - (target_y_q4 < state.y_q4);
        state.x_q4 += dx * kFastWalkSpeedQ4;
        state.y_q4 += dy * kFastWalkSpeedQ4;

        const bool reached_x = dx == 0 || (target_x_q4 - state.x_q4) * dx <= 0;
        const bool reached_y = dy == 0 || (target_y_q4 - state.y_q4) * dy <= 0;
        if (!reached_x || !reached_y)
        {
            continue;
        }

        state.x_q4 = target_x_q4;
        state.y_q4 = target_y_q4;
        ++state.route_step;
        if (state.route_step + 1 < state.route.size())
        {
            const Cell& from = state.route[state.route_step];
            const Cell& to = state.route[state.route_step + 1];
            const Facing next_facing = facing_from_delta(to.x - from.x, to.y - from.y);
            if (next_facing != state.facing)
            {
                state.facing = next_facing;
                state.frame_entry = 0;
                state.frame_ticks = 0;
            }
            continue;
        }

        // Presentation has arrived. Keep the actual occupancy owned by the
        // caller; it receives this event and decides when/how to commit the
        // gameplay move. Until now poses() hid the old static occupancy pose.
        const Cell& origin = state.route.front();
        const Cell& destination = state.route.back();
        if (committed_move_)
        {
            occupancy_[origin.y][origin.x] = 0;
            occupancy_[destination.y][destination.x] = state.entity_id;
        }
        completed_move_ = CompletedMove{state.entity_id, origin, destination};
        state.moving = false;
        state.route.clear();
        state.route_step = 0;
        state.facing = Facing::Selected;
        state.frame_entry = 0;
        state.frame_ticks = 0;
        committed_move_ = false;
        moving_entity_.reset();
    }
}

std::optional<CompletedMove> SpriteLayer::take_completed_move()
{
    const std::optional<CompletedMove> result = completed_move_;
    completed_move_.reset();
    return result;
}

std::vector<UnitPose> SpriteLayer::poses() const
{
    validate_occupancy();
    std::vector<UnitPose> result;
    std::unordered_set<int> emitted;

    const auto definition_for = [this](int entity_id) -> const Definition*
    {
        const auto it = std::find_if(definitions_.begin(), definitions_.end(),
            [entity_id](const Definition& unit) { return unit.entity_id == entity_id; });
        return it == definitions_.end() ? nullptr : &*it;
    };
    const auto state_for = [this](int entity_id) -> const MotionState*
    {
        const auto it = std::find_if(states_.begin(), states_.end(),
            [entity_id](const MotionState& state) { return state.entity_id == entity_id; });
        return it == states_.end() ? nullptr : &*it;
    };

    for (std::size_t y = 0; y < occupancy_.size(); ++y)
    {
        for (std::size_t x = 0; x < occupancy_[y].size(); ++x)
        {
            const int entity_id = occupancy_[y][x];
            if (entity_id == 0 || (moving_entity_.has_value() && entity_id == *moving_entity_))
            {
                continue;
            }
            const auto* definition = definition_for(entity_id);
            const auto* state = state_for(entity_id);
            if (definition == nullptr || state == nullptr || !emitted.insert(entity_id).second)
            {
                continue;
            }
            const MotionFrame& frame = kMotion[static_cast<int>(state->facing)][state->frame_entry];
            result.push_back({entity_id, definition->visual, definition->color,
                              static_cast<double>(x), static_cast<double>(y),
                              frame.sheet_cell, frame.flip_x});
        }
    }

    if (moving_entity_.has_value())
    {
        const auto* state = state_for(*moving_entity_);
        const auto* definition = definition_for(*moving_entity_);
        if (state != nullptr && definition != nullptr)
        {
            const MotionFrame& frame = kMotion[static_cast<int>(state->facing)][state->frame_entry];
            result.push_back({state->entity_id, definition->visual, definition->color,
                              static_cast<double>(state->x_q4) / kQ4PerTile,
                              static_cast<double>(state->y_q4) / kQ4PerTile,
                              frame.sheet_cell, frame.flip_x});
        }
    }
    return result;
}
}
