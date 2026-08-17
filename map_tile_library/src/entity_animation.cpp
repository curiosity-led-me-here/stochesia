#include "entity_animation.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "pathfinder.h"

namespace fe_tiles
{
EntityAnimation::EntityAnimation(AnimationRenderer& renderer, Entity& entity)
    : renderer_(&renderer), entity_(&entity)
{
}

bool EntityAnimation::move(const std::vector<std::vector<int>>& coords)
{
    return renderer_->control().begin_move(*entity_, coords);
}

bool EntityAnimation::play_committed_move(const std::vector<std::vector<int>>& coords)
{
    return renderer_->control().begin_committed_move(*entity_, coords);
}

void EntityAnimation::paint_blue(const RenderGrid& path)
{
    renderer_->require_dimensions(path, "Blue path");
    renderer_->blue_ = path;
    renderer_->apply_paint();
}

void EntityAnimation::paint_red(const RenderGrid& path)
{
    renderer_->require_dimensions(path, "Red path");
    renderer_->red_ = path;
    renderer_->apply_paint();
}

void EntityAnimation::paint_blue()
{
    paint_blue(entity_->path);
}

void EntityAnimation::paint_red()
{
    paint_red(entity_->attack_range);
}

void EntityAnimation::clear_paint()
{
    renderer_->clear_paint();
}

void EntityAnimation::turn_greyscale(bool enabled)
{
    renderer_->set_turn_greyscale(entity_->entity_id, enabled);
}

bool EntityAnimation::is_turn_greyscale() const
{
    return renderer_->turn_greyscale_enabled(entity_->entity_id);
}

void EntityAnimation::dash(Entity& target,
                            bool attacker_defeated,
                            bool target_defeated)
{
    renderer_->control().begin_strike(
        *entity_, target, StrikeOutcome::Hit, attacker_defeated, target_defeated
    );
}

void EntityAnimation::dash(Entity& target,
                            int target_hp_after,
                            bool attacker_defeated,
                            bool target_defeated)
{
    renderer_->stage_health_at_impact(target, target_hp_after);
    renderer_->control().begin_strike(
        *entity_, target, StrikeOutcome::Hit, attacker_defeated, target_defeated
    );
}

void EntityAnimation::critical(Entity& target,
                                int target_hp_after,
                                bool attacker_defeated,
                                bool target_defeated)
{
    renderer_->stage_health_at_impact(target, target_hp_after);
    renderer_->control().begin_strike(
        *entity_, target, StrikeOutcome::Critical, attacker_defeated, target_defeated
    );
}

void EntityAnimation::miss(Entity& target)
{
    renderer_->control().begin_strike(*entity_, target, StrikeOutcome::Miss);
}

void EntityAnimation::death()
{
    renderer_->control().begin_death(*entity_);
}

Entity& EntityAnimation::entity() const
{
    return *entity_;
}

void AnimationRenderer::load_map(Mapmaker& map)
{
    terrain_ = map.get_map();
    if (terrain_.empty() || terrain_.front().empty())
    {
        throw std::invalid_argument("load_map(): Mapmaker returned an empty map.");
    }
    const std::size_t columns = terrain_.front().size();
    for (const RenderGrid::value_type& row : terrain_)
    {
        if (row.size() != columns)
        {
            throw std::invalid_argument("load_map(): Mapmaker map must be rectangular.");
        }
    }

    blue_.assign(terrain_.size(), std::vector<int>(columns, -1));
    red_.assign(terrain_.size(), std::vector<int>(columns, 0));
    control_ = std::make_unique<LogicRenderControl>(
        static_cast<int>(terrain_.size()), static_cast<int>(columns)
    );
}

bool AnimationRenderer::has_map() const
{
    return control_ != nullptr;
}

const RenderGrid& AnimationRenderer::terrain_ids() const
{
    if (!has_map())
    {
        throw std::logic_error("Call load_map(map) before reading terrain IDs.");
    }
    return terrain_;
}

EntityAnimation AnimationRenderer::entity(Entity& unit, UnitVisual visual, GuildColor color)
{
    if (unit.group != nullptr)
    {
        const auto guild_color = guild_colours_.find(unit.group->guild_id);
        if (guild_color != guild_colours_.end())
        {
            color = guild_color->second;
        }
    }
    control().bind(unit, visual, color);
    return EntityAnimation(*this, unit);
}

EntityAnimation AnimationRenderer::entity_for_fe8_class(Entity& unit,
                                                         int fe8_class_id,
                                                         GuildColor color)
{
    const std::optional<UnitVisual> visual = unit_visual_for_class(fe8_class_id);
    if (!visual.has_value())
    {
        throw std::invalid_argument(
            "No literal FE8 map-unit visual exists for class ID "
            + std::to_string(fe8_class_id) + "."
        );
    }
    return entity(unit, *visual, color);
}

void AnimationRenderer::set_guild_color(const Guild& guild, GuildColor color)
{
    if (guild.guild_id == 0)
    {
        throw std::invalid_argument("Guild ID zero is reserved and cannot receive a renderer colour.");
    }
    guild_colours_[guild.guild_id] = color;
}

void AnimationRenderer::set_guild_color(const Guild& guild, std::uint32_t rgb_code)
{
    set_guild_color(guild, GuildColor::custom(rgb_code));
}

void AnimationRenderer::sync_units(const std::vector<Entity*>& live_entities)
{
    std::unordered_set<int> still_present;
    for (Entity* entity : live_entities)
    {
        if (entity == nullptr || !entity->alive)
        {
            continue;
        }

        const int maximum_hp = std::max(1, entity->ogstats.HP);
        const int current_hp = std::clamp(entity->stats.HP, 0, maximum_hp);
        still_present.insert(entity->entity_id);

        const auto existing = health_.find(entity->entity_id);
        if (existing == health_.end())
        {
            // First sighting has no prior value to animate from.
            health_.emplace(entity->entity_id, HealthState{
                static_cast<double>(current_hp), current_hp, maximum_hp
            });
        }
        else
        {
            existing->second.current_hp = current_hp;
            existing->second.maximum_hp = maximum_hp;
        }
    }

    for (auto it = health_.begin(); it != health_.end();)
    {
        if (still_present.find(it->first) == still_present.end())
        {
            it = health_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // This is presentation state only. Do not retain an acted appearance for
    // an entity your game has removed from the live scene.
    for (auto it = turn_greyscale_ids_.begin(); it != turn_greyscale_ids_.end();)
    {
        if (still_present.find(*it) == still_present.end())
        {
            it = turn_greyscale_ids_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    control().sync_units(live_entities);
}

void AnimationRenderer::tick_60hz(bool advance_combat)
{
    const bool had_hit_effect = control().hit_effect().has_value();
    control().tick_60hz(advance_combat);

    // battle() has already committed the logical HP result. Release its
    // visual HP target only on FE8's impact frame, not when the round begins.
    if (advance_combat && !had_hit_effect && control().hit_effect().has_value())
    {
        const int target_id = control().hit_effect()->pose.entity_id;
        const auto staged = staged_impact_hp_.find(target_id);
        const auto health = health_.find(target_id);
        if (staged != staged_impact_hp_.end() && health != health_.end())
        {
            health->second.current_hp = std::clamp(
                staged->second, 0, health->second.maximum_hp
            );
            staged_impact_hp_.erase(staged);
        }
    }

    const bool advance_health = advance_combat || !control().is_presenting_attack();
    if (!advance_health)
    {
        return;
    }
    for (auto& [entity_id, health] : health_)
    {
        (void)entity_id;
        const double target = static_cast<double>(health.current_hp);
        if (health.displayed_hp > target)
        {
            health.displayed_hp = std::max(target, health.displayed_hp - 1.0);
        }
        else if (health.displayed_hp < target)
        {
            health.displayed_hp = std::min(target, health.displayed_hp + 1.0);
        }
    }
}

bool AnimationRenderer::is_busy() const
{
    return control().is_busy();
}

bool AnimationRenderer::is_moving() const
{
    return control().is_moving();
}

bool AnimationRenderer::is_presenting_combat() const
{
    return control().is_presenting_attack();
}

std::optional<CompletedMove> AnimationRenderer::take_completed_move()
{
    return control().take_completed_move();
}

const RenderGrid& AnimationRenderer::blue_tiles() const
{
    return control().movement_overlay();
}

const RenderGrid& AnimationRenderer::red_tiles() const
{
    return control().attack_overlay();
}

void AnimationRenderer::clear_paint()
{
    for (std::vector<int>& row : blue_)
    {
        std::fill(row.begin(), row.end(), -1);
    }
    for (std::vector<int>& row : red_)
    {
        std::fill(row.begin(), row.end(), 0);
    }
    apply_paint();
}

const OccupancyGrid& AnimationRenderer::occupancy() const
{
    return control().occupancy();
}

std::vector<UnitPose> AnimationRenderer::unit_poses() const
{
    return control().visible_unit_poses();
}

std::vector<HealthBar> AnimationRenderer::health_bars() const
{
    std::vector<HealthBar> result;
    std::unordered_set<int> dying_ids;
    for (const DeathEffect& effect : death_effects())
    {
        dying_ids.insert(effect.pose.entity_id);
    }

    for (const UnitPose& pose : unit_poses())
    {
        // A lethal strike starts DeathEffect on its impact frame. Health bars
        // are UI, not part of the death sprite, so suppress them immediately.
        if (dying_ids.find(pose.entity_id) != dying_ids.end())
        {
            continue;
        }
        const auto found = health_.find(pose.entity_id);
        if (found == health_.end())
        {
            continue;
        }
        const HealthState& health = found->second;
        result.push_back(HealthBar{
            pose.entity_id,
            pose.visual,
            pose.color,
            pose.sheet_cell,
            pose.x,
            pose.y,
            health.displayed_hp,
            health.current_hp,
            health.maximum_hp
        });
    }
    return result;
}

const std::optional<MissEffect>& AnimationRenderer::miss_effect() const
{
    return control().miss_effect();
}

const std::optional<HitEffect>& AnimationRenderer::hit_effect() const
{
    return control().hit_effect();
}

const std::vector<DeathEffect>& AnimationRenderer::death_effects() const
{
    return control().death_effects();
}

bool AnimationRenderer::turn_greyscale_enabled(int entity_id) const
{
    return turn_greyscale_ids_.find(entity_id) != turn_greyscale_ids_.end();
}

LogicRenderControl& AnimationRenderer::control()
{
    if (control_ == nullptr)
    {
        throw std::logic_error("Call load_map(map) before using animations.");
    }
    return *control_;
}

const LogicRenderControl& AnimationRenderer::control() const
{
    if (control_ == nullptr)
    {
        throw std::logic_error("Call load_map(map) before using animations.");
    }
    return *control_;
}

void AnimationRenderer::stage_health_at_impact(const Entity& target, int hp_after)
{
    const auto health = health_.find(target.entity_id);
    if (health == health_.end())
    {
        throw std::logic_error(
            "Call sync_units() before resolving a battle so the renderer can preserve pre-hit HP."
        );
    }
    staged_impact_hp_[target.entity_id] = std::clamp(
        hp_after, 0, health->second.maximum_hp
    );
}

void AnimationRenderer::set_turn_greyscale(int entity_id, bool enabled)
{
    if (enabled)
    {
        turn_greyscale_ids_.insert(entity_id);
    }
    else
    {
        turn_greyscale_ids_.erase(entity_id);
    }
}

void AnimationRenderer::require_dimensions(const RenderGrid& grid, const char* name) const
{
    if (grid.size() != terrain_.size())
    {
        throw std::invalid_argument(std::string(name) + " height does not match loaded map.");
    }
    for (std::size_t y = 0; y < grid.size(); ++y)
    {
        if (grid[y].size() != terrain_[y].size())
        {
            throw std::invalid_argument(std::string(name) + " width does not match loaded map.");
        }
    }
}

void AnimationRenderer::apply_paint()
{
    control().show_action_state(blue_, red_);
}
}
