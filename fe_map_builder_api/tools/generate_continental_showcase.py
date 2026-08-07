#!/usr/bin/env python3
"""Build a large FE8-inspired campaign-map recipe with deliberate biomes."""

from __future__ import annotations

import json
from pathlib import Path


W, H = 96, 72
PLAIN, ROAD, WOODS, DESERT, MOUNTAIN, RIVER, BRIDGE, FORT, WALL, VILLAGE, FLOOR = range(11)
SAND, PEAK, SEA, GATE, RUINS, CLIFF, THRONE, SPECIAL_2E = 20, 21, 23, 35, 37, 38, 31, 46


def line(grid: list[list[int]], x0: int, y0: int, x1: int, y1: int, value: int, width: int = 1) -> None:
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    error = dx + dy
    while True:
        for oy in range(-(width // 2), width // 2 + 1):
            for ox in range(-(width // 2), width // 2 + 1):
                x, y = x0 + ox, y0 + oy
                if 0 <= x < W and 0 <= y < H:
                    grid[y][x] = value
        if x0 == x1 and y0 == y1:
            return
        twice = 2 * error
        if twice >= dy:
            error += dy
            x0 += sx
        if twice <= dx:
            error += dx
            y0 += sy


def village(grid: list[list[int]], x: int, y: int) -> None:
    """Place the FE8 3x3 village prefab; x/y are its upper-left cell."""
    pattern = ((SPECIAL_2E, SPECIAL_2E, SPECIAL_2E),
               (SPECIAL_2E, SPECIAL_2E, SPECIAL_2E),
               (SPECIAL_2E, VILLAGE, SPECIAL_2E))
    for dy in range(3):
        for dx in range(3):
            grid[y + dy][x + dx] = pattern[dy][dx]


def main() -> None:
    grid = [[PLAIN for _ in range(W)] for _ in range(H)]

    # South-west ocean, surrounded by a sand shoreline rather than a hard
    # grass-to-water seam.
    for y in range(47, H):
        for x in range(0, 37):
            distance = ((x - 16) / 25) ** 2 + ((y - 64) / 19) ** 2
            if distance < 1.0:
                grid[y][x] = SEA
            elif distance < 1.26:
                grid[y][x] = SAND

    # Eastern desert with a broad sand transition band.
    for y in range(24, 65):
        for x in range(67, W):
            distance = ((x - 89) / 24) ** 2 + ((y - 45) / 27) ** 2
            if distance < 1.0:
                grid[y][x] = DESERT
            elif distance < 1.22:
                grid[y][x] = SAND

    # Northern mountains descend through cliffs and sparse foothill forest.
    for y in range(0, 25):
        for x in range(37, 70):
            ridge = ((x - 54) / 22) ** 2 + ((y - 3) / 20) ** 2
            if ridge < 0.72:
                grid[y][x] = MOUNTAIN
            elif ridge < 1.08:
                grid[y][x] = CLIFF if (x + y) % 4 == 0 else MOUNTAIN

    # This showcase keeps the highland as one continuous MOUNTAIN mass. Peaks
    # require their own compact summit prefab, so they are deliberately absent
    # until that prefab library is implemented.

    # Forest realm at the north-west, with deliberate open clearings.
    for y in range(3, 35):
        for x in range(2, 34):
            cluster = ((x - 15) / 18) ** 2 + ((y - 17) / 18) ** 2
            if cluster < 0.90 and (3 * x + 5 * y) % 7 not in (0, 1):
                grid[y][x] = WOODS

    # A mountain-fed river, then bridges and a road corridor that connect the
    # distinct regions. Roads cross terrain; bridges replace river cells.
    line(grid, 54, 7, 50, 28, RIVER, 2)
    line(grid, 50, 28, 42, 43, RIVER, 2)
    line(grid, 42, 43, 35, 57, RIVER, 2)
    line(grid, 35, 57, 29, 62, RIVER, 2)

    line(grid, 8, 28, 46, 28, ROAD)
    line(grid, 46, 28, 54, 39, ROAD)
    line(grid, 54, 39, 74, 39, ROAD)
    line(grid, 54, 39, 54, 62, ROAD)
    line(grid, 54, 62, 30, 62, ROAD)
    line(grid, 74, 39, 83, 50, ROAD)
    for x, y in ((50, 28), (42, 43), (35, 57)):
        for ox in (-1, 0, 1):
            grid[y][x + ox] = BRIDGE

    # Village prefabs are set after roads so their interaction cells stay
    # clear and the renderer can recognize each complete building.
    village(grid, 9, 20)
    village(grid, 25, 31)
    village(grid, 76, 48)
    village(grid, 51, 55)

    # Walled central keep: outdoor road -> gate -> floor courtyard -> throne.
    left, top, right, bottom = 39, 31, 55, 44
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            grid[y][x] = WALL if x in (left, right) or y in (top, bottom) else FLOOR
    grid[bottom][47] = GATE
    grid[bottom][48] = GATE
    grid[top + 2][47] = THRONE
    grid[top + 2][48] = THRONE
    for x in range(47, 49):
        grid[bottom + 1][x] = ROAD

    # Desert ruins and military landmarks give the east a tactical purpose.
    for x, y in ((79, 31), (84, 34), (89, 29), (87, 54), (91, 57)):
        grid[y][x] = RUINS
    for x, y in ((18, 34), (60, 27), (73, 40), (75, 56)):
        grid[y][x] = FORT

    recipe = {
        "schema_version": 1,
        "id": "continental_campaign_showcase",
        "width": W,
        "height": H,
        "terrain_id_scheme": "strategic_procedural_generation_v1",
        "description": "Large FE8-inspired combined-biome map: forest, plains, mountains, sea, desert, villages, bridges, ruins, and a walled keep.",
        "terrain_rows": grid,
    }
    output = Path(__file__).resolve().parent.parent / "recipes" / "showcase" / "continental_campaign.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(recipe, indent=2) + "\n", encoding="utf-8")
    rows = ",\n".join("    {" + ", ".join(str(value) for value in row) + "}" for row in grid)
    cpp_output = output.with_suffix(".h")
    cpp_output.write_text(
        "#pragma once\n\n"
        "#include <vector>\n\n"
        "namespace showcase_maps {\n\n"
        "// FE8-inspired 96x72 campaign-map recipe. Rows are y; entries are x.\n"
        "static const std::vector<std::vector<int>> continental_campaign = {\n"
        + rows + "\n"
        "};\n\n"
        "} // namespace showcase_maps\n",
        encoding="utf-8",
    )
    print("Wrote {}x{} showcase recipe to {}".format(W, H, output))


if __name__ == "__main__":
    main()
