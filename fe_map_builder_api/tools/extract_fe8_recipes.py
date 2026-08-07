#!/usr/bin/env python3
"""Convert all FE8 chapter terrain grids into the sandbox integer vocabulary."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


# FE8's raw terrain ids (0x00–0x40) -> Strategic-Procedural-Generation ids.
# The sandbox keeps its simple outdoor vocabulary at 0–10, then appends the
# remaining FE8 terrain concepts at 11–64.
RAW_TO_SANDBOX = [
    11, 0, 1, 9, 12, 13, 14, 15, 16, 17, 7, 18, 2, 19, 20, 3,
    5, 4, 21, 6, 22, 23, 24, 10, 25, 26, 8, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
    47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    62, 63, 64,
]


def chapter_variant(map_data: dict, chapter: dict) -> dict:
    variants = map_data.get("variants", [])
    matches = [item for item in variants if item.get("chapter_index") == chapter["chapter_index"]]
    if not matches:
        matches = [item for item in variants if item.get("internal_name") == chapter["internal_name"]]
    if not matches:
        matches = [item for item in variants if item.get("terrain_rows")]
    if len(matches) != 1 or not matches[0].get("terrain_rows"):
        raise ValueError("Could not identify one terrain variant for " + chapter["assets"]["layout"])
    return matches[0]


def safe_name(value: str) -> str:
    return value if value else "unnamed"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-data", type=Path, required=True, help="Fe8-files/data directory.")
    parser.add_argument("--output", type=Path, required=True, help="recipes/fe8 destination.")
    args = parser.parse_args()

    chapters = json.loads((args.source_data / "chapters.json").read_text(encoding="utf-8"))["chapters"]
    recipes_dir = args.output / "chapters"
    recipes_dir.mkdir(parents=True, exist_ok=True)
    catalog = []

    for chapter in chapters:
        layout = chapter["assets"]["layout"]
        map_data = json.loads((args.source_data / "maps" / (layout + ".json")).read_text(encoding="utf-8"))
        variant = chapter_variant(map_data, chapter)
        raw_rows = variant["terrain_rows"]
        terrain_rows = []
        for row in raw_rows:
            if any(value < 0 or value >= len(RAW_TO_SANDBOX) for value in row):
                raise ValueError("Out-of-range raw terrain id in " + layout)
            terrain_rows.append([RAW_TO_SANDBOX[value] for value in row])

        filename = "{:02d}_{}_{}.json".format(chapter["chapter_index"], safe_name(chapter["internal_name"]), layout)
        recipe = {
            "schema_version": 1,
            "chapter_index": chapter["chapter_index"],
            "internal_name": chapter["internal_name"],
            "layout": layout,
            "width": len(terrain_rows[0]),
            "height": len(terrain_rows),
            "terrain_id_scheme": "strategic_procedural_generation_v1",
            "terrain_rows": terrain_rows,
        }
        (recipes_dir / filename).write_text(json.dumps(recipe, indent=2) + "\n", encoding="utf-8")
        catalog.append({
            "chapter_index": chapter["chapter_index"],
            "internal_name": chapter["internal_name"],
            "layout": layout,
            "width": recipe["width"],
            "height": recipe["height"],
            "path": "chapters/" + filename,
        })

    manifest = {
        "schema_version": 1,
        "description": "All 79 FE8 chapter-table maps converted into sandbox terrain IDs.",
        "source": "Fe8-files/data/chapters.json and maps/*.json",
        "raw_fe8_to_sandbox": RAW_TO_SANDBOX,
        "recipes": catalog,
    }
    (args.output / "catalog.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Wrote {} converted chapter recipes to {}".format(len(catalog), args.output))


if __name__ == "__main__":
    main()
