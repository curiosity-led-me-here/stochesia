#!/usr/bin/env python3
"""Import FE8 visual/audio assets and generate the Stochesia FE8 catalogues.

This is an import-time tool only. After it finishes, map_tile_library contains
every runtime asset and data catalogue it needs; the original FE8 checkout is
not consulted by the renderer or map loader.
"""

from __future__ import annotations

import argparse
import csv
import re
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / "assets" / "fe8"
DATA_ROOT = ROOT / "data" / "fe8"
INCLUDE_ROOT = ROOT / "include"
SOURCE_ROOT = ROOT / "src"


def copy_tree(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise RuntimeError(f"Missing FE8 asset directory: {source}")
    shutil.copytree(source, destination, dirs_exist_ok=True)


def parse_constants(path: Path, prefix: str) -> dict[str, int]:
    result: dict[str, int] = {}
    pattern = re.compile(rf"^\s*({prefix}[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", re.M)
    for symbol, value in pattern.findall(path.read_text()):
        result[symbol] = int(value, 0)
    return result


def entry_blocks(text: str, prefix: str) -> list[tuple[str, str]]:
    start_pattern = re.compile(rf"\[({prefix}[A-Z0-9_]+)\s*-\s*1\]\s*=\s*\{{")
    result: list[tuple[str, str]] = []
    for match in start_pattern.finditer(text):
        depth = 1
        cursor = match.end()
        while cursor < len(text) and depth:
            if text[cursor] == "{":
                depth += 1
            elif text[cursor] == "}":
                depth -= 1
            cursor += 1
        if depth:
            raise RuntimeError(f"Unclosed initializer for {match.group(1)}")
        result.append((match.group(1), text[match.end():cursor - 1]))
    return result


def field(block: str, name: str, fallback: str = "") -> str:
    match = re.search(rf"\.{re.escape(name)}\s*=\s*([^,\n]+)", block)
    return match.group(1).strip() if match else fallback


def integer(value: str, constants: dict[str, int], fallback: int = 0) -> int:
    value = value.strip()
    if value in constants:
        return constants[value]
    try:
        return int(value, 0)
    except ValueError:
        return fallback


def pretty(symbol: str, prefix: str) -> str:
    return symbol.removeprefix(prefix).replace("_", " ").title()


def enum_name(asset_key: str) -> str:
    return "".join(part[:1].upper() + part[1:].lower() if part.isupper() else part[:1].upper() + part[1:]
                   for part in asset_key.split("_"))


def write_tsv(path: Path, rows: list[dict[str, object]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def asset_manifest() -> list[dict[str, object]]:
    """Return every raw runtime asset copied into this self-contained bundle."""
    rows = []
    for family in ("graphics", "sound"):
        root = ASSET_ROOT / family
        for path in sorted(candidate for candidate in root.rglob("*") if candidate.is_file()):
            relative = path.relative_to(ASSET_ROOT).as_posix()
            rows.append({
                "family": family,
                "extension": path.suffix.lower().lstrip("."),
                "bytes": path.stat().st_size,
                "relative_path": relative,
            })
    return rows


def generate_unit_visuals(fe8: Path, classes: dict[str, int]) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    move_dir = fe8 / "graphics" / "unit_icon" / "move"
    wait_dir = fe8 / "graphics" / "unit_icon" / "wait"
    move_prefix = "unit_icon_move_"
    move_suffix = "_sheet.png"
    assets = sorted(
        path.name[len(move_prefix):-len(move_suffix)]
        for path in move_dir.glob(f"{move_prefix}*{move_suffix}")
    )
    rows = []
    for index, asset in enumerate(assets):
        wait_name = f"unit_icon_wait_{asset}_sheet.png"
        rows.append({
            "visual_id": index,
            "enum": enum_name(asset),
            "asset_key": asset,
            "move_png": f"graphics/unit_icon/move/{move_prefix}{asset}{move_suffix}",
            "wait_png": f"graphics/unit_icon/wait/{wait_name}" if (wait_dir / wait_name).exists() else "",
            "source_motion": f"unit_icon_move_{asset}_motion",
        })

    move_table = (fe8 / "src" / "unit_icon_move_data.c").read_text()
    table_assets = re.findall(r"\{unit_icon_move_([A-Za-z0-9_]+)_sheet,\s*unit_icon_move_[A-Za-z0-9_]+_motion\}", move_table)
    visual_by_asset = {row["asset_key"]: row for row in rows}
    class_by_id = {value: key for key, value in classes.items() if value > 0}
    class_rows = []
    for table_index, asset in enumerate(table_assets):
        class_id = table_index + 1
        visual = visual_by_asset.get(asset)
        if visual is None:
            raise RuntimeError(f"Move table references missing visual asset {asset}")
        class_rows.append({
            "class_id": class_id,
            "class_symbol": class_by_id.get(class_id, f"CLASS_UNKNOWN_{class_id}"),
            "visual_id": visual["visual_id"],
            "visual_enum": visual["enum"],
            "asset_key": asset,
        })
    return rows, class_rows


def generate_motion_catalogue(fe8: Path, visual_rows: list[dict[str, object]]) -> list[dict[str, object]]:
    source = (fe8 / "data" / "const_data_unit_icon_move.s").read_text()
    rows = []
    for visual in visual_rows:
        key = str(visual["asset_key"])
        # Most map-unit sheets expose the five standard movement programs.
        # A few classes (for example Dancer) expose additional action programs;
        # preserve those literal source programs too rather than discarding them.
        animations = sorted({
            int(value) for value in re.findall(
                rf"^unit_icon_move_{re.escape(key)}_anim_(\d+):",
                source,
                re.M,
            )
        })
        if not animations:
            raise RuntimeError(f"Missing movement programs for {key}")

        standard_names = ("left", "right", "down", "up", "selected")
        for animation in animations:
            pattern = re.compile(
                rf"^unit_icon_move_{re.escape(key)}_anim_{animation}:.*?(?=^unit_icon_move_{re.escape(key)}_anim_|^\s*\.align|\Z)",
                re.M | re.S,
            )
            match = pattern.search(source)
            if not match:
                raise RuntimeError(f"Missing motion program {key} animation {animation}")
            pairs = re.findall(r"^\s*\.2byte\s+(\d+),\s*(\d+)", match.group(0), re.M)
            rows.append({
                "visual_id": visual["visual_id"],
                "visual_enum": visual["enum"],
                "asset_key": key,
                "animation_id": animation,
                "animation": (standard_names[animation]
                              if animation < len(standard_names)
                              else f"action_{animation}"),
                "frames": ",".join(frame for _, frame in pairs),
                "ticks": ",".join(tick for tick, _ in pairs),
            })
    return rows


def generate_character_data(fe8: Path, characters: dict[str, int], classes: dict[str, int]) -> list[dict[str, object]]:
    text = (fe8 / "src" / "data_characters.c").read_text()
    stat_fields = ("baseHP", "basePow", "baseSkl", "baseSpd", "baseDef", "baseRes", "baseLck", "baseCon",
                   "growthHP", "growthPow", "growthSkl", "growthSpd", "growthDef", "growthRes", "growthLck")
    rows = []
    for symbol, block in entry_blocks(text, "CHARACTER_"):
        row: dict[str, object] = {
            "id": characters.get(symbol, 0),
            "symbol": symbol,
            "name": pretty(symbol, "CHARACTER_"),
            "default_class_symbol": field(block, "defaultClass"),
            "default_class_id": integer(field(block, "defaultClass"), classes),
            "portrait_id": integer(field(block, "portraitId"), {}),
            "affinity": field(block, "affinity"),
            "base_level": integer(field(block, "baseLevel"), {}),
            "attributes": field(block, "attributes"),
            "visit_group": integer(field(block, "visit_group"), {}),
        }
        for stat in stat_fields:
            row[stat] = integer(field(block, stat), {})
        rows.append(row)
    return sorted(rows, key=lambda row: int(row["id"]))


def generate_class_data(fe8: Path, classes: dict[str, int]) -> list[dict[str, object]]:
    text = (fe8 / "src" / "data_classes.c").read_text()
    stats = ("HP", "Pow", "Skl", "Spd", "Def", "Res", "Con")
    rows = []
    for symbol, block in entry_blocks(text, "CLASS_"):
        mov_cost = re.search(r"\.pMovCostTable\s*=\s*\{\s*([^,\n]+)", block, re.S)
        row: dict[str, object] = {
            "id": classes.get(symbol, 0),
            "symbol": symbol,
            "name": pretty(symbol, "CLASS_"),
            "promotion_symbol": field(block, "promotion"),
            "promotion_id": integer(field(block, "promotion"), classes),
            "sms_id": integer(field(block, "SMSId"), {}),
            "slow_walking": integer(field(block, "slowWalking"), {}),
            "attributes": field(block, "attributes"),
            "movement_cost_table": mov_cost.group(1).strip() if mov_cost else "",
        }
        for stat in stats:
            row[f"base{stat}"] = integer(field(block, f"base{stat}"), {})
            row[f"max{stat}"] = integer(field(block, f"max{stat}"), {})
            row[f"growth{stat}"] = integer(field(block, f"growth{stat}"), {})
            row[f"promotion{stat}"] = integer(field(block, f"promotion{stat}"), {})
        row["baseMov"] = integer(field(block, "baseMov"), {})
        rows.append(row)
    return sorted(rows, key=lambda row: int(row["id"]))


def generate_visual_header(rows: list[dict[str, object]], motion_rows: list[dict[str, object]]) -> str:
    enum_rows = ",\n".join(f"    {row['enum']} = {row['visual_id']}" for row in rows)
    return f'''// Generated by tools/import_fe8_assets.py. Do not hand-edit.
#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace fe_tiles
{{
// Every literal FE8 map-unit movement-sheet variant present in this bundle.
// A visual is art, not a gameplay class: several classes can share one sheet.
enum class UnitVisual : int
{{
{enum_rows},
    Count,

    // Compatibility alias used by the original three-unit monitor proof.
    Eirika = EirikaLord,
}};

struct UnitVisualInfo
{{
    UnitVisual visual;
    std::string_view key;
    std::string_view move_png; // relative to assets/fe8/
    std::string_view wait_png; // relative to assets/fe8/; may be empty
    std::string_view source_motion_symbol;
}};

// Literal AP timing program from FE8's const_data_unit_icon_move.s.
// IDs 0..4 are left/right/down/up/selected; a few visuals add actions.
struct MapUnitMotionStep
{{
    int ticks;
    int frame;
}};

struct MapUnitMotionProgram
{{
    UnitVisual visual;
    int animation_id;
    std::string_view name;
    std::vector<MapUnitMotionStep> steps;
}};

const UnitVisualInfo& unit_visual_info(UnitVisual visual);
const std::vector<UnitVisualInfo>& all_unit_visuals();
std::optional<UnitVisual> unit_visual_for_class(int fe8_class_id);
const std::vector<MapUnitMotionProgram>& all_map_unit_motion_programs();
const MapUnitMotionProgram* map_unit_motion_program(UnitVisual visual,
                                                    int animation_id);
}}
'''


def generate_visual_cpp(rows: list[dict[str, object]], class_rows: list[dict[str, object]],
                        motion_rows: list[dict[str, object]]) -> str:
    infos = ",\n".join(
        f'    {{UnitVisual::{row["enum"]}, "{row["asset_key"]}", "{row["move_png"]}", "{row["wait_png"]}", "{row["source_motion"]}"}}'
        for row in rows
    )
    class_entries = "\n".join(
        f'        case {row["class_id"]}: return UnitVisual::{row["visual_enum"]};'
        for row in class_rows
    )
    motion_infos = []
    for row in motion_rows:
        ticks = str(row["ticks"]).split(",")
        frames = str(row["frames"]).split(",")
        steps = ", ".join(f"{{{tick}, {frame}}}" for tick, frame in zip(ticks, frames))
        motion_infos.append(
            f'    {{UnitVisual::{row["visual_enum"]}, {row["animation_id"]}, '
            f'"{row["animation"]}", {{{steps}}}}}'
        )
    motions = ",\n".join(motion_infos)
    return f'''// Generated by tools/import_fe8_assets.py. Do not hand-edit.
#include "fe8_unit_visuals.h"

#include <stdexcept>

namespace fe_tiles
{{
namespace
{{
const std::vector<UnitVisualInfo> kVisuals = {{
{infos}
}};

const std::vector<MapUnitMotionProgram> kMotionPrograms = {{
{motions}
}};
}}

const UnitVisualInfo& unit_visual_info(UnitVisual visual)
{{
    const int index = static_cast<int>(visual);
    if (index < 0 || index >= static_cast<int>(kVisuals.size()))
    {{
        throw std::out_of_range("Unknown FE8 UnitVisual.");
    }}
    return kVisuals[static_cast<std::size_t>(index)];
}}

const std::vector<UnitVisualInfo>& all_unit_visuals()
{{
    return kVisuals;
}}

std::optional<UnitVisual> unit_visual_for_class(int fe8_class_id)
{{
    switch (fe8_class_id)
    {{
{class_entries}
        default: return std::nullopt;
    }}
}}

const std::vector<MapUnitMotionProgram>& all_map_unit_motion_programs()
{{
    return kMotionPrograms;
}}

const MapUnitMotionProgram* map_unit_motion_program(UnitVisual visual,
                                                    int animation_id)
{{
    for (const MapUnitMotionProgram& program : kMotionPrograms)
    {{
        if (program.visual == visual && program.animation_id == animation_id)
        {{
            return &program;
        }}
    }}
    return nullptr;
}}
}}
'''


def stat_initializer(row: dict[str, object], prefix: str, mov_key: str = "") -> str:
    values = [str(row.get(prefix + stat, 0)) for stat in ("HP", "Pow", "Skl", "Spd", "Def", "Res")]
    values.append(str(row.get(prefix + "Lck", 0)))
    values.append(str(row.get(prefix + "Con", 0)))
    values.append(str(row.get(mov_key, 0)) if mov_key else "0")
    return "{" + ", ".join(values) + "}"


def generate_data_header() -> str:
    return '''// Generated data API. Source records are exported by import_fe8_assets.py.
#pragma once

#include <string_view>
#include <vector>

namespace fe_tiles
{
struct Fe8StatBlock
{
    int hp = 0;
    int pow = 0;
    int skl = 0;
    int spd = 0;
    int def = 0;
    int res = 0;
    int lck = 0;
    int con = 0;
    int mov = 0;
};

// Character bases are FE8's personal modifiers. Combine them with the
// matching ClassRecord.base for the displayed initial statline.
struct Fe8CharacterRecord
{
    int id = 0;
    std::string_view symbol;
    std::string_view name;
    int default_class_id = 0;
    int portrait_id = 0;
    int base_level = 0;
    Fe8StatBlock personal_base;
    Fe8StatBlock growth;
    std::string_view affinity;
    std::string_view attributes;
    int visit_group = 0;
};

struct Fe8ClassRecord
{
    int id = 0;
    std::string_view symbol;
    std::string_view name;
    int promotion_id = 0;
    int sms_id = 0;
    bool slow_walking = false;
    Fe8StatBlock base;
    Fe8StatBlock maximum;
    Fe8StatBlock growth;
    Fe8StatBlock promotion;
    std::string_view attributes;
    std::string_view movement_cost_table;
};

const std::vector<Fe8CharacterRecord>& fe8_characters();
const std::vector<Fe8ClassRecord>& fe8_classes();
const Fe8CharacterRecord* fe8_character(int character_id);
const Fe8ClassRecord* fe8_class(int class_id);

// FE8's displayed initial stats: class base + character personal modifiers.
Fe8StatBlock resolved_base_stats(const Fe8CharacterRecord& character,
                                 const Fe8ClassRecord& unit_class);
}
'''


def generate_data_cpp(chars: list[dict[str, object]], classes: list[dict[str, object]]) -> str:
    char_entries = []
    for row in chars:
        personal = "{" + ", ".join(str(row[f"base{stat}"]) for stat in ("HP", "Pow", "Skl", "Spd", "Def", "Res", "Lck", "Con")) + ", 0}"
        growth = "{" + ", ".join(str(row[f"growth{stat}"]) for stat in ("HP", "Pow", "Skl", "Spd", "Def", "Res", "Lck")) + ", 0, 0}"
        char_entries.append(
            f'    {{{row["id"]}, "{row["symbol"]}", "{row["name"]}", {row["default_class_id"]}, {row["portrait_id"]}, {row["base_level"]}, {personal}, {growth}, "{row["affinity"]}", "{row["attributes"]}", {row["visit_group"]}}}'
        )
    class_entries = []
    for row in classes:
        base = stat_initializer(row, "base", "baseMov")
        maximum = stat_initializer(row, "max")
        growth = stat_initializer(row, "growth")
        promotion = stat_initializer(row, "promotion")
        class_entries.append(
            f'    {{{row["id"]}, "{row["symbol"]}", "{row["name"]}", {row["promotion_id"]}, {row["sms_id"]}, {"true" if row["slow_walking"] else "false"}, {base}, {maximum}, {growth}, {promotion}, "{row["attributes"]}", "{row["movement_cost_table"]}"}}'
        )
    return f'''// Generated by tools/import_fe8_assets.py. Do not hand-edit.
#include "fe8_unit_data.h"

namespace fe_tiles
{{
namespace
{{
const std::vector<Fe8CharacterRecord> kCharacters = {{
{",\n".join(char_entries)}
}};

const std::vector<Fe8ClassRecord> kClasses = {{
{",\n".join(class_entries)}
}};
}}

const std::vector<Fe8CharacterRecord>& fe8_characters() {{ return kCharacters; }}
const std::vector<Fe8ClassRecord>& fe8_classes() {{ return kClasses; }}

const Fe8CharacterRecord* fe8_character(int character_id)
{{
    for (const Fe8CharacterRecord& character : kCharacters)
    {{
        if (character.id == character_id) return &character;
    }}
    return nullptr;
}}

const Fe8ClassRecord* fe8_class(int class_id)
{{
    for (const Fe8ClassRecord& unit_class : kClasses)
    {{
        if (unit_class.id == class_id) return &unit_class;
    }}
    return nullptr;
}}

Fe8StatBlock resolved_base_stats(const Fe8CharacterRecord& character,
                                 const Fe8ClassRecord& unit_class)
{{
    return {{
        unit_class.base.hp + character.personal_base.hp,
        unit_class.base.pow + character.personal_base.pow,
        unit_class.base.skl + character.personal_base.skl,
        unit_class.base.spd + character.personal_base.spd,
        unit_class.base.def + character.personal_base.def,
        unit_class.base.res + character.personal_base.res,
        character.personal_base.lck,
        unit_class.base.con + character.personal_base.con,
        unit_class.base.mov,
    }};
}}
}}
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True, help="Local FE8 decomp/source checkout")
    args = parser.parse_args()
    fe8 = args.source.resolve()

    # Full visual/audio asset snapshot, including battle graphics, portraits,
    # UI, every map tileset, every map-unit move/wait sheet, and music/SFX.
    copy_tree(fe8 / "graphics", ASSET_ROOT / "graphics")
    copy_tree(fe8 / "sound", ASSET_ROOT / "sound")

    source_dest = DATA_ROOT / "source"
    source_dest.mkdir(parents=True, exist_ok=True)
    for relative in (
        "src/data_characters.c",
        "src/data_classes.c",
        "src/unit_icon_move_data.c",
        "src/unit_icon_wait_data.c",
        "data/const_data_unit_icon_move.s",
        "data/const_data_unit_icon_wait.s",
        "include/constants/characters.h",
        "include/constants/classes.h",
        "include/bmunit.h",
    ):
        src = fe8 / relative
        destination = source_dest / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, destination)

    characters = parse_constants(fe8 / "include/constants/characters.h", "CHARACTER_")
    classes = parse_constants(fe8 / "include/constants/classes.h", "CLASS_")
    visual_rows, class_visual_rows = generate_unit_visuals(fe8, classes)
    motion_rows = generate_motion_catalogue(fe8, visual_rows)
    character_rows = generate_character_data(fe8, characters, classes)
    class_rows = generate_class_data(fe8, classes)

    write_tsv(DATA_ROOT / "unit_visuals.tsv", visual_rows,
              ["visual_id", "enum", "asset_key", "move_png", "wait_png", "source_motion"])
    write_tsv(DATA_ROOT / "class_visuals.tsv", class_visual_rows,
              ["class_id", "class_symbol", "visual_id", "visual_enum", "asset_key"])
    write_tsv(DATA_ROOT / "map_unit_motion.tsv", motion_rows,
              ["visual_id", "visual_enum", "asset_key", "animation_id", "animation", "frames", "ticks"])
    write_tsv(DATA_ROOT / "characters.tsv", character_rows,
              ["id", "symbol", "name", "default_class_symbol", "default_class_id", "portrait_id", "affinity", "base_level",
               "baseHP", "basePow", "baseSkl", "baseSpd", "baseDef", "baseRes", "baseLck", "baseCon",
               "growthHP", "growthPow", "growthSkl", "growthSpd", "growthDef", "growthRes", "growthLck", "attributes", "visit_group"])
    write_tsv(DATA_ROOT / "classes.tsv", class_rows,
              ["id", "symbol", "name", "promotion_symbol", "promotion_id", "sms_id", "slow_walking",
               "baseHP", "basePow", "baseSkl", "baseSpd", "baseDef", "baseRes", "baseCon", "baseMov",
               "maxHP", "maxPow", "maxSkl", "maxSpd", "maxDef", "maxRes", "maxCon",
               "growthHP", "growthPow", "growthSkl", "growthSpd", "growthDef", "growthRes", "growthLck", "growthCon",
               "promotionHP", "promotionPow", "promotionSkl", "promotionSpd", "promotionDef", "promotionRes", "promotionCon",
               "attributes", "movement_cost_table"])
    write_tsv(DATA_ROOT / "assets.tsv", asset_manifest(),
              ["family", "extension", "bytes", "relative_path"])

    (INCLUDE_ROOT / "fe8_unit_visuals.h").write_text(
        generate_visual_header(visual_rows, motion_rows)
    )
    (SOURCE_ROOT / "fe8_unit_visuals.cpp").write_text(
        generate_visual_cpp(visual_rows, class_visual_rows, motion_rows)
    )
    (INCLUDE_ROOT / "fe8_unit_data.h").write_text(generate_data_header())
    (SOURCE_ROOT / "fe8_unit_data.cpp").write_text(generate_data_cpp(character_rows, class_rows))

    print(f"Imported {len(visual_rows)} map-unit visuals, {len(character_rows)} character records, "
          f"{len(class_rows)} class records, and {len(asset_manifest())} raw graphics/audio files.")


if __name__ == "__main__":
    main()
