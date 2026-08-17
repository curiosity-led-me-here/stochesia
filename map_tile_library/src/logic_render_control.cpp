#include "logic_render_control.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace fe_tiles
{
namespace
{
void face_unit_toward(UnitPose& pose, double target_x, double target_y)
{
    const double dx = target_x - pose.x;
    const double dy = target_y - pose.y;
    const double abs_dx = dx < 0.0 ? -dx : dx;
    const double abs_dy = dy < 0.0 ? -dy : dy;

    // Match FE8's cardinal-facing decision: vertical only wins when it is
    // clearly the larger component, so diagonal targets still read naturally.
    if (abs_dx * 2.0 < abs_dy)
    {
        pose.sheet_cell = dy > 0.0 ? 4 : 8; // down / up
        pose.flip_x = false;
        return;
    }

    pose.sheet_cell = 0;                   // left; right is this cell flipped
    pose.flip_x = dx > 0.0;
}
}

LogicRenderControl::LogicRenderControl(int rows, int columns)
    : occupancy_(rows > 0 ? rows : 1, std::vector<int>(columns > 0 ? columns : 1, 0)),
      sprites_(occupancy_),
      movement_(rows > 0 ? rows : 1, std::vector<int>(columns > 0 ? columns : 1, -1)),
      attack_(rows > 0 ? rows : 1, std::vector<int>(columns > 0 ? columns : 1, 0))
{
    if (rows <= 0 || columns <= 0)
    {
        throw std::invalid_argument("LogicRenderControl needs positive grid dimensions.");
    }
}

void LogicRenderControl::bind(Entity& entity, UnitVisual visual, GuildColor color)
{
    if (entity.entity_id <= 0)
    {
        throw std::invalid_argument("Bind a renderer only after the entity has a positive Registry ID.");
    }
    const auto existing = std::find_if(bindings_.begin(), bindings_.end(),
        [&entity](const Binding& binding) { return binding.entity_id == entity.entity_id; });
    if (existing == bindings_.end())
    {
        bindings_.push_back({entity.entity_id, visual, color});
    }
    else
    {
        existing->visual = visual;
        existing->color = color;
    }
    sprites_.register_unit(entity.entity_id, visual, color);
}

std::optional<LogicRenderControl::Binding> LogicRenderControl::binding_for(int entity_id) const
{
    const auto found = std::find_if(bindings_.begin(), bindings_.end(),
        [entity_id](const Binding& binding) { return binding.entity_id == entity_id; });
    if (found == bindings_.end())
    {
        return std::nullopt;
    }
    return *found;
}

void LogicRenderControl::sync_units(const std::vector<Entity*>& live_entities)
{
    if (sprites_.is_animating())
    {
        throw std::logic_error("Do not replace renderer occupancy while a movement animation is active.");
    }
    // A normal world-state refresh ends the temporary combat-facing hold.
    held_attacker_pose_.reset();
    held_defender_pose_.reset();
    hidden_after_death_ids_.clear();
    for (std::vector<int>& row : occupancy_)
    {
        std::fill(row.begin(), row.end(), 0);
    }

    std::unordered_set<int> seen;
    for (Entity* entity : live_entities)
    {
        if (entity == nullptr || !entity->alive)
        {
            continue;
        }
        if (entity->location.size() < 2)
        {
            throw std::invalid_argument("Live Entity location must have the form {x, y}.");
        }
        if (!binding_for(entity->entity_id).has_value())
        {
            throw std::invalid_argument("Every rendered Entity needs bind(entity, visual) first.");
        }
        const int x = entity->location[0];
        const int y = entity->location[1];
        if (x < 0 || y < 0 || y >= static_cast<int>(occupancy_.size()) ||
            x >= static_cast<int>(occupancy_[y].size()))
        {
            throw std::out_of_range("Entity location is outside the renderer grid.");
        }
        if (!seen.insert(entity->entity_id).second || occupancy_[y][x] != 0)
        {
            throw std::invalid_argument("Renderer occupancy requires one live Entity per tile and ID.");
        }
        occupancy_[y][x] = entity->entity_id;
    }
}

void LogicRenderControl::require_dimensions(const RenderGrid& grid, const char* name) const
{
    if (grid.size() != occupancy_.size())
    {
        throw std::invalid_argument(std::string(name) + " height does not match the renderer grid.");
    }
    for (std::size_t y = 0; y < grid.size(); ++y)
    {
        if (grid[y].size() != occupancy_[y].size())
        {
            throw std::invalid_argument(std::string(name) + " width does not match the renderer grid.");
        }
    }
}

void LogicRenderControl::show_action_state(const Entity& unit)
{
    show_action_state(unit.path, unit.attack_range);
}

void LogicRenderControl::show_action_state(const RenderGrid& movement, const RenderGrid& attack)
{
    require_dimensions(movement, "Movement grid");
    require_dimensions(attack, "Attack grid");
    movement_ = movement;
    attack_ = attack;
}

void LogicRenderControl::show_standing_attack(const RenderGrid& attack)
{
    require_dimensions(attack, "Standing attack grid");
    for (std::vector<int>& row : movement_)
    {
        std::fill(row.begin(), row.end(), -1);
    }
    attack_ = attack;
}

void LogicRenderControl::clear_action_state()
{
    for (std::vector<int>& row : movement_)
    {
        std::fill(row.begin(), row.end(), -1);
    }
    for (std::vector<int>& row : attack_)
    {
        std::fill(row.begin(), row.end(), 0);
    }
}

const RenderGrid& LogicRenderControl::movement_overlay() const
{
    return movement_;
}

const RenderGrid& LogicRenderControl::attack_overlay() const
{
    return attack_;
}

const OccupancyGrid& LogicRenderControl::occupancy() const
{
    return occupancy_;
}

bool LogicRenderControl::begin_move(const Entity& entity, const std::vector<Cell>& route)
{
    if (entity.location.size() < 2)
    {
        throw std::invalid_argument("Moving Entity location must have the form {x, y}.");
    }
    if (route.empty() || route.front().x != entity.location[0] || route.front().y != entity.location[1])
    {
        throw std::invalid_argument("Animation route must begin at Entity::location.");
    }
    return sprites_.begin_move(entity.entity_id, route);
}

bool LogicRenderControl::begin_move(
    const Entity& entity,
    const std::vector<std::vector<int>>& route_xy
)
{
    std::vector<Cell> route;
    route.reserve(route_xy.size());
    for (const std::vector<int>& coordinate : route_xy)
    {
        if (coordinate.size() != 2)
        {
            throw std::invalid_argument(
                "Each visual route coordinate must be exactly {x, y}."
            );
        }
        route.push_back({coordinate[0], coordinate[1]});
    }
    return begin_move(entity, route);
}

bool LogicRenderControl::begin_committed_move(
    const Entity& entity,
    const std::vector<std::vector<int>>& route_xy
)
{
    if (entity.location.size() < 2)
    {
        throw std::invalid_argument("Moving Entity location must have the form {x, y}.");
    }
    std::vector<Cell> route;
    route.reserve(route_xy.size());
    for (const std::vector<int>& coordinate : route_xy)
    {
        if (coordinate.size() != 2)
        {
            throw std::invalid_argument("Each visual route coordinate must be exactly {x, y}.");
        }
        route.push_back({coordinate[0], coordinate[1]});
    }
    if (route.empty() || route.back().x != entity.location[0] || route.back().y != entity.location[1])
    {
        throw std::invalid_argument("Committed animation route must end at Entity::location.");
    }
    return sprites_.begin_committed_move(entity.entity_id, route);
}

std::optional<CompletedMove> LogicRenderControl::take_completed_move()
{
    return sprites_.take_completed_move();
}

std::optional<UnitPose> LogicRenderControl::pose_for_entity(const Entity& entity) const
{
    if (entity.location.size() < 2)
    {
        return std::nullopt;
    }
    const std::optional<Binding> binding = binding_for(entity.entity_id);
    if (!binding.has_value())
    {
        return std::nullopt;
    }
    return UnitPose{entity.entity_id, binding->visual, binding->color,
                    static_cast<double>(entity.location[0]),
                    static_cast<double>(entity.location[1]), 12, false};
}

void LogicRenderControl::begin_attack(const Entity& attacker,
                                      const Entity& defender,
                                      bool attacker_defeated,
                                      bool defender_defeated)
{
    begin_strike(attacker, defender, StrikeOutcome::Hit,
                 attacker_defeated, defender_defeated);
}

void LogicRenderControl::begin_strike(const Entity& attacker,
                                      const Entity& defender,
                                      StrikeOutcome outcome,
                                      bool attacker_defeated,
                                      bool defender_defeated)
{
    if (sprites_.is_animating() || combat_.is_presenting())
    {
        throw std::logic_error("Wait for the current renderer animation before starting an attack.");
    }
    const std::optional<UnitPose> attacker_pose = pose_for_entity(attacker);
    const std::optional<UnitPose> defender_pose = pose_for_entity(defender);
    if (!attacker_pose.has_value() || !defender_pose.has_value())
    {
        throw std::invalid_argument("Both attack entities need an ID, {x,y} location, and bound visual.");
    }
    BattleWindow result;
    result.attacker_id = attacker.entity_id;
    result.defender_id = defender.entity_id;
    result.strike_outcome = outcome;
    result.attacker_defeated = attacker_defeated;
    result.defender_defeated = defender_defeated;
    combat_.begin(result, attacker_pose, defender_pose);
    held_attacker_pose_ = *attacker_pose;
    held_defender_pose_ = *defender_pose;
    face_unit_toward(*held_attacker_pose_, defender_pose->x, defender_pose->y);
    face_unit_toward(*held_defender_pose_, attacker_pose->x, attacker_pose->y);
    clear_action_state();
}

void LogicRenderControl::begin_death(const Entity& defeated)
{
    if (sprites_.is_animating() || combat_.is_presenting())
    {
        throw std::logic_error("Wait for the current renderer animation before starting a death fade.");
    }
    const std::optional<UnitPose> pose = pose_for_entity(defeated);
    if (!pose.has_value())
    {
        throw std::invalid_argument("A death animation needs an ID, {x,y} location, and bound visual.");
    }
    combat_.begin_death(*pose);
    held_attacker_pose_.reset();
    held_defender_pose_.reset();
    hidden_after_death_ids_.insert(pose->entity_id);
    clear_action_state();
}

void LogicRenderControl::tick_60hz(bool advance_combat)
{
    sprites_.tick_fe_frame();
    if (advance_combat)
    {
        combat_.tick_fe_frame();
    }
    // DeathEffect is born on the lethal impact frame. Keep that entity hidden
    // after its 32-frame fade expires, until game logic synchronises the new
    // live scene. This prevents a cached combat pose flashing back for a frame.
    for (const DeathEffect& effect : combat_.death_effects())
    {
        hidden_after_death_ids_.insert(effect.pose.entity_id);
    }
}

bool LogicRenderControl::is_moving() const
{
    return sprites_.is_animating();
}

bool LogicRenderControl::is_presenting_attack() const
{
    return combat_.is_presenting();
}

bool LogicRenderControl::is_busy() const
{
    return is_moving() || is_presenting_attack();
}

std::vector<UnitPose> LogicRenderControl::visible_unit_poses() const
{
    std::vector<UnitPose> result;
    const std::optional<AttackEffect>& attack = combat_.attack_effect();
    std::unordered_set<int> emitted;
    std::unordered_set<int> fading;
    for (const DeathEffect& effect : combat_.death_effects())
    {
        fading.insert(effect.pose.entity_id);
    }
    for (const UnitPose& pose : sprites_.poses())
    {
        if ((held_attacker_pose_.has_value() &&
             pose.entity_id == held_attacker_pose_->entity_id) ||
            (held_defender_pose_.has_value() &&
             pose.entity_id == held_defender_pose_->entity_id) ||
            hidden_after_death_ids_.find(pose.entity_id) != hidden_after_death_ids_.end() ||
            fading.find(pose.entity_id) != fading.end())
        {
            continue;
        }
        result.push_back(pose);
        emitted.insert(pose.entity_id);
    }
    if (held_defender_pose_.has_value() &&
        hidden_after_death_ids_.find(held_defender_pose_->entity_id) == hidden_after_death_ids_.end() &&
        fading.find(held_defender_pose_->entity_id) == fading.end())
    {
        result.push_back(*held_defender_pose_);
    }
    if (attack.has_value())
    {
        result.push_back(attack->pose);
    }
    else if (held_attacker_pose_.has_value() &&
             hidden_after_death_ids_.find(held_attacker_pose_->entity_id) == hidden_after_death_ids_.end() &&
             fading.find(held_attacker_pose_->entity_id) == fading.end())
    {
        result.push_back(*held_attacker_pose_);
    }
    return result;
}

const std::vector<DeathEffect>& LogicRenderControl::death_effects() const
{
    return combat_.death_effects();
}

const std::optional<MissEffect>& LogicRenderControl::miss_effect() const
{
    return combat_.miss_effect();
}

const std::optional<HitEffect>& LogicRenderControl::hit_effect() const
{
    return combat_.hit_effect();
}
}
