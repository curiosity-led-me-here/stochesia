#!/usr/bin/env python3
"""Build a standalone, read-only map-data package from this FE8 repository.

This script deliberately exports gameplay terrain ids, visual metatile ids, and
the raw tile-configuration entries separately. A raw entry becomes a metatile
after `entry >> 2`; that metatile becomes a gameplay terrain only when it is
interpreted through a particular tile configuration. Procedural generators can
therefore operate on terrain ids while still retaining a lossless bridge back
to FE8 art assets.
"""

from __future__ import annotations

import hashlib
import json
import re
import shutil
from collections import Counter, defaultdict
from pathlib import Path


PACKAGE_ROOT = Path(__file__).resolve().parent
REPO_ROOT = PACKAGE_ROOT.parent
OUTPUT = PACKAGE_ROOT / "data"
ASSETS = PACKAGE_ROOT / "assets"

TERRAIN_HEADER = REPO_ROOT / "include/constants/terrains.h"
TERRAIN_SOURCE = REPO_ROOT / "src/data_terrains.c"
CHAPTER_SOURCE = REPO_ROOT / "src/data/chapter_settings.json"
ASSET_TABLE = REPO_ROOT / "data/data_8B363C.s"
MAP_LAYOUTS = REPO_ROOT / "graphics/map/layout"
MAP_ASSETS = REPO_ROOT / "graphics/map"
MAP_METADATA = REPO_ROOT / "src/data/map"

COPY_SOURCES = {
    "graphics_map": MAP_ASSETS,
    "map_metadata": MAP_METADATA,
    "source/terrains.h": TERRAIN_HEADER,
    "source/data_terrains.c": TERRAIN_SOURCE,
    "source/chapter_settings.json": CHAPTER_SOURCE,
    "source/chapter_asset_table.s": ASSET_TABLE,
    "source/chapter_map_assets.s": REPO_ROOT / "data/const_data_chapter_maps.s",
    "source/map_renderer.c": REPO_ROOT / "src/bmmap.c",
    "source/map_renderer.h": REPO_ROOT / "include/bmmap.h",
    "source/map_changes.s": REPO_ROOT / "data/data_map_change.s",
}


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=False) + "\n", encoding="utf-8")


def parse_terrain_constants() -> tuple[list[dict[str, object]], dict[str, int]]:
    text = TERRAIN_HEADER.read_text(encoding="utf-8")
    matches = re.findall(r"^\s*(TERRAIN_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", text, re.MULTILINE)

    records = []
    by_name = {}
    for symbol, raw_id in matches:
        if symbol == "TERRAIN_COUNT":
            continue
        terrain_id = int(raw_id, 0)
        by_name[symbol] = terrain_id
        records.append({"id": terrain_id, "symbol": symbol, "name": symbol.removeprefix("TERRAIN_").lower()})
    return sorted(records, key=lambda record: int(record["id"])), by_name


def parse_terrain_tables(terrain_names: dict[str, int]) -> dict[str, dict[int, int]]:
    text = TERRAIN_SOURCE.read_text(encoding="utf-8")
    pattern = re.compile(r"CONST_DATA\s+s8\s+(\w+)\[\]\s*=\s*\{(.*?)\n\};", re.DOTALL)
    entry_pattern = re.compile(r"\[(TERRAIN_[A-Z0-9_]+)\]\s*=\s*(-?\d+)")
    tables: dict[str, dict[int, int]] = {}

    for table_name, body in pattern.findall(text):
        entries = {terrain_names[symbol]: int(value) for symbol, value in entry_pattern.findall(body) if symbol in terrain_names}
        if entries:
            tables[table_name] = entries
    return tables


def terrain_tags(symbol: str) -> list[str]:
    """Convenience tags only; source-of-truth mechanics remain the raw tables."""
    tags: list[str] = []
    if "VILLAGE" in symbol or symbol in {"TERRAIN_HOUSE", "TERRAIN_CHURCH", "TERRAIN_INN"}:
        tags.append("settlement")
    if "CHEST" in symbol:
        tags.append("chest")
    if "BALLISTA" in symbol:
        tags.append("ballista")
    if symbol in {"TERRAIN_FORT", "TERRAIN_THRONE", "TERRAIN_GATE_CASTLE"}:
        tags.append("defensive_position")
    if symbol in {"TERRAIN_DOOR", "TERRAIN_WALL_REGULAR", "TERRAIN_WALL_DAMAGED", "TERRAIN_SNAG"}:
        tags.append("destructible_or_openable")
    if "BRIDGE" in symbol:
        tags.append("bridge")
    if symbol in {"TERRAIN_RIVER", "TERRAIN_SEA", "TERRAIN_LAKE", "TERRAIN_WATER", "TERRAIN_DEEPS"}:
        tags.append("water")
    return tags


def build_terrain_catalog() -> dict[str, object]:
    records, terrain_names = parse_terrain_constants()
    tables = parse_terrain_tables(terrain_names)

    movement_tables = {name: table for name, table in tables.items() if name.startswith("TerrainTable_MovCost_")}
    combat_tables = {name: table for name, table in tables.items() if name.startswith("TerrainTable_Avo_") or name.startswith("TerrainTable_Def_") or name.startswith("TerrainTable_Res_")}
    healing_tables = {name: table for name, table in tables.items() if name in {"TerrainTable_HealAmount", "TerrainTable_HealsStatus"}}

    for record in records:
        terrain_id = int(record["id"])
        record["tags"] = terrain_tags(str(record["symbol"]))
        record["movement_costs"] = {name: table.get(terrain_id) for name, table in movement_tables.items()}
        record["combat_modifiers"] = {name: table.get(terrain_id) for name, table in combat_tables.items()}
        record["healing"] = {name: table.get(terrain_id) for name, table in healing_tables.items()}

    return {
        "schema_version": 1,
        "description": "Terrain vocabulary. Negative movement cost means the movement type cannot enter the terrain.",
        "source": "assets/source/terrains.h and assets/source/data_terrains.c",
        "terrain_count": len(records),
        "movement_profiles": list(movement_tables),
        "combat_tables": list(combat_tables),
        "healing_tables": list(healing_tables),
        "terrains": records,
        "raw_tables": {name: {str(key): value for key, value in table.items()} for name, table in tables.items()},
    }


def parse_asset_table() -> list[str]:
    symbols = []
    for line in ASSET_TABLE.read_text(encoding="utf-8").splitlines():
        match = re.match(r"\s*\.word\s+([A-Za-z_][A-Za-z0-9_]*|0)\s*$", line)
        if match:
            symbols.append(match.group(1))
    return symbols


def tile_configurations() -> tuple[dict[str, list[int]], list[dict[str, object]]]:
    terrain_maps: dict[str, list[int]] = {}
    records = []
    for path in sorted(MAP_ASSETS.glob("*TileConfiguration*.bin"), key=lambda item: item.stem):
        raw = path.read_bytes()
        if len(raw) != 0x2400:
            raise ValueError(f"Unexpected tile configuration size for {path}: {len(raw)}")
        terrain_lookup = list(raw[0x2000:])
        symbol = path.stem
        terrain_maps[symbol] = terrain_lookup
        counts = Counter(terrain_lookup)
        records.append({
            "id": symbol,
            "source_asset": f"assets/graphics_map/{path.name}",
            "metatile_count": len(terrain_lookup),
            "terrain_by_metatile_id": terrain_lookup,
            "terrain_counts": {str(terrain_id): count for terrain_id, count in sorted(counts.items())},
        })
    return terrain_maps, records


def read_layout(layout_json: Path) -> tuple[dict[str, object], list[list[int]]]:
    metadata = json.loads(layout_json.read_text(encoding="utf-8"))
    mar_path = layout_json.with_suffix(".mar")
    raw = mar_path.read_bytes()
    width, height = int(metadata["width"]), int(metadata["height"])
    expected_size = width * height * 2
    if len(raw) != expected_size:
        raise ValueError(f"{mar_path} has {len(raw)} bytes, expected {expected_size}")

    # FEBuilder's .mar encoding stores a value with three low-order display bits.
    # The game's map renderer receives the remaining 0..0xFFF tile-config entry.
    entries = [int.from_bytes(raw[offset:offset + 2], "little") >> 3 for offset in range(0, len(raw), 2)]
    if any(entry >= 0x1000 for entry in entries):
        raise ValueError(f"{mar_path} contains an out-of-range tile configuration entry")
    return metadata, [entries[row * width:(row + 1) * width] for row in range(height)]


def build_chapter_data(asset_symbols: list[str]) -> tuple[list[dict[str, object]], dict[str, list[dict[str, object]]]]:
    source = json.loads(CHAPTER_SOURCE.read_text(encoding="utf-8"))
    chapters = []
    variants: dict[str, list[dict[str, object]]] = defaultdict(list)

    def lookup(asset_id: object) -> str | None:
        if not isinstance(asset_id, int) or asset_id < 0 or asset_id >= len(asset_symbols):
            return None
        symbol = asset_symbols[asset_id]
        return None if symbol == "0" else symbol

    for index, chapter in enumerate(source["chapters"]):
        map_data = chapter.get("map", {})
        entry = {
            "chapter_index": index,
            "internal_name": chapter.get("internalName"),
            "initial_weather": chapter.get("initialWeather"),
            "initial_fog_level": chapter.get("initialFogLevel"),
            "goal": chapter.get("goal", {}),
            "map": map_data,
            "assets": {
                "object_1": lookup(map_data.get("obj1Id")),
                "object_2": lookup(map_data.get("obj2Id")),
                "palette": lookup(map_data.get("paletteId")),
                "tile_configuration": lookup(map_data.get("tileConfigId")),
                "layout": lookup(map_data.get("mainLayerId")),
                "object_animation": lookup(map_data.get("objAnimId")),
                "palette_animation": lookup(map_data.get("paletteAnimId")),
                "map_changes": lookup(map_data.get("changeLayerId")),
            },
        }
        chapters.append(entry)
        if entry["assets"]["layout"]:
            variants[str(entry["assets"]["layout"])].append({
                "chapter_index": index,
                "internal_name": entry["internal_name"],
                "tile_configuration": entry["assets"]["tile_configuration"],
                "palette": entry["assets"]["palette"],
                "object_1": entry["assets"]["object_1"],
                "object_2": entry["assets"]["object_2"],
                "map_changes": entry["assets"]["map_changes"],
            })
    return chapters, variants


def copy_sources() -> None:
    for destination, source in COPY_SOURCES.items():
        target = ASSETS / destination
        if source.is_dir():
            shutil.copytree(source, target, dirs_exist_ok=True)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)


def build_asset_manifest() -> list[dict[str, object]]:
    files = []
    for path in sorted(ASSETS.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(PACKAGE_ROOT).as_posix()
        files.append({
            "path": relative,
            "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        })
    return files


def main() -> None:
    copy_sources()
    terrain = build_terrain_catalog()
    asset_symbols = parse_asset_table()
    terrain_maps, tilesets = tile_configurations()
    chapters, variants_by_layout = build_chapter_data(asset_symbols)

    layout_index = []
    maps_dir = OUTPUT / "maps"
    if maps_dir.exists():
        shutil.rmtree(maps_dir)
    maps_dir.mkdir(parents=True, exist_ok=True)

    for layout_json in sorted(MAP_LAYOUTS.glob("*.json")):
        metadata, entry_rows = read_layout(layout_json)
        layout_id = str(metadata["id"])
        variants = variants_by_layout.get(layout_id, [])
        metatile_rows = [[entry >> 2 for entry in row] for row in entry_rows]
        exported_variants = []
        for variant in variants:
            tileset_id = variant.get("tile_configuration")
            terrain_lookup = terrain_maps.get(str(tileset_id))
            terrain_rows = None
            if terrain_lookup is not None:
                terrain_rows = [[terrain_lookup[metatile] for metatile in row] for row in metatile_rows]
            exported_variants.append({**variant, "terrain_rows": terrain_rows})

        exported_map = {
            "schema_version": 1,
            "id": layout_id,
            "width": metadata["width"],
            "height": metadata["height"],
            "source_assets": {
                "layout_metadata": f"assets/graphics_map/layout/{layout_json.name}",
                "raw_layout": f"assets/graphics_map/layout/{layout_json.with_suffix('.mar').name}",
            },
            "tile_configuration_entry_rows": entry_rows,
            "metatile_rows": metatile_rows,
            "variants": exported_variants,
        }
        write_json(maps_dir / f"{layout_id}.json", exported_map)
        layout_index.append({
            "id": layout_id,
            "width": metadata["width"],
            "height": metadata["height"],
            "chapter_variants": len(exported_variants),
            "endpoint": f"/api/maps/{layout_id}",
        })

    write_json(OUTPUT / "terrains.json", terrain)
    write_json(OUTPUT / "tilesets.json", {"schema_version": 1, "tilesets": tilesets})
    write_json(OUTPUT / "chapters.json", {"schema_version": 1, "chapters": chapters})
    write_json(OUTPUT / "layouts.json", {"schema_version": 1, "layouts": layout_index})
    write_json(OUTPUT / "assets.json", {"schema_version": 1, "assets": build_asset_manifest()})
    write_json(OUTPUT / "catalog.json", {
        "schema_version": 1,
        "description": "Standalone FE8 map research catalog generated from local source assets.",
        "counts": {"terrains": terrain["terrain_count"], "tilesets": len(tilesets), "layouts": len(layout_index), "chapters": len(chapters)},
        "endpoints": {
            "health": "/api/health",
            "catalog": "/api/catalog",
            "terrains": "/api/terrains",
            "tilesets": "/api/tilesets",
            "chapters": "/api/chapters",
            "layouts": "/api/layouts",
            "map_by_id": "/api/maps/{layout_id}",
            "asset_manifest": "/api/assets",
            "copied_source_assets": "/assets/",
        },
    })
    print(f"Built {OUTPUT.relative_to(REPO_ROOT)}: {len(layout_index)} layouts, {len(tilesets)} tilesets, {terrain['terrain_count']} terrain ids.")


if __name__ == "__main__":
    main()
