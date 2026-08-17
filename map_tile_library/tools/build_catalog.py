#!/usr/bin/env python3
"""Build the FE8 chapter-theme/class/subclass/orientation tile library.

Theme = one original FE8 source map (chapter, route chapter, skirmish, tower,
ruin, or cutscene). Class = the Tile Map Editor visual family. Subclass = one
same-class adjacency-connected tileset inside that source map. Orientation =
one raw 16x16 tile inside the subclass.

The editor records only adjacency, not semantic labels such as "northwest
mountain corner". Therefore Orientation is an exact stable variant code, while
data/adjacency.tsv contains the source-provided cardinal compatibility graph for
any later orientation-inference algorithm.
"""

from __future__ import annotations

import json
import re
import shutil
from collections import defaultdict
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "fe_map_builder_api"
SOURCE_REFERENCES = SOURCE_ROOT / "assets" / "tileReferences.json"
SOURCE_IMAGES = SOURCE_ROOT / "assets" / "images"
LIBRARY_ROOT = PROJECT_ROOT / "map_tile_library"
ASSETS_ROOT = LIBRARY_ROOT / "assets"
DATA_ROOT = LIBRARY_ROOT / "data"
INCLUDE_ROOT = LIBRARY_ROOT / "include"


# Stable visual Class IDs. They are intentionally separate from the sandbox's
# gameplay Terrain IDs.
CLASS_ORDER = [
    "PLAIN", "FOREST", "MOUNTAIN", "PEAK", "VALLEY", "CLIFF",
    "ROAD", "PLAIN-ROAD", "DESERT", "SAND", "RIVER", "WATER",
    "LAKE", "SEA", "BRIDGE", "FORT", "VILLAGE", "HOUSE", "ARMORY",
    "VENDOR", "ARENA", "INN", "WALL", "WALL2", "FLOOR", "STAIRS",
    "DOOR", "GATE", "FENCE", "FENCE-WALL", "FENCE-BRACE",
    "WALL-BRACE", "BRACE-WALL", "PILLAR", "THRONE", "CHEST", "ROOF",
    "RUINS", "THICKET", "SNAG", "BARREL", "BONE", "DARK", "DECK",
    "GUNNELS", "MAST", "BRACE", "FLAT", "DASHDASH", "UNDEFINED",
    "LAKE-CLIFF", "VILLAGE-HOUSE",
]
CLASS_ID = {name: index + 1 for index, name in enumerate(CLASS_ORDER)}
DIRECTIONS = ("north", "east", "south", "west")


def natural_key(value: str) -> list[object]:
    return [int(piece) if piece.isdigit() else piece.lower()
            for piece in re.split(r"(\d+)", value)]


def theme_sort_key(source: str) -> tuple[int, list[object]]:
    # Keep story chapters first; everything else remains a source-map theme so
    # assets exclusive to skirmishes, towers, ruins, and cutscenes are kept.
    tail = source.removeprefix("Fire Emblem 8/")
    family = tail.split("/", 1)[0]
    order = {
        "Chapters": 0,
        "Cutscenes": 1,
        "Tower of Valni": 2,
        "Lagdou Ruins": 3,
        "Skirmishes": 4,
    }.get(family, 9)
    return order, natural_key(tail)


def clean_name(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")


def enum_name(source: str) -> str:
    return "THEME_" + re.sub(r"[^A-Z0-9]+", "_", source.upper()).strip("_").replace(
        "FIRE_EMBLEM_8_", ""
    )


def components(entries: list[dict], by_hash: dict[str, dict]) -> list[list[str]]:
    """Find undirected same-class components from editor cardinal links."""
    hashes = {entry["tileHash"] for entry in entries}
    graph: dict[str, set[str]] = {tile_hash: set() for tile_hash in hashes}
    for tile_hash in hashes:
        for direction in DIRECTIONS:
            for neighbor in by_hash[tile_hash].get(direction, []):
                if neighbor in hashes:
                    graph[tile_hash].add(neighbor)
                    graph[neighbor].add(tile_hash)

    seen: set[str] = set()
    out: list[list[str]] = []
    for start in sorted(hashes):
        if start in seen:
            continue
        stack = [start]
        seen.add(start)
        component: list[str] = []
        while stack:
            current = stack.pop()
            component.append(current)
            for neighbor in graph[current]:
                if neighbor not in seen:
                    seen.add(neighbor)
                    stack.append(neighbor)
        out.append(sorted(component))
    return sorted(out, key=lambda component: component[0])


def write_theme_header(themes: list[str]) -> None:
    lines = [
        "#pragma once",
        "",
        "namespace fe_tiles",
        "{",
        "using ThemeId = int;",
        "",
        "// Generated from data/themes.tsv. Theme means one FE8 source map.",
    ]
    for theme_id, source in enumerate(themes, start=1):
        lines.append(f"constexpr ThemeId {enum_name(source)} = {theme_id};")
    lines.extend(["}", ""])
    (INCLUDE_ROOT / "fe8_theme_ids.h").write_text("\n".join(lines))


def main() -> None:
    references: list[dict] = json.loads(SOURCE_REFERENCES.read_text())
    by_hash = {entry["tileHash"]: entry for entry in references}
    unknown = sorted({entry["group"] for entry in references} - set(CLASS_ID))
    if unknown:
        raise RuntimeError(f"Unmapped FE8 visual classes: {', '.join(unknown)}")

    themes = sorted(
        {
            source
            for entry in references
            for source in entry["originFilePaths"]
            if source.startswith("Fire Emblem 8/")
        },
        key=theme_sort_key,
    )
    theme_id_by_source = {source: index + 1 for index, source in enumerate(themes)}

    # A hash may have several original source maps. It belongs to each matching
    # chapter-theme once, not to FE6/FE7 source maps retained in editor history.
    by_theme_class: dict[tuple[str, str], list[dict]] = defaultdict(list)
    for entry in references:
        for source in entry["originFilePaths"]:
            if source in theme_id_by_source:
                by_theme_class[(source, entry["group"])].append(entry)

    DATA_ROOT.mkdir(parents=True, exist_ok=True)
    INCLUDE_ROOT.mkdir(parents=True, exist_ok=True)
    if ASSETS_ROOT.exists():
        shutil.rmtree(ASSETS_ROOT)
    ASSETS_ROOT.mkdir(parents=True)

    themes_tsv = ["theme_id\ttheme_source\ttheme_folder"]
    for source in themes:
        theme_id = theme_id_by_source[source]
        themes_tsv.append(f"{theme_id}\t{source}\ttheme_{theme_id:03d}_{clean_name(source.removeprefix('Fire Emblem 8/'))}")

    classes_tsv = ["class_id\tclass_name"]
    for class_name in CLASS_ORDER:
        classes_tsv.append(f"{CLASS_ID[class_name]}\t{class_name}")

    catalogue_tsv = [
        "theme_id\ttheme_source\tclass_id\tclass_name\tsubclass_id\torientation_id\t"
        "tile_hash\trelative_png"
    ]
    adjacency_tsv = [
        "theme_id\tclass_id\tsubclass_id\torientation_id\tnorth\teast\tsouth\twest"
    ]

    copied = 0
    for (source, class_name), entries in sorted(
        by_theme_class.items(),
        key=lambda item: (theme_id_by_source[item[0][0]], CLASS_ID[item[0][1]]),
    ):
        theme_id = theme_id_by_source[source]
        class_id = CLASS_ID[class_name]
        folder = (
            ASSETS_ROOT /
            f"theme_{theme_id:03d}_{clean_name(source.removeprefix('Fire Emblem 8/'))}" /
            f"class_{class_id:02d}_{clean_name(class_name)}"
        )

        # No repeated hash inside a source/class pair.
        unique_entries = {entry["tileHash"]: entry for entry in entries}
        orientation_address: dict[str, tuple[int, int]] = {}
        for subclass_id, component in enumerate(components(list(unique_entries.values()), by_hash)):
            subclass_folder = folder / f"subclass_{subclass_id:03d}"
            subclass_folder.mkdir(parents=True, exist_ok=True)
            for orientation_id, tile_hash in enumerate(component):
                orientation_address[tile_hash] = (subclass_id, orientation_id)
                source_png = SOURCE_IMAGES / class_name / f"{tile_hash}.png"
                if not source_png.is_file():
                    raise RuntimeError(f"Missing referenced PNG: {source_png}")
                target = subclass_folder / f"orientation_{orientation_id:03d}.png"
                shutil.copy2(source_png, target)
                catalogue_tsv.append(
                    f"{theme_id}\t{source}\t{class_id}\t{class_name}\t{subclass_id}\t"
                    f"{orientation_id}\t{tile_hash}\t{target.relative_to(LIBRARY_ROOT).as_posix()}"
                )
                copied += 1

        for tile_hash, (subclass_id, orientation_id) in sorted(orientation_address.items()):
            neighbors: list[str] = []
            for direction in DIRECTIONS:
                resolved = []
                for neighbor in by_hash[tile_hash].get(direction, []):
                    if neighbor in orientation_address:
                        n_subclass, n_orientation = orientation_address[neighbor]
                        resolved.append(f"{n_subclass}:{n_orientation}")
                neighbors.append(";".join(sorted(resolved, key=natural_key)))
            adjacency_tsv.append(
                f"{theme_id}\t{class_id}\t{subclass_id}\t{orientation_id}\t" +
                "\t".join(neighbors)
            )

    (DATA_ROOT / "themes.tsv").write_text("\n".join(themes_tsv) + "\n")
    (DATA_ROOT / "classes.tsv").write_text("\n".join(classes_tsv) + "\n")
    (DATA_ROOT / "catalogue.tsv").write_text("\n".join(catalogue_tsv) + "\n")
    (DATA_ROOT / "adjacency.tsv").write_text("\n".join(adjacency_tsv) + "\n")
    write_theme_header(themes)

    print(f"Copied {copied} chapter-theme tile references into {ASSETS_ROOT}")
    print(f"Wrote {len(themes)} FE8 source-map themes and the catalogue/adjacency TSV files")


if __name__ == "__main__":
    main()
