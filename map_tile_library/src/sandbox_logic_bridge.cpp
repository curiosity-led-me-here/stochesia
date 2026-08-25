#include "sandbox_logic_bridge.h"

#include <algorithm>
#include <array>
#include <stdexcept>

#include "general_pathtracing.h"
#include "terrain_data.h"
using namespace std;

namespace
{
using fe_tiles::Cell;
using fe_tiles::IntGrid;

constexpr array<Cell, 4> kDirections = {
    Cell{0, 1}, Cell{0, -1}, Cell{1, 0}, Cell{-1, 0},
};

int terrain_for_visual_class(int tile_class)
{
    switch (tile_class)
    {
        case fe_tiles::FOREST: return TERRAIN_FOREST;
        case fe_tiles::MOUNTAIN: return TERRAIN_MOUNTAIN;
        case fe_tiles::PEAK: return TERRAIN_PEAK;
        case fe_tiles::VALLEY: return TERRAIN_VALLEY;
        case fe_tiles::CLIFF:
        case fe_tiles::LAKE_CLIFF: return TERRAIN_CLIFF;
        case fe_tiles::ROAD:
        case fe_tiles::PLAIN_ROAD: return TERRAIN_ROAD;
        case fe_tiles::DESERT_GROUND: return TERRAIN_DESERT;
        case fe_tiles::SAND: return TERRAIN_SAND;
        case fe_tiles::RIVER: return TERRAIN_RIVER;
        case fe_tiles::WATER: return TERRAIN_WATER;
        case fe_tiles::LAKE: return TERRAIN_LAKE;
        case fe_tiles::SEA: return TERRAIN_SEA;
        case fe_tiles::BRIDGE: return TERRAIN_BRIDGE_REGULAR;
        case fe_tiles::FORT: return TERRAIN_FORT;
        case fe_tiles::VILLAGE: return TERRAIN_VILLAGE_REGULAR;
        case fe_tiles::HOUSE:
        case fe_tiles::VILLAGE_HOUSE: return TERRAIN_HOUSE;
        case fe_tiles::ARMORY: return TERRAIN_ARMORY;
        case fe_tiles::VENDOR: return TERRAIN_VENDOR;
        case fe_tiles::ARENA: return TERRAIN_ARENA_REGULAR;
        case fe_tiles::INN: return TERRAIN_INN;
        case fe_tiles::WALL:
        case fe_tiles::WALL2: return TERRAIN_WALL_REGULAR;
        case fe_tiles::FLOOR: return TERRAIN_FLOOR_REGULAR;
        case fe_tiles::STAIRS: return TERRAIN_STAIRS;
        case fe_tiles::DOOR: return TERRAIN_DOOR;
        case fe_tiles::GATE: return TERRAIN_GATE_REGULAR;
        case fe_tiles::FENCE:
        case fe_tiles::FENCE_WALL:
        case fe_tiles::FENCE_BRACE: return TERRAIN_FENCE_REGULAR;
        case fe_tiles::WALL_BRACE:
        case fe_tiles::BRACE_WALL: return TERRAIN_BRACE;
        case fe_tiles::PILLAR: return TERRAIN_PILLAR;
        case fe_tiles::THRONE: return TERRAIN_THRONE;
        case fe_tiles::CHEST: return TERRAIN_CHEST_FULL;
        case fe_tiles::ROOF: return TERRAIN_ROOF;
        case fe_tiles::RUINS: return TERRAIN_RUINS_REGULAR;
        case fe_tiles::THICKET: return TERRAIN_THICKET;
        case fe_tiles::SNAG: return TERRAIN_SNAG;
        case fe_tiles::BARREL: return TERRAIN_BARREL;
        case fe_tiles::BONE: return TERRAIN_BONE;
        case fe_tiles::DARK: return TERRAIN_DARK;
        case fe_tiles::DECK: return TERRAIN_DECK;
        case fe_tiles::GUNNELS: return TERRAIN_GUNNELS;
        case fe_tiles::MAST: return TERRAIN_MAST;
        case fe_tiles::BRACE: return TERRAIN_BRACE;
        case fe_tiles::EMPTY:
        case fe_tiles::PLAIN:
        case fe_tiles::FLAT:
        case fe_tiles::DASHDASH:
        case fe_tiles::UNDEFINED:
        default: return TERRAIN_PLAINS;
    }
}

bool same_cell(const vector<int>& position, Cell cell)
{
    return position.size() >= 2 && position[0] == cell.x && position[1] == cell.y;
}
}

namespace fe_tiles
{
SandboxLogicBridge::SandboxLogicBridge(IntGrid terrain)
    : terrain_(move(terrain))
{
    require_rectangular(terrain_);
    movement_.assign(terrain_.size(), vector<int>(terrain_.front().size(), -1));
    attack_.assign(terrain_.size(), vector<int>(terrain_.front().size(), 0));
    standing_attack_.assign(terrain_.size(), vector<int>(terrain_.front().size(), 0));
    rebuild_mapmaker();
}

void SandboxLogicBridge::require_rectangular(const IntGrid& terrain) const
{
    if (terrain.empty() || terrain.front().empty())
    {
        throw invalid_argument("The sandbox terrain grid cannot be empty.");
    }
    const size_t width = terrain.front().size();
    if (any_of(terrain.begin(), terrain.end(), [width](const vector<int>& row)
        {
            return row.size() != width;
        }))
    {
        throw invalid_argument("The sandbox terrain grid must be rectangular.");
    }
}

IntGrid SandboxLogicBridge::gameplay_terrain_from_visual_classes(const IntGrid& classes)
{
    if (classes.empty() || classes.front().empty())
    {
        throw invalid_argument("Visual class grid cannot be empty.");
    }
    IntGrid terrain = classes;
    for (vector<int>& row : terrain)
    {
        for (int& tile_class : row)
        {
            tile_class = terrain_for_visual_class(tile_class);
        }
    }
    return terrain;
}

void SandboxLogicBridge::set_terrain(IntGrid terrain)
{
    require_rectangular(terrain);
    terrain_ = move(terrain);
    clear_preview();
    rebuild_mapmaker();
}

const IntGrid& SandboxLogicBridge::terrain() const
{
    return terrain_;
}

void SandboxLogicBridge::create_chapter_one_roster(const array<Cell, 4>& locations)
{
    if (units_[0] != nullptr)
    {
        throw logic_error("The workbench roster was already created.");
    }

    guilds_[0].name = "Renais";
    guilds_[0].guild_id = 1;
    guilds_[1].name = "Bandits";
    guilds_[1].guild_id = 2;

    units_[0] = &registry_.spawn(entities::eirika(), 1);
    units_[1] = &registry_.spawn(entities::seth(), 2);
    units_[2] = &registry_.spawn(entities::soldier(), 3);
    units_[3] = &registry_.spawn(entities::soldier(), 4);

    units_[2]->name = "Breguet";
    units_[2]->Lvl = 4;

    units_[0]->inventory.slot[0] = {IRON_SWORD, 46};
    units_[0]->inventory.EquippedSlot = 0;
    units_[1]->inventory.slot[0] = {IRON_LANCE, 45};
    units_[1]->inventory.slot[1] = {IRON_SWORD, 46};
    units_[1]->inventory.EquippedSlot = 0;
    units_[2]->inventory.slot[0] = {IRON_LANCE, 45};
    units_[2]->inventory.EquippedSlot = 0;
    units_[3]->inventory.slot[0] = {IRON_LANCE, 45};
    units_[3]->inventory.EquippedSlot = 0;

    guilds_[0].add(*units_[0]);
    guilds_[0].add(*units_[1]);
    guilds_[1].add(*units_[2]);
    guilds_[1].add(*units_[3]);

    for (size_t index = 0; index < units_.size(); ++index)
    {
        if (locations[index].x < 0 || locations[index].y < 0 ||
            locations[index].y >= static_cast<int>(terrain_.size()) ||
            locations[index].x >= static_cast<int>(terrain_.front().size()))
        {
            throw out_of_range("Workbench roster location is outside the terrain grid.");
        }
        units_[index]->location = {locations[index].x, locations[index].y};
    }
    rebuild_mapmaker();
}

Entity* SandboxLogicBridge::find_unit(int entity_id) const
{
    const auto it = find_if(units_.begin(), units_.end(), [entity_id](const Entity* unit)
        {
            return unit != nullptr && unit->entity_id == entity_id;
        });
    return it == units_.end() ? nullptr : *it;
}

Entity* SandboxLogicBridge::find_unit_at(Cell location) const
{
    const auto it = find_if(units_.begin(), units_.end(), [location](const Entity* unit)
        {
            return unit != nullptr && unit->alive && same_cell(unit->location, location);
        });
    return it == units_.end() ? nullptr : *it;
}

Entity& SandboxLogicBridge::unit(int entity_id)
{
    Entity* found = find_unit(entity_id);
    if (found == nullptr)
    {
        throw invalid_argument("Unknown workbench entity ID.");
    }
    return *found;
}

const Entity& SandboxLogicBridge::unit(int entity_id) const
{
    const Entity* found = find_unit(entity_id);
    if (found == nullptr)
    {
        throw invalid_argument("Unknown workbench entity ID.");
    }
    return *found;
}

vector<Entity*> SandboxLogicBridge::units() const
{
    return {units_.begin(), units_.end()};
}

void SandboxLogicBridge::clear_preview()
{
    selected_entity_id_ = 0;
    available_attacks_.clear();
    const int height = static_cast<int>(terrain_.size());
    const int width = height > 0 ? static_cast<int>(terrain_.front().size()) : 0;
    movement_.assign(height, vector<int>(width, -1));
    attack_.assign(height, vector<int>(width, 0));
    standing_attack_.assign(height, vector<int>(width, 0));
}

void SandboxLogicBridge::rebuild_mapmaker()
{
    require_rectangular(terrain_);
    board_ = make_unique<Mapmaker>(terrain_);
    for (Entity* current : units_)
    {
        if (current != nullptr && current->alive)
        {
            board_->place_unit(*current);
        }
    }
}

void SandboxLogicBridge::inspect(int entity_id)
{
    Entity& current = unit(entity_id);
    if (!current.alive)
    {
        throw invalid_argument("That entity is dead.");
    }
    if (!current.turn)
    {
        throw invalid_argument("That entity has already acted this phase.");
    }
    rebuild_mapmaker();
    board_->path_trace(current);
    board_->attack_range(current);
    movement_ = board_->consider_occupancy(current);
    movement_[current.location[1]][current.location[0]] =
        current.path[current.location[1]][current.location[0]];
    attack_ = current.attack_range;
    standing_attack_.assign(terrain_.size(), vector<int>(terrain_.front().size(), 0));
    available_attacks_.clear();
    selected_entity_id_ = entity_id;
}

int SandboxLogicBridge::selected_entity_id() const
{
    return selected_entity_id_;
}

const IntGrid& SandboxLogicBridge::movement() const
{
    return movement_;
}

const IntGrid& SandboxLogicBridge::attack() const
{
    return attack_;
}

vector<Cell> SandboxLogicBridge::route_to(int entity_id, Cell destination) const
{
    if (entity_id != selected_entity_id_)
    {
        throw invalid_argument("Run inspect/check for this entity first.");
    }
    const Entity& current = unit(entity_id);
    if (destination.x < 0 || destination.y < 0 ||
        destination.y >= static_cast<int>(movement_.size()) ||
        destination.x >= static_cast<int>(movement_.front().size()) ||
        movement_[destination.y][destination.x] < 0)
    {
        throw invalid_argument("Destination is not landable according to Mapmaker.");
    }
    if (same_cell(current.location, destination))
    {
        throw invalid_argument("Choose a destination different from the current tile.");
    }

    vector<Cell> reverse_route = {destination};
    Cell cursor = destination;
    const Cell origin = {current.location[0], current.location[1]};
    const int max_steps = static_cast<int>(terrain_.size() * terrain_.front().size());
    for (int steps = 0; !(cursor.x == origin.x && cursor.y == origin.y); ++steps)
    {
        if (steps >= max_steps)
        {
            throw logic_error("Mapmaker movement state did not lead back to the unit origin.");
        }
        const int cost = terrain::movement_cost(
            terrain_[cursor.y][cursor.x], current.movement
        );
        bool found_parent = false;
        for (const Cell delta : kDirections)
        {
            const Cell parent = {cursor.x + delta.x, cursor.y + delta.y};
            if (parent.x < 0 || parent.y < 0 ||
                parent.y >= static_cast<int>(current.path.size()) ||
                parent.x >= static_cast<int>(current.path[parent.y].size()))
            {
                continue;
            }
            if (current.path[parent.y][parent.x] == current.path[cursor.y][cursor.x] + cost)
            {
                reverse_route.push_back(parent);
                cursor = parent;
                found_parent = true;
                break;
            }
        }
        if (!found_parent)
        {
            throw logic_error("Could not reconstruct a Mapmaker movement route.");
        }
    }
    reverse(reverse_route.begin(), reverse_route.end());
    return reverse_route;
}

IntGrid SandboxLogicBridge::make_weapon_attack_grid(const Entity& current, const Weapon& weapon) const
{
    IntGrid result(terrain_.size(), vector<int>(terrain_.front().size(), 0));
    IntGrid out_min(terrain_.size(), vector<int>(terrain_.front().size(), -1));
    IntGrid out_max = out_min;

    // trace() normalizes both its grids as inclusive radii, so the inner grid
    // must end at MINRG - 1. Passing MINRG itself is the source sandbox bug
    // that removes a 1-range weapon's entire ring. This is bridge-only until
    // you decide to make the same one-line correction in general_pathtracing.
    const int inner_radius = max(0, weapon.MINRG - 1);
    trace(out_min, out_max, current.location, inner_radius, weapon.MAXRG);
    return out_max;
}

IntGrid SandboxLogicBridge::make_standing_attack_grid(const Entity& current) const
{
    IntGrid result(terrain_.size(), vector<int>(terrain_.front().size(), 0));
    for (const ItemStack& item : current.inventory.slot)
    {
        try
        {
            const Weapon weapon = get_weapon(Armory, item.ID);
            const IntGrid out_max = make_weapon_attack_grid(current, weapon);
            for (size_t y = 0; y < result.size(); ++y)
            {
                for (size_t x = 0; x < result[y].size(); ++x)
                {
                    result[y][x] = max(result[y][x], out_max[y][x]);
                }
            }
        }
        catch (const invalid_argument&)
        {
            // NO_ITEM and healing-only entries are correctly not attack tools.
        }
    }
    return result;
}

void SandboxLogicBridge::preview_arrival(int entity_id, Cell destination)
{
    Entity& current = unit(entity_id);
    if (entity_id != selected_entity_id_)
    {
        throw invalid_argument("Run inspect/check for this entity first.");
    }

    const vector<int> original_location = current.location;
    current.location = {destination.x, destination.y};
    try
    {
        standing_attack_ = make_standing_attack_grid(current);
        available_attacks_.clear();
        for (int slot = 0; slot < 5; ++slot)
        {
            try
            {
                const Weapon weapon = get_weapon(Armory, current.inventory.slot[slot].ID);
                const IntGrid weapon_range = make_weapon_attack_grid(current, weapon);
                for (Entity* candidate : units_)
                {
                    if (candidate == nullptr || !candidate->alive || candidate->group == nullptr ||
                        current.group == nullptr || candidate->group->guild_id == current.group->guild_id ||
                        candidate->location.size() < 2)
                    {
                        continue;
                    }
                    const int x = candidate->location[0];
                    const int y = candidate->location[1];
                    if (y >= 0 && y < static_cast<int>(weapon_range.size()) &&
                        x >= 0 && x < static_cast<int>(weapon_range[y].size()) &&
                        weapon_range[y][x] > 0)
                    {
                        available_attacks_.push_back({{x, y}, weapon, slot});
                    }
                }
            }
            catch (const invalid_argument&)
            {
                // Empty inventory slot: no weapon, no target prompt.
            }
        }
    }
    catch (...)
    {
        current.location = original_location;
        throw;
    }
    current.location = original_location;
}

const IntGrid& SandboxLogicBridge::standing_attack() const
{
    return standing_attack_;
}

const vector<avl_for_atk>& SandboxLogicBridge::available_attacks() const
{
    return available_attacks_;
}

void SandboxLogicBridge::commit_move(int entity_id, Cell destination)
{
    Entity& current = unit(entity_id);
    current.location = {destination.x, destination.y};
    rebuild_mapmaker();
    board_->path_trace(current);
}

void SandboxLogicBridge::wait(int entity_id)
{
    Entity& current = unit(entity_id);
    current.turn = false;
    clear_preview();
}

BattleResolution SandboxLogicBridge::attack(int attacker_id, Cell defender_location, int inventory_slot)
{
    if (attacker_id != selected_entity_id_)
    {
        throw invalid_argument("The active attacker does not match the checked entity.");
    }
    Entity& attacker = unit(attacker_id);
    Entity* defender = find_unit_at(defender_location);
    if (defender == nullptr)
    {
        throw invalid_argument("No living entity occupies that attack coordinate.");
    }
    const auto exact_choice = find_if(available_attacks_.begin(), available_attacks_.end(),
        [defender_location, inventory_slot](const avl_for_atk& choice)
        {
            return choice.coords.size() == 2 &&
                   choice.coords[0] == defender_location.x &&
                   choice.coords[1] == defender_location.y &&
                   choice.inventory_id == inventory_slot;
        });
    if (exact_choice == available_attacks_.end())
    {
        throw invalid_argument("That target/weapon slot is not in Mapmaker::prompt_attack().");
    }

    attacker.inventory.EquippedSlot = inventory_slot;
    const int attacker_hp_before = attacker.stats.HP;
    const int defender_hp_before = defender->stats.HP;
    const vector<CombatInfo> forecast = interact(attacker, *defender);
    battle(attacker, *defender, *board_);
    attacker.turn = false;

    BattleResolution result;
    result.attacker_id = attacker.entity_id;
    result.defender_id = defender->entity_id;
    result.attacker_name = attacker.name;
    result.defender_name = defender->name;
    result.attacker_hp_before = attacker_hp_before;
    result.attacker_hp_after = attacker.stats.HP;
    result.attacker_hp_max = attacker.ogstats.HP;
    result.defender_hp_before = defender_hp_before;
    result.defender_hp_after = defender->stats.HP;
    result.defender_hp_max = defender->ogstats.HP;
    result.attacker_defeated = !attacker.alive;
    result.defender_defeated = !defender->alive;
    (void)forecast;
    clear_preview();
    return result;
}

void SandboxLogicBridge::ready_guild(int guild_id)
{
    for (Entity* current : units_)
    {
        if (current != nullptr && current->alive && current->group != nullptr &&
            current->group->guild_id == guild_id)
        {
            current->turn = true;
        }
    }
}
}
