#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "map_tile_library.h"
#include "occupancy_overlay.h"
#include "sandbox_logic_bridge.h"
using namespace std;

namespace
{
// Defaults; main() optionally replaces them from: fe8_map_workbench <rows> <columns>
int kRows = 12;
int kColumns = 18;
constexpr int kFirstTheme = 1;
constexpr int kLastTheme = 72;
constexpr CGFloat kSidebarWidth = 310.0;
constexpr CGFloat kMargin = 22.0;
// FE8's source canvas is two terrain tiles wide. In this generator workbench
// we render it at 85% so large classes such as Paladin read as occupying their
// own logical tile rather than visually spilling across neighbours.
constexpr CGFloat kUnitCanvasInTiles = 1.9;

struct PaletteColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

using Palette = array<PaletteColor, 16>;

const char* class_name(int tile_class)
{
    switch (tile_class)
    {
        case fe_tiles::PLAIN: return "PLAIN";
        case fe_tiles::FOREST: return "FOREST";
        case fe_tiles::MOUNTAIN: return "MOUNTAIN";
        case fe_tiles::PEAK: return "PEAK";
        case fe_tiles::ROAD: return "ROAD";
        case fe_tiles::RIVER: return "RIVER";
        case fe_tiles::WATER: return "WATER";
        case fe_tiles::LAKE: return "LAKE";
        case fe_tiles::BRIDGE: return "BRIDGE";
        case fe_tiles::FORT: return "FORT";
        case fe_tiles::VILLAGE: return "VILLAGE";
        case fe_tiles::HOUSE: return "HOUSE";
        case fe_tiles::WALL: return "WALL";
        default: return "OTHER";
    }
}

// An in-memory map recipe plus the literal TileCanvas built from it.
// No PNG is written anywhere by this program.
struct Workshop
{
    string root;
    int theme = fe_tiles::THEME_CHAPTERS_01;
    int selected_class = fe_tiles::PLAIN;
    int selected_subclass = 0;
    int selected_orientation = 0;
    // The catalogue lives on disk. These are refreshed only when the user
    // changes a selection, never from drawRect()/the 60 Hz timer.
    int selected_subclass_count = 0;
    int selected_orientation_count = 0;
    fe_tiles::IntGrid classes;
    fe_tiles::IntGrid visuals;
    // This is the second, gameplay-shaped layer: 0 = empty; all other values
    // are entity IDs. It is intentionally independent of terrain visuals.
    fe_tiles::OccupancyGrid occupancy;
    fe_tiles::SpriteLayer sprites;
    fe_tiles::CombatPresentation combat_presentation;
    unique_ptr<fe_tiles::SandboxLogicBridge> logic;
    fe_tiles::IntGrid preview_cost;
    fe_tiles::IntGrid preview_attack;
    vector<vector<fe_tiles::Cell>> preview_parent;
    optional<int> checked_entity;
    struct PendingMove
    {
        int entity_id = 0;
        fe_tiles::Cell destination;
    };
    optional<PendingMove> pending_move;
    bool awaiting_attack_choice = false;
    bool sync_after_attack = false;
    unique_ptr<fe_tiles::TileCanvas> canvas;
    string status;
    bool invariant_movement_enabled = false;
    size_t next_invariant_unit = 0;
    array<size_t, 4> invariant_steps = {0, 0, 0, 0};

    explicit Workshop(string library_root)
        : root(move(library_root)),
          classes(kRows, vector<int>(kColumns, fe_tiles::PLAIN)),
          visuals(kRows, vector<int>(kColumns, fe_tiles::make_tile_code(0, 0))),
          occupancy(kRows, vector<int>(kColumns, 0)),
          sprites(occupancy),
          preview_cost(kRows, vector<int>(kColumns, -1)),
          preview_attack(kRows, vector<int>(kColumns, 0)),
          preview_parent(kRows, vector<fe_tiles::Cell>(kColumns))
    {
        build_starter_map();
        logic = make_unique<fe_tiles::SandboxLogicBridge>(
            fe_tiles::SandboxLogicBridge::gameplay_terrain_from_visual_classes(classes)
        );
        logic->create_chapter_one_roster({{
            fe_tiles::Cell{3, 6},
            fe_tiles::Cell{15, 7},
            fe_tiles::Cell{5, 2},
            fe_tiles::Cell{14, 3},
        }});
        add_sample_units();
        sync_occupancy_from_logic();
        redraw();
    }

    int subclass_total(int tile_class) const
    {
        return fe_tiles::subclass_count(theme, tile_class, root);
    }

    bool in_bounds(int x, int y) const
    {
        return x >= 0 && x < kColumns && y >= 0 && y < kRows;
    }

    int movement_budget(int entity_id) const
    {
        switch (entity_id)
        {
            case 1: return 5; // Eirika
            case 2: return 8; // Paladin
            default: return 4; // Soldiers
        }
    }

    int visual_traversal_cost(int x, int y) const
    {
        switch (classes[y][x])
        {
            case fe_tiles::FOREST: return 2;
            case fe_tiles::MOUNTAIN:
            case fe_tiles::PEAK:
            case fe_tiles::VALLEY:
            case fe_tiles::CLIFF:
            case fe_tiles::RIVER:
            case fe_tiles::WATER:
            case fe_tiles::LAKE:
            case fe_tiles::SEA:
                return -1;
            default:
                return 1;
        }
    }

    void clear_preview()
    {
        for (vector<int>& row : preview_cost)
        {
            fill(row.begin(), row.end(), -1);
        }
        for (vector<int>& row : preview_attack)
        {
            fill(row.begin(), row.end(), 0);
        }
        checked_entity.reset();
        awaiting_attack_choice = false;
    }

    void sync_occupancy_from_logic()
    {
        for (vector<int>& row : occupancy)
        {
            fill(row.begin(), row.end(), 0);
        }
        if (!logic)
        {
            return;
        }
        for (Entity* unit : logic->units())
        {
            if (unit == nullptr || !unit->alive || unit->location.size() < 2)
            {
                continue;
            }
            const int x = unit->location[0];
            const int y = unit->location[1];
            if (in_bounds(x, y))
            {
                occupancy[y][x] = unit->entity_id;
            }
        }
    }

    void refresh_logic_terrain()
    {
        if (!logic)
        {
            return;
        }
        logic->set_terrain(
            fe_tiles::SandboxLogicBridge::gameplay_terrain_from_visual_classes(classes)
        );
        clear_preview();
        sync_occupancy_from_logic();
    }

    optional<fe_tiles::UnitPose> pose_for(int entity_id) const
    {
        const vector<fe_tiles::UnitPose> poses = sprites.poses();
        const auto it = find_if(poses.begin(), poses.end(),
            [entity_id](const fe_tiles::UnitPose& pose)
            {
                return pose.entity_id == entity_id;
            });
        if (it == poses.end())
        {
            return nullopt;
        }
        return *it;
    }

    string check_entity(int entity_id)
    {
        if (sprites.is_animating() || combat_presentation.is_presenting() || pending_move.has_value())
        {
            return "check: wait for the current movement or attack presentation to finish.";
        }
        try
        {
            logic->inspect(entity_id);
            invariant_movement_enabled = false;
            checked_entity = entity_id;
            preview_cost = logic->movement();
            preview_attack = logic->attack();
            status = "Mapmaker path_trace + attack_range: blue is landable movement; red is attack-only.";
            return "check: entity " + to_string(entity_id) + " selected by sandbox logic.";
        }
        catch (const exception& error)
        {
            return string("check: ") + error.what();
        }
    }

    string move_checked_by_offset(int delta_x, int delta_y)
    {
        if (sprites.is_animating() || combat_presentation.is_presenting() || pending_move.has_value())
        {
            return "move: wait for the current movement or attack presentation to finish.";
        }
        if (awaiting_attack_choice)
        {
            return "move: choose 'attack <x> <y> <slot>' or 'wait' first.";
        }
        if (!checked_entity.has_value())
        {
            return "move: run 'check <unit id>' first.";
        }
        try
        {
            const Entity& unit = logic->unit(*checked_entity);
            // Terminal commands remain Cartesian: +x right, +y up.
            const fe_tiles::Cell destination = {
                unit.location[0] + delta_x,
                unit.location[1] - delta_y,
            };
            const vector<fe_tiles::Cell> route = logic->route_to(*checked_entity, destination);
            if (!sprites.begin_move(*checked_entity, route))
            {
                return "move: renderer rejected the Mapmaker route.";
            }
            pending_move = PendingMove{*checked_entity, destination};
            status = "Animating the route reconstructed from Mapmaker::path_trace().";
            return "move: following the sandbox route to {x=" +
                   to_string(destination.x) + ", y=" + to_string(destination.y) + "}.";
        }
        catch (const exception& error)
        {
            return string("move: ") + error.what();
        }
    }

    string attack_selected_target(int x, int y, int inventory_slot)
    {
        if (!awaiting_attack_choice || !checked_entity.has_value())
        {
            return "attack: move onto a tile with an available target first.";
        }
        if (sprites.is_animating() || combat_presentation.is_presenting())
        {
            return "attack: wait for the current presentation to finish.";
        }
        if (!in_bounds(x, y))
        {
            return "attack: target coordinate is outside the map.";
        }
        try
        {
            const int attacker_id = *checked_entity;
            const auto attacker = pose_for(attacker_id);
            const auto defender = pose_for(occupancy[y][x]);
            if (!attacker.has_value() || !defender.has_value())
            {
                return "attack: that coordinate does not contain a rendered target.";
            }

            const fe_tiles::BattleResolution resolved = logic->attack(
                attacker_id, {x, y}, inventory_slot
            );
            fe_tiles::BattleWindow presentation;
            presentation.attacker_id = resolved.attacker_id;
            presentation.defender_id = resolved.defender_id;
            presentation.attacker_name = resolved.attacker_name;
            presentation.defender_name = resolved.defender_name;
            presentation.attacker_hp_before = resolved.attacker_hp_before;
            presentation.attacker_hp_after = resolved.attacker_hp_after;
            presentation.attacker_hp_max = resolved.attacker_hp_max;
            presentation.defender_hp_before = resolved.defender_hp_before;
            presentation.defender_hp_after = resolved.defender_hp_after;
            presentation.defender_hp_max = resolved.defender_hp_max;
            presentation.attacker_defeated = resolved.attacker_defeated;
            presentation.defender_defeated = resolved.defender_defeated;
            combat_presentation.begin(presentation, attacker, defender);

            sync_after_attack = true;
            clear_preview();
            status = "battle() resolved the attack; rendering FE8's map-unit lunge.";
            return "attack: sandbox battle resolved; displaying the map attack animation.";
        }
        catch (const exception& error)
        {
            return string("attack: ") + error.what();
        }
    }

    string wait_after_move()
    {
        if (!awaiting_attack_choice || !checked_entity.has_value())
        {
            return "wait: no moved unit is awaiting an attack choice.";
        }
        logic->wait(*checked_entity);
        clear_preview();
        status = "Unit waited; its sandbox turn is complete.";
        return "wait: unit turn complete.";
    }

    string ready_phase(int guild_id)
    {
        logic->ready_guild(guild_id);
        status = "Marked living guild " + to_string(guild_id) + " units READY.";
        return "phase: guild " + to_string(guild_id) + " is ready.";
    }

    string execute_terminal_command(const string& line)
    {
        istringstream input(line);
        string command;
        input >> command;
        if (command == "check")
        {
            int entity_id = 0;
            if (input >> entity_id)
            {
                return check_entity(entity_id);
            }
            return "Usage: check <int unit id>";
        }
        if (command == "move")
        {
            int delta_x = 0;
            int delta_y = 0;
            if (input >> delta_x >> delta_y)
            {
                return move_checked_by_offset(delta_x, delta_y);
            }
            return "Usage: move <int x offset> <int y offset>";
        }
        if (command == "attack")
        {
            int x = 0;
            int y = 0;
            int inventory_slot = 0;
            if (input >> x >> y >> inventory_slot)
            {
                return attack_selected_target(x, y, inventory_slot);
            }
            return "Usage: attack <target x> <target y> <inventory slot>";
        }
        if (command == "wait")
        {
            return wait_after_move();
        }
        if (command == "phase")
        {
            int guild_id = 0;
            if (input >> guild_id)
            {
                return ready_phase(guild_id);
            }
            return "Usage: phase <guild id>";
        }
        if (command == "help")
        {
            return "Commands: check <id> | move <dx> <dy> | attack <x> <y> <slot> | wait | phase <guild>";
        }
        return "Unknown command. Type 'help'.";
    }

    bool choose_class(int tile_class)
    {
        if (subclass_total(tile_class) <= 0)
        {
            status = string("That class does not exist in theme ") +
                     to_string(theme) + ".";
            return false;
        }
        selected_class = tile_class;
        selected_subclass = 0;
        selected_orientation = 0;
        normalize_selection();
        status = "Selected " + string(class_name(selected_class)) + ".";
        return true;
    }

    bool choose_first_available_class()
    {
        for (int candidate = fe_tiles::PLAIN; candidate <= fe_tiles::VILLAGE_HOUSE;
             ++candidate)
        {
            if (subclass_total(candidate) > 0)
            {
                selected_class = candidate;
                selected_subclass = 0;
                selected_orientation = 0;
                return true;
            }
        }
        return false;
    }

    void normalize_selection()
    {
        selected_subclass_count = subclass_total(selected_class);
        if (selected_subclass_count <= 0)
        {
            if (!choose_first_available_class())
            {
                throw runtime_error("This theme contains no drawable tile classes.");
            }
            selected_subclass_count = subclass_total(selected_class);
        }

        selected_subclass = clamp(selected_subclass, 0, selected_subclass_count - 1);
        selected_orientation_count = fe_tiles::orientation_count(
            theme, selected_class, selected_subclass, root
        );
        if (selected_orientation_count <= 0)
        {
            throw runtime_error("Selected class/subclass has no orientations.");
        }
        selected_orientation = clamp(selected_orientation, 0, selected_orientation_count - 1);
    }

    void redraw()
    {
        try
        {
            normalize_selection();
            auto next = make_unique<fe_tiles::TileCanvas>(kRows, kColumns);
            next->draw(theme, classes, visuals, root);
            canvas = move(next);
            if (status.empty())
            {
                status = "Ready. Click the board to paint a literal FE8 source tile.";
            }
        }
        catch (const exception& error)
        {
            status = string("Render error: ") + error.what();
        }
    }

    void fill_selected()
    {
        normalize_selection();
        const int code = fe_tiles::make_tile_code(selected_subclass, selected_orientation);
        for (vector<int>& row : classes)
        {
            fill(row.begin(), row.end(), selected_class);
        }
        for (vector<int>& row : visuals)
        {
            fill(row.begin(), row.end(), code);
        }
        refresh_logic_terrain();
        status = "Filled the canvas with the selected literal tile.";
        redraw();
    }

    void paint(int x, int y)
    {
        if (sprites.is_animating() || combat_presentation.is_presenting() || pending_move.has_value())
        {
            status = "Terrain editing is paused while a sandbox action is running.";
            return;
        }
        if (x < 0 || x >= kColumns || y < 0 || y >= kRows)
        {
            return;
        }
        normalize_selection();
        classes[y][x] = selected_class;
        visuals[y][x] = fe_tiles::make_tile_code(selected_subclass, selected_orientation);
        refresh_logic_terrain();
        ostringstream message;
        message << "Painted {x=" << x << ", y=" << y << "}: "
                << class_name(selected_class) << " / subclass "
                << selected_subclass << " / orientation " << selected_orientation << ".";
        status = message.str();
        redraw();
    }

    static void write_cpp_grid(ostream& output,
                               const char* name,
                               const fe_tiles::IntGrid& grid)
    {
        output << "maps::IntGrid " << name << " = {\n";
        for (const vector<int>& row : grid)
        {
            output << "    {";
            for (size_t x = 0; x < row.size(); ++x)
            {
                if (x != 0)
                {
                    output << ", ";
                }
                output << row[x];
            }
            output << "},\n";
        }
        output << "};\n\n";
    }

    // Export exactly the three aligned [y][x] layers needed by maps::compose.
    // This performs no image export and does not touch the sandbox's map;
    // it is simply a copyable handoff from visual editing to your game logic.
    bool export_recipe(const string& destination)
    {
        try
        {
            const fe_tiles::IntGrid terrain =
                fe_tiles::SandboxLogicBridge::gameplay_terrain_from_visual_classes(classes);
            ofstream output(destination);
            if (!output)
            {
                status = "Could not write recipe export: " + destination;
                return false;
            }

            output << "// Generated by FE8 Tile Workbench. Grids are [y][x].\n"
                   << "// Theme " << theme << "; copy this into a .cpp file after #include \\\"maps.h\\\".\n\n";
            write_cpp_grid(output, "terrain_grid", terrain);
            write_cpp_grid(output, "classes", classes);
            write_cpp_grid(output, "tiles", visuals);
            output << "maps::MapRecipe my_map = maps::compose(\n"
                   << "    " << theme << ", // theme_id\n"
                   << "    terrain_grid,\n"
                   << "    classes,\n"
                   << "    tiles\n"
                   << ");\n";
            status = "Exported terrain_grid, classes, and tiles to " + destination;
            return true;
        }
        catch (const exception& error)
        {
            status = string("Recipe export failed: ") + error.what();
            return false;
        }
    }

    void sample(int x, int y)
    {
        if (x < 0 || x >= kColumns || y < 0 || y >= kRows)
        {
            return;
        }
        selected_class = classes[y][x];
        selected_subclass = fe_tiles::subclass_from_code(visuals[y][x]);
        selected_orientation = fe_tiles::orientation_from_code(visuals[y][x]);
        normalize_selection();
        ostringstream message;
        message << "Sampled {x=" << x << ", y=" << y << "}.";
        status = message.str();
    }

    void cycle_subclass(int delta)
    {
        normalize_selection();
        if (selected_subclass_count <= 0)
        {
            return;
        }
        selected_subclass = (selected_subclass + delta + selected_subclass_count) % selected_subclass_count;
        selected_orientation = 0;
        normalize_selection();
        status = "Changed subclass.";
    }

    void cycle_orientation(int delta)
    {
        normalize_selection();
        if (selected_orientation_count <= 0)
        {
            return;
        }
        selected_orientation = (selected_orientation + delta + selected_orientation_count) % selected_orientation_count;
        status = "Changed orientation variant.";
    }

    void change_theme(int delta)
    {
        int candidate = theme;
        for (int tries = 0; tries < kLastTheme; ++tries)
        {
            candidate += delta;
            if (candidate < kFirstTheme)
            {
                candidate = kLastTheme;
            }
            if (candidate > kLastTheme)
            {
                candidate = kFirstTheme;
            }
            theme = candidate;
            if (choose_first_available_class())
            {
                fill_selected();
                status = "Changed to theme " + to_string(theme) +
                         " and reset the canvas to a compatible base tile.";
                return;
            }
        }
        status = "No drawable tile theme found.";
    }

    void build_starter_map()
    {
        // A small raw-tile sketch, deliberately not auto-oriented. The point
        // of this workbench is to let your generator choose every integer.
        for (int y = 1; y <= 4; ++y)
        {
            for (int x = 1; x <= 4; ++x)
            {
                classes[y][x] = fe_tiles::FOREST;
            }
        }
        for (int y = 8; y <= 10; ++y)
        {
            for (int x = 12; x <= 16; ++x)
            {
                classes[y][x] = fe_tiles::PEAK;
            }
        }
        for (int y = 0; y < kRows; ++y)
        {
            classes[y][9] = fe_tiles::LAKE;
        }
        classes[5][8] = fe_tiles::HOUSE;
    }

    void add_sample_units()
    {
        // IDs and locations come from SandboxLogicBridge's Entity/Registry
        // setup. This table maps those real IDs to FE8 map-sprite artwork.
        sprites.register_unit(1, fe_tiles::UnitVisual::Eirika);
        sprites.register_unit(2, fe_tiles::UnitVisual::Paladin);
        sprites.register_unit(3, fe_tiles::UnitVisual::Soldier);
        sprites.register_unit(4, fe_tiles::UnitVisual::Soldier);
    }

    bool play_demo_move()
    {
        if (sprites.is_animating())
        {
            status = "A unit is already moving.";
            return false;
        }
        const auto origin = sprites.location_of(1);
        if (!origin.has_value())
        {
            status = "Eirika (entity ID 1) is not in occupancy.";
            return false;
        }

        // This demo exercises the exact public path contract: an ordered
        // cardinal route. Replace this vector with your pathfinder's route
        // when we wire the real Mapmaker object in.
        const array<vector<fe_tiles::Cell>, 4> patterns = {{
            {{1, 0}, {1, 0}, {0, 1}, {0, 1}},
            {{-1, 0}, {-1, 0}, {0, -1}, {0, -1}},
            {{0, 1}, {0, 1}, {1, 0}, {1, 0}},
            {{0, -1}, {0, -1}, {-1, 0}, {-1, 0}},
        }};
        for (const vector<fe_tiles::Cell>& pattern : patterns)
        {
            vector<fe_tiles::Cell> route = {*origin};
            fe_tiles::Cell next = *origin;
            for (const fe_tiles::Cell& delta : pattern)
            {
                next = {next.x + delta.x, next.y + delta.y};
                route.push_back(next);
            }
            if (sprites.begin_move(1, route))
            {
                status = "Animating entity 1 from the occupancy route at 60 Hz.";
                return true;
            }
        }
        status = "No clear demo route for entity 1.";
        return false;
    }

    bool begin_next_invariant_move()
    {
        if (!invariant_movement_enabled || sprites.is_animating() || combat_presentation.is_presenting())
        {
            return false;
        }

        // Every unit has a small, disjoint square. We animate one cardinal
        // edge at a time so the test is deterministic and occupancy can stay
        // a plain [y][x] entity-ID layer. These are a renderer test harness,
        // not a replacement for your Mapmaker-approved movement routes.
        const array<int, 4> entity_ids = {1, 2, 3, 4};
        const array<array<fe_tiles::Cell, 4>, 4> loops = {{
            {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}},
            {{{-1, 0}, {0, 1}, {1, 0}, {0, -1}}},
            {{{1, 0}, {0, -1}, {-1, 0}, {0, 1}}},
            {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}},
        }};

        for (int attempt = 0; attempt < 4; ++attempt)
        {
            const size_t unit_index = next_invariant_unit;
            next_invariant_unit = (next_invariant_unit + 1) % entity_ids.size();
            const int entity_id = entity_ids[unit_index];
            const auto origin = sprites.location_of(entity_id);
            if (!origin.has_value())
            {
                continue;
            }
            const fe_tiles::Cell delta = loops[unit_index][invariant_steps[unit_index]];
            const fe_tiles::Cell destination = {origin->x + delta.x, origin->y + delta.y};
            if (sprites.begin_move(entity_id, {*origin, destination}))
            {
                invariant_steps[unit_index] = (invariant_steps[unit_index] + 1) % loops[unit_index].size();
                status = "Invariant movement test: entity " + to_string(entity_id) + " is moving.";
                return true;
            }
        }
        status = "Invariant movement paused: a loop destination is occupied.";
        invariant_movement_enabled = false;
        return false;
    }

    void toggle_invariant_movement()
    {
        invariant_movement_enabled = false;
        status = "The old demo loop is disabled. Use check <id>, then move <dx> <dy>.";
    }

    void tick_fe_frame()
    {
        sprites.tick_fe_frame();
        combat_presentation.tick_fe_frame();

        // battle() has already changed Entity::alive and Guild membership.
        // Keep the pre-battle sprites visible through the lunge, then replace
        // them with sandbox occupancy exactly when the death fade begins.
        if (sync_after_attack && !combat_presentation.attack_effect().has_value())
        {
            logic->rebuild_mapmaker();
            sync_occupancy_from_logic();
            sync_after_attack = false;
        }

        const auto completed = sprites.take_completed_move();
        if (!completed.has_value())
        {
            return;
        }
        if (!pending_move.has_value() || pending_move->entity_id != completed->entity_id)
        {
            status = "Renderer movement completed without a sandbox move request.";
            return;
        }

        try
        {
            const PendingMove moved = *pending_move;
            pending_move.reset();
            logic->commit_move(moved.entity_id, moved.destination);
            sync_occupancy_from_logic();
            logic->preview_arrival(moved.entity_id, moved.destination);

            for (vector<int>& row : preview_cost)
            {
                fill(row.begin(), row.end(), -1);
            }
            preview_attack = logic->standing_attack();
            checked_entity = moved.entity_id;

            if (logic->available_attacks().empty())
            {
                logic->wait(moved.entity_id);
                clear_preview();
                status = "No target was in Mapmaker::prompt_attack(); unit turn completed.";
                return;
            }

            awaiting_attack_choice = true;
            ostringstream choices;
            choices << "Standing attack range formed. Targets: ";
            for (const avl_for_atk& choice : logic->available_attacks())
            {
                choices << "(" << choice.coords[0] << "," << choice.coords[1]
                        << ", slot " << choice.inventory_id << ") ";
            }
            status = choices.str();
            cout << "Attack options from Mapmaker::prompt_attack():\n";
            for (const avl_for_atk& choice : logic->available_attacks())
            {
                cout << "  attack " << choice.coords[0] << ' ' << choice.coords[1]
                          << ' ' << choice.inventory_id << "  // "
                          << choice.weapon.NAME << '\n';
            }
            cout << "  wait\nworkbench> " << flush;
        }
        catch (const exception& error)
        {
            pending_move.reset();
            clear_preview();
            status = string("Sandbox move handoff failed: ") + error.what();
        }
    }
};

NSImage* make_canvas_image(const fe_tiles::TileCanvas& canvas)
{
    const int width = canvas.pixel_width();
    const int height = canvas.pixel_height();
    unsigned char* pixels = const_cast<unsigned char*>(canvas.rgba().data());
    NSBitmapImageRep* representation = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:&pixels
        pixelsWide:width
        pixelsHigh:height
        bitsPerSample:8
        samplesPerPixel:4
        hasAlpha:YES
        isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bitmapFormat:NSBitmapFormatThirtyTwoBitBigEndian
        bytesPerRow:width * 4
        bitsPerPixel:32];
    if (representation == nil)
    {
        return nil;
    }
    NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    [image addRepresentation:representation];
    return image;
}

NSImage* load_image(const string& path)
{
    return [[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:path.c_str()]];
}

Palette load_gba_palette(const string& path)
{
    ifstream file(path, ios::binary);
    if (!file)
    {
        throw runtime_error("Could not open FE8 map-unit palette: " + path);
    }
    Palette palette{};
    for (PaletteColor& color : palette)
    {
        unsigned char low = 0;
        unsigned char high = 0;
        file.read(reinterpret_cast<char*>(&low), 1);
        file.read(reinterpret_cast<char*>(&high), 1);
        if (!file)
        {
            throw runtime_error("FE8 map-unit palette is incomplete: " + path);
        }
        const uint16_t bgr555 = static_cast<uint16_t>(low) |
                                     (static_cast<uint16_t>(high) << 8);
        color.r = static_cast<uint8_t>(((bgr555 >> 0) & 31) * 255 / 31);
        color.g = static_cast<uint8_t>(((bgr555 >> 5) & 31) * 255 / 31);
        color.b = static_cast<uint8_t>(((bgr555 >> 10) & 31) * 255 / 31);
    }
    return palette;
}

int palette_distance(uint8_t r, uint8_t g, uint8_t b,
                     const PaletteColor& palette_color)
{
    const int dr = static_cast<int>(r) - palette_color.r;
    const int dg = static_cast<int>(g) - palette_color.g;
    const int db = static_cast<int>(b) - palette_color.b;
    return dr * dr + dg * dg + db * db;
}

// The asset extractor writes a colour-keyed GBA sprite sheet as opaque PNG.
// Palette index 0 is the transparent colour; restore that alpha channel
// before the workbench ever draws a unit frame.
NSImage* restore_sprite_transparency(NSImage* source, const Palette& palette)
{
    NSRect proposed = NSMakeRect(0.0, 0.0, source.size.width, source.size.height);
    CGImageRef image = [source CGImageForProposedRect:&proposed context:nil hints:nil];
    if (image == nullptr)
    {
        return nil;
    }
    const size_t width = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);
    vector<uint8_t> pixels(width * height * 4, 0);
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(), width, height, 8, width * 4, colors,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colors);
    if (context == nullptr)
    {
        return nil;
    }
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), image);

    for (size_t pixel = 0; pixel < width * height; ++pixel)
    {
        uint8_t* rgba = pixels.data() + pixel * 4;
        int closest = 0;
        int distance = palette_distance(rgba[0], rgba[1], rgba[2], palette[0]);
        for (int index = 1; index < 16; ++index)
        {
            const int candidate = palette_distance(rgba[0], rgba[1], rgba[2], palette[index]);
            if (candidate < distance)
            {
                closest = index;
                distance = candidate;
            }
        }
        if (closest == 0)
        {
            rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0;
        }
        else
        {
            // Reconstruct the original GBA palette exactly; this avoids small
            // PNG conversion shifts at the edge of an opaque sprite pixel.
            rgba[0] = palette[closest].r;
            rgba[1] = palette[closest].g;
            rgba[2] = palette[closest].b;
            rgba[3] = 255;
        }
    }
    CGImageRef fixed_image = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    NSImage* result = [[NSImage alloc] initWithCGImage:fixed_image size:source.size];
    CGImageRelease(fixed_image);
    return result;
}

// FEBuilderGBA's patch artwork is a GBA background tileset: palette index
// zero is transparent on the original BG layer, but arrives in the PNG as an
// opaque colour. Restore that transparent colour before assembling its TSA.
[[maybe_unused]] NSImage* restore_color_key_transparency(NSImage* source)
{
    NSRect proposed = NSMakeRect(0.0, 0.0, source.size.width, source.size.height);
    CGImageRef image = [source CGImageForProposedRect:&proposed context:nil hints:nil];
    if (image == nullptr)
    {
        return nil;
    }
    const size_t width = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);
    vector<uint8_t> pixels(width * height * 4, 0);
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(), width, height, 8, width * 4, colors,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colors);
    if (context == nullptr)
    {
        return nil;
    }
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), image);

    const uint8_t key_r = pixels[0];
    const uint8_t key_g = pixels[1];
    const uint8_t key_b = pixels[2];
    for (size_t pixel = 0; pixel < width * height; ++pixel)
    {
        uint8_t* rgba = pixels.data() + pixel * 4;
        if (rgba[0] == key_r && rgba[1] == key_g && rgba[2] == key_b)
        {
            rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0;
        }
    }
    CGImageRef fixed_image = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    NSImage* result = [[NSImage alloc] initWithCGImage:fixed_image size:source.size];
    CGImageRelease(fixed_image);
    return result;
}

NSImage* white_sprite(NSImage* source)
{
    NSRect proposed = NSMakeRect(0.0, 0.0, source.size.width, source.size.height);
    CGImageRef image = [source CGImageForProposedRect:&proposed context:nil hints:nil];
    if (image == nullptr)
    {
        return nil;
    }
    const size_t width = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);
    vector<uint8_t> pixels(width * height * 4, 0);
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(), width, height, 8, width * 4, colors,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colors);
    if (context == nullptr)
    {
        return nil;
    }
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), image);
    for (size_t pixel = 0; pixel < width * height; ++pixel)
    {
        uint8_t* rgba = pixels.data() + pixel * 4;
        if (rgba[3] != 0)
        {
            rgba[0] = rgba[1] = rgba[2] = 255;
        }
    }
    CGImageRef white_image = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    NSImage* result = [[NSImage alloc] initWithCGImage:white_image size:source.size];
    CGImageRelease(white_image);
    return result;
}

// Exact tile layouts bundled by FEBuilderGBA's "FE8 Battle Stats with Anims
// Off v2" patch. The high bit is Tiled's horizontal-flip flag.
[[maybe_unused]] constexpr array<uint32_t, 99> kAnimsOffLeftBox = {{
    0x80000006, 7, 8, 9, 4, 4, 4, 4, 4, 4, 4,
    0x80000011, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    0x80000017, 19, 19, 20, 21, 21, 21, 21, 21, 22, 19,
    0x8000001E, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    0x8000001E, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    0x8000001E, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    0x8000001E, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    0x8000001E, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    0x8000000E, 11, 12, 12, 12, 12, 12, 12, 12, 13, 13,
}};

[[maybe_unused]] constexpr array<uint32_t, 108> kAnimsOffRightBox = {{
    1, 4, 4, 4, 4, 4, 4, 4, 7, 8, 9, 6,
    1, 24, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17,
    1, 25, 19, 20, 21, 21, 21, 21, 21, 22, 19, 23,
    1, 31, 28, 28, 28, 28, 28, 28, 28, 28, 28, 30,
    1, 31, 28, 28, 28, 28, 28, 28, 28, 28, 28, 30,
    1, 31, 28, 28, 28, 28, 28, 28, 28, 28, 28, 30,
    1, 31, 28, 28, 28, 28, 28, 28, 28, 28, 28, 30,
    1, 31, 28, 28, 28, 28, 28, 28, 28, 28, 28, 30,
    1, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 14,
}};

NSString* visual_key(fe_tiles::UnitVisual visual)
{
    const string key(fe_tiles::unit_visual_info(visual).key);
    return [NSString stringWithUTF8String:key.c_str()];
}

NSFont* title_font()
{
    return [NSFont systemFontOfSize:22.0 weight:NSFontWeightBold];
}

void draw_text(NSString* text, NSPoint point, NSFont* font, NSColor* color)
{
    NSDictionary* attributes = @{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: color,
    };
    [text drawAtPoint:point withAttributes:attributes];
}
}

@interface MapWorkbenchView : NSView
{
    unique_ptr<Workshop> _workshop;
    NSImage* _canvasImage;
    NSMutableDictionary<NSString*, NSImage*>* _unitSheets;
    NSMutableDictionary<NSString*, NSImage*>* _whiteUnitSheets;
    // Deliberately retained only so the dormant info-box helper methods stay
    // buildable. The workbench no longer loads or draws this UI.
    NSImage* _battleUiSheet;
    NSImage* _battleStatLabels;
    NSImage* _battleNumbers;
    NSRect _boardRect;
    CGFloat _cellPixels;
    NSTimer* _timer;
}
- (instancetype)initWithFrame:(NSRect)frame libraryRoot:(string)root;
- (void)startTerminalCommandReader;
- (void)processTerminalCommand:(const string&)command;
- (void)drawUnit:(const fe_tiles::UnitPose&)pose usingSheets:(NSDictionary<NSString*, NSImage*>*)sheets;
- (void)drawDeathEffects;
- (void)exportRecipe;
@end

@implementation MapWorkbenchView

- (instancetype)initWithFrame:(NSRect)frame libraryRoot:(string)root
{
    self = [super initWithFrame:frame];
    if (self)
    {
        _workshop = make_unique<Workshop>(move(root));
        _canvasImage = _workshop->canvas ? make_canvas_image(*_workshop->canvas) : nil;
        const string fe8_root = FE8_SOURCE_ROOT;
        const string sprite_root = fe8_root + "/graphics/unit_icon/move/";
        const Palette player_palette = load_gba_palette(
            fe8_root + "/graphics/unit_icon/palette/unit_icon_pal_player.agbpal"
        );
        _unitSheets = [[NSMutableDictionary alloc] init];
        _unitSheets[@"Eirika"] = restore_sprite_transparency(
            load_image(sprite_root + "unit_icon_move_Eirika_Lord_sheet.png"), player_palette);
        _unitSheets[@"Paladin"] = restore_sprite_transparency(
            load_image(sprite_root + "unit_icon_move_Paladin_sheet.png"), player_palette);
        _unitSheets[@"Soldier"] = restore_sprite_transparency(
            load_image(sprite_root + "unit_icon_move_Soldier_sheet.png"), player_palette);
        if (_unitSheets[@"Eirika"] == nil || _unitSheets[@"Paladin"] == nil ||
            _unitSheets[@"Soldier"] == nil)
        {
            @throw [NSException exceptionWithName:@"AssetError"
                                           reason:@"Required FE8 map-unit sprite sheets could not be loaded."
                                         userInfo:nil];
        }
        _whiteUnitSheets = [[NSMutableDictionary alloc] init];
        for (NSString* key in _unitSheets)
        {
            _whiteUnitSheets[key] = white_sprite(_unitSheets[key]);
        }

        _cellPixels = 1.0;
        _timer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                                   target:self
                                                 selector:@selector(tick:)
                                                 userInfo:nil
                                                  repeats:YES];
        [self startTerminalCommandReader];
    }
    return self;
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)dealloc
{
    [_timer invalidate];
}

- (void)refreshCanvasImage
{
    _canvasImage = _workshop->canvas ? make_canvas_image(*_workshop->canvas) : nil;
    [self setNeedsDisplay:YES];
}

- (void)startTerminalCommandReader
{
    cout << "\nFE8 Tile Workbench terminal controls\n"
              << "  check <int unit id>\n"
              << "  move <int x offset> <int y offset>\n"
              << "  attack <target x> <target y> <inventory slot>\n"
              << "  wait\n"
              << "  phase <guild id>\n"
              << "  help\n\nworkbench> " << flush;

    __weak MapWorkbenchView* weakSelf = self;
    thread([weakSelf]
    {
        string line;
        while (getline(cin, line))
        {
            const string command = line;
            dispatch_async(dispatch_get_main_queue(), ^{
                MapWorkbenchView* view = weakSelf;
                if (view != nil)
                {
                    [view processTerminalCommand:command];
                }
            });
        }
    }).detach();
}

- (void)processTerminalCommand:(const string&)command
{
    const string response = _workshop->execute_terminal_command(command);
    cout << response << "\nworkbench> " << flush;
    [self setNeedsDisplay:YES];
}

- (void)layoutBoard
{
    const NSRect bounds = self.bounds;
    const CGFloat availableWidth = max<CGFloat>(1.0, bounds.size.width - kSidebarWidth - 3.0 * kMargin);
    const CGFloat availableHeight = max<CGFloat>(1.0, bounds.size.height - 2.0 * kMargin);
    _cellPixels = floor(min(availableWidth / kColumns, availableHeight / kRows));
    _cellPixels = max<CGFloat>(1.0, _cellPixels);
    const CGFloat boardWidth = _cellPixels * kColumns;
    const CGFloat boardHeight = _cellPixels * kRows;
    _boardRect = NSMakeRect(kMargin, kMargin + (availableHeight - boardHeight) / 2.0,
                            boardWidth, boardHeight);
}

- (void)drawUnit:(const fe_tiles::UnitPose&)pose
{
    [self drawUnit:pose usingSheets:_unitSheets];
}

- (void)drawUnit:(const fe_tiles::UnitPose&)pose usingSheets:(NSDictionary<NSString*, NSImage*>*)sheets
{
    NSImage* sheet = sheets[visual_key(pose.visual)];
    if (sheet == nil)
    {
        return;
    }

    // FE8's map-unit cells are 32×32 while terrain cells are 16×16. The game
    // does *not* centre this 32×32 canvas in a map cell: its OAM origin is
    // {-16, -32} around {tile.x * 16 + 8, tile.y * 16 + 16}. In other words,
    // the canvas is horizontally centred and vertically bottom-anchored to
    // the current terrain tile. Preserve that exact anchor at any scale.
    const CGFloat frameSize = _cellPixels * kUnitCanvasInTiles;
    const NSRect destination = NSMakeRect(
                                          NSMinX(_boardRect) + (pose.x + 0.5) * _cellPixels - frameSize * 0.5,
                                          NSMinY(_boardRect) + (pose.y + 1.0) * _cellPixels - frameSize,
                                          frameSize, frameSize);
    const NSRect source = NSMakeRect(0.0,
                                     sheet.size.height - (pose.sheet_cell + 1) * 32.0,
                                     32.0, 32.0);

    [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
    [NSGraphicsContext saveGraphicsState];
    if (pose.flip_x)
    {
        NSAffineTransform* flip = [NSAffineTransform transform];
        [flip translateXBy:2.0 * NSMidX(destination) yBy:0.0];
        [flip scaleXBy:-1.0 yBy:1.0];
        [flip concat];
    }
    [sheet drawInRect:destination
              fromRect:source
             operation:NSCompositingOperationSourceOver
              fraction:1.0
        respectFlipped:YES
                 hints:nil];
    [NSGraphicsContext restoreGraphicsState];
}

- (void)drawUnits
{
    // SpriteLayer::poses() is already deduplicated by entity ID. A moving
    // entity contributes its fractional animation pose only, never an extra
    // static sprite at its old occupancy cell.
    const optional<fe_tiles::AttackEffect>& attack =
        _workshop->combat_presentation.attack_effect();
    for (const fe_tiles::UnitPose& pose : _workshop->sprites.poses())
    {
        if (attack.has_value() && pose.entity_id == attack->pose.entity_id)
        {
            continue;
        }
        [self drawUnit:pose];
    }
    if (attack.has_value())
    {
        [self drawUnit:attack->pose];
    }
}

- (void)drawDeathEffects
{
    for (const fe_tiles::DeathEffect& effect : _workshop->combat_presentation.death_effects())
    {
        // This is FE8's map-unit death, not the full battle-sprite effect:
        // MU_StartDeathFade freezes the MU, flashes its palette white, and
        // blends it to zero alpha over timeLeft = 0x20 frames.
        const CGFloat opacity = clamp(
            (32.0 - static_cast<CGFloat>(effect.tick)) / 32.0, 0.0, 1.0
        );
        [NSGraphicsContext saveGraphicsState];
        CGContextSetAlpha([[NSGraphicsContext currentContext] CGContext], opacity);
        [self drawUnit:effect.pose usingSheets:_whiteUnitSheets];
        [NSGraphicsContext restoreGraphicsState];
    }
}

- (void)drawBattleBox:(const uint32_t*)tiles
                width:(int)width
               height:(int)height
                   at:(NSPoint)origin
                scale:(CGFloat)scale
{
    if (_battleUiSheet == nil)
    {
        return;
    }
    constexpr uint32_t kTiledFlipHorizontal = 0x80000000;
    constexpr uint32_t kTiledGidMask = 0x1FFFFFFF;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const uint32_t gid = tiles[y * width + x];
            if ((gid & kTiledGidMask) == 0)
            {
                continue;
            }
            const int tile_index = static_cast<int>((gid & kTiledGidMask) - 1);
            const NSRect source = NSMakeRect((tile_index % 8) * 8,
                                             _battleUiSheet.size.height - (tile_index / 8 + 1) * 8,
                                             8.0, 8.0);
            const NSRect destination = NSMakeRect(origin.x + x * 8.0 * scale,
                                                   origin.y + y * 8.0 * scale,
                                                   8.0 * scale, 8.0 * scale);
            [NSGraphicsContext saveGraphicsState];
            if (gid & kTiledFlipHorizontal)
            {
                NSAffineTransform* flip = [NSAffineTransform transform];
                [flip translateXBy:2.0 * NSMidX(destination) yBy:0.0];
                [flip scaleXBy:-1.0 yBy:1.0];
                [flip concat];
            }
            [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
            [_battleUiSheet drawInRect:destination
                               fromRect:source
                              operation:NSCompositingOperationSourceOver
                               fraction:1.0
                         respectFlipped:YES
                                  hints:nil];
            [NSGraphicsContext restoreGraphicsState];
        }
    }
}

- (void)drawBattleStatLabel:(int)label at:(NSPoint)point scale:(CGFloat)scale
{
    static constexpr array<int, 4> start_x = {0, 16, 40, 56};
    static constexpr array<int, 4> widths = {16, 24, 16, 24};
    if (_battleStatLabels == nil || label < 0 || label >= static_cast<int>(start_x.size()))
    {
        return;
    }
    const NSRect source = NSMakeRect(start_x[label], 0.0, widths[label], 8.0);
    const NSRect destination = NSMakeRect(point.x, point.y, widths[label] * scale, 8.0 * scale);
    [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
    [_battleStatLabels drawInRect:destination
                          fromRect:source
                         operation:NSCompositingOperationSourceOver
                          fraction:1.0
                    respectFlipped:YES
                             hints:nil];
}

- (void)drawBattleStatValue:(int)value at:(NSPoint)point scale:(CGFloat)scale
{
    if (_battleNumbers == nil)
    {
        return;
    }
    string number = value < 0 ? "-" : to_string(min(value, 999));
    const CGFloat glyph_width = 8.0 * scale;
    CGFloat x = point.x + glyph_width * (3 - static_cast<int>(number.size()));
    for (const char character : number)
    {
        int source_x = 0;
        int source_y = 0;
        if (character == '-')
        {
            source_x = 8;
        }
        else
        {
            const int digit = character - '0';
            source_x = (digit == 9 ? 0 : digit * 8);
            source_y = (digit == 9 ? 0 : 8);
        }
        const NSRect source = NSMakeRect(source_x, source_y, 8.0, 8.0);
        const NSRect destination = NSMakeRect(x, point.y, glyph_width, 8.0 * scale);
        [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
        [_battleNumbers drawInRect:destination
                           fromRect:source
                          operation:NSCompositingOperationSourceOver
                           fraction:1.0
                     respectFlipped:YES
                              hints:nil];
        x += glyph_width;
    }
}

- (void)drawBattleInfo
{
    // The map information UI is intentionally parked for this pass. The
    // workbench renders only the authentic map-unit lunge and death fade.
}

- (void)drawRect:(NSRect)dirtyRect
{
    [[NSColor colorWithCalibratedRed:0.035 green:0.05 blue:0.09 alpha:1.0] setFill];
    NSRectFill(self.bounds);
    [self layoutBoard];

    if (_canvasImage != nil)
    {
        [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
        [_canvasImage drawInRect:_boardRect
                        fromRect:NSMakeRect(0, 0, _canvasImage.size.width, _canvasImage.size.height)
                       operation:NSCompositingOperationSourceOver
                        fraction:1.0
                  respectFlipped:YES
                           hints:nil];
    }
    else
    {
        [[NSColor systemRedColor] set];
        draw_text(@"No canvas could be rendered.", NSMakePoint(kMargin, kMargin),
                  [NSFont systemFontOfSize:15], [NSColor systemRedColor]);
    }

    // This exists only for a terminal `check <id>` command. Terrain and the
    // underlying raw tile canvas are never changed by a movement preview.
    for (int y = 0; y < kRows; ++y)
    {
        for (int x = 0; x < kColumns; ++x)
        {
            const bool movement = _workshop->preview_cost[y][x] >= 0;
            const bool attack_only = !movement && _workshop->preview_attack[y][x] > 0;
            if (!movement && !attack_only)
            {
                continue;
            }
            const NSRect cell = NSMakeRect(NSMinX(_boardRect) + x * _cellPixels,
                                           NSMinY(_boardRect) + y * _cellPixels,
                                           _cellPixels, _cellPixels);
            if (movement)
            {
                [[NSColor colorWithCalibratedRed:0.08 green:0.42 blue:1.0 alpha:0.34] setFill];
            }
            else
            {
                // Red is deliberately only for cells outside Mapmaker's
                // landable movement layer.
                [[NSColor colorWithCalibratedRed:0.72 green:0.17 blue:0.10 alpha:0.42] setFill];
            }
            [[NSBezierPath bezierPathWithRect:NSInsetRect(cell, 1.0, 1.0)] fill];
        }
    }

    [self drawUnits];
    [self drawDeathEffects];

    [[NSColor colorWithWhite:1.0 alpha:0.16] setStroke];
    NSBezierPath* border = [NSBezierPath bezierPathWithRect:_boardRect];
    border.lineWidth = 1.0;
    [border stroke];

    const CGFloat sideX = NSMaxX(_boardRect) + kMargin;
    NSColor* bright = [NSColor colorWithCalibratedWhite:0.94 alpha:1.0];
    NSColor* muted = [NSColor colorWithCalibratedWhite:0.68 alpha:1.0];
    NSColor* accent = [NSColor colorWithCalibratedRed:0.36 green:0.73 blue:1.0 alpha:1.0];

    draw_text(@"FE8 Tile Workbench", NSMakePoint(sideX, 34), title_font(), bright);
    draw_text(@"Terrain + occupancy · in memory", NSMakePoint(sideX, 65),
              [NSFont systemFontOfSize:13], accent);

    ostringstream selection;
    selection << "Theme: " << _workshop->theme << "\n"
              << "Class: " << class_name(_workshop->selected_class)
              << " (" << _workshop->selected_class << ")\n"
              << "Subclass: " << _workshop->selected_subclass << " / "
              << max(0, _workshop->selected_subclass_count - 1) << "\n"
              << "Orientation: " << _workshop->selected_orientation << " / "
              << max(0, _workshop->selected_orientation_count - 1);
    const string text = selection.str();
    NSArray<NSString*>* selectionLines = [[NSString stringWithUTF8String:text.c_str()]
        componentsSeparatedByString:@"\n"];
    CGFloat y = 110;
    for (NSString* line in selectionLines)
    {
        draw_text(line, NSMakePoint(sideX, y), [NSFont monospacedSystemFontOfSize:14 weight:NSFontWeightMedium], bright);
        y += 24;
    }

    const array<NSString*, 16> instructions = {
        @"Click            paint",
        @"Right click      sample",
        @"1–5             basic class",
        @"[ / ]           subclass",
        @"- / =           orientation",
        @"Up / Down       chapter theme",
        @"R               fill canvas",
        @"Space           describe live controls",
        @"Terminal         check <id>",
        @"Terminal         move <dx> <dy>",
        @"Terminal         attack <x> <y> <slot>",
        @"Terminal         wait",
        @"Terminal         phase <guild>",
        @"E               export recipe",
        @"Esc / close      export + quit",
        @"",
    };
    y += 32;
    draw_text(@"CONTROLS", NSMakePoint(sideX, y), [NSFont systemFontOfSize:12 weight:NSFontWeightBold], accent);
    y += 26;
    for (NSString* line : instructions)
    {
        draw_text(line, NSMakePoint(sideX, y), [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular], muted);
        y += 20;
    }

    draw_text(@"STATUS", NSMakePoint(sideX, y + 12), [NSFont systemFontOfSize:12 weight:NSFontWeightBold], accent);
    const NSString* status = [NSString stringWithUTF8String:_workshop->status.c_str()];
    const NSRect statusRect = NSMakeRect(sideX, y + 34, kSidebarWidth - kMargin, 110);
    NSDictionary* statusAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:12],
        NSForegroundColorAttributeName: muted,
    };
    [status drawWithRect:statusRect options:NSStringDrawingUsesLineFragmentOrigin attributes:statusAttributes];
}

- (BOOL)cellFromEvent:(NSEvent*)event x:(int*)x y:(int*)y
{
    [self layoutBoard];
    const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    if (!NSPointInRect(point, _boardRect))
    {
        return NO;
    }
    *x = static_cast<int>((point.x - NSMinX(_boardRect)) / _cellPixels);
    *y = static_cast<int>((point.y - NSMinY(_boardRect)) / _cellPixels);
    return *x >= 0 && *x < kColumns && *y >= 0 && *y < kRows;
}

- (void)mouseDown:(NSEvent*)event
{
    int x = 0;
    int y = 0;
    if ([self cellFromEvent:event x:&x y:&y])
    {
        _workshop->paint(x, y);
        [self refreshCanvasImage];
    }
}

- (void)rightMouseDown:(NSEvent*)event
{
    int x = 0;
    int y = 0;
    if ([self cellFromEvent:event x:&x y:&y])
    {
        _workshop->sample(x, y);
        [self setNeedsDisplay:YES];
    }
}

- (void)keyDown:(NSEvent*)event
{
    if (event.keyCode == 53) // Escape
    {
        [NSApp terminate:nil];
        return;
    }
    if (event.keyCode == 126) // up arrow
    {
        _workshop->change_theme(-1);
        [self refreshCanvasImage];
        return;
    }
    if (event.keyCode == 125) // down arrow
    {
        _workshop->change_theme(1);
        [self refreshCanvasImage];
        return;
    }

    const NSString* characters = event.charactersIgnoringModifiers;
    if (characters.length == 0)
    {
        return;
    }
    const unichar key = [characters characterAtIndex:0];
    bool needsCanvasRefresh = false;
    switch (key)
    {
        case '1': _workshop->choose_class(fe_tiles::PLAIN); break;
        case '2': _workshop->choose_class(fe_tiles::FOREST); break;
        case '3': _workshop->choose_class(fe_tiles::PEAK); break;
        case '4': _workshop->choose_class(fe_tiles::LAKE); break;
        case '5': _workshop->choose_class(fe_tiles::HOUSE); break;
        case '[': _workshop->cycle_subclass(-1); break;
        case ']': _workshop->cycle_subclass(1); break;
        case '-': _workshop->cycle_orientation(-1); break;
        case '=': _workshop->cycle_orientation(1); break;
        case 'r':
        case 'R': _workshop->fill_selected(); needsCanvasRefresh = true; break;
        case 'e':
        case 'E': [self exportRecipe]; return;
        case ' ': _workshop->toggle_invariant_movement(); break;
        default: return;
    }
    if (needsCanvasRefresh)
    {
        [self refreshCanvasImage];
    }
    else
    {
        [self setNeedsDisplay:YES];
    }
}

- (void)exportRecipe
{
    const string output_path = _workshop->root + "/viewer/exported_recipe.cpp";
    _workshop->export_recipe(output_path);
    [self setNeedsDisplay:YES];
}

- (void)tick:(NSTimer*)timer
{
    _workshop->tick_fe_frame();
    [self setNeedsDisplay:YES];
}
@end

@interface WorkbenchAppDelegate : NSObject <NSApplicationDelegate>
{
    MapWorkbenchView* _view;
}
@end

@implementation WorkbenchAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    const string root = FE_TILE_LIBRARY_ROOT;
    NSRect frame = NSMakeRect(0, 0, 1320, 790);
    NSWindow* window = [[NSWindow alloc]
        initWithContentRect:frame
        styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                   NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
        backing:NSBackingStoreBuffered
        defer:NO];
    window.title = @"FE8 Tile Workbench";
    window.minSize = NSMakeSize(920, 530);
    _view = [[MapWorkbenchView alloc] initWithFrame:frame libraryRoot:root];
    window.contentView = _view;
    [window makeKeyAndOrderFront:nil];
    [window makeFirstResponder:_view];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
    [_view exportRecipe];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
    return YES;
}
@end

int main(int argc, const char* argv[])
{
    @autoreleasepool
    {
        if (argc == 3)
        {
            try
            {
                kRows = stoi(argv[1]);
                kColumns = stoi(argv[2]);
            }
            catch (const exception&)
            {
                cerr << "Usage: fe8_map_workbench [rows columns]\n";
                return 1;
            }
        }
        else if (argc != 1)
        {
            cerr << "Usage: fe8_map_workbench [rows columns]\n";
            return 1;
        }
        if (kRows <= 0 || kColumns <= 0)
        {
            cerr << "Rows and columns must both be positive.\n";
            return 1;
        }

        NSApplication* app = [NSApplication sharedApplication];
        WorkbenchAppDelegate* delegate = [[WorkbenchAppDelegate alloc] init];
        app.delegate = delegate;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app run];
    }
    return 0;
}
