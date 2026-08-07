#!/usr/bin/env python3
"""Compile an integer terrain grid into a Tile Map Editor tile-hash map and PNG.

No third-party packages are required. Tile choices are constrained by the
editor's observed north/east/south/west neighbour relationships.
"""

from __future__ import annotations

import argparse
import json
import random
import struct
import sys
import zlib
from collections import deque
from pathlib import Path


class BuildError(Exception):
    pass


DIRS = {"north": (0, -1), "east": (1, 0), "south": (0, 1), "west": (-1, 0)}
OPPOSITE = {"north": "south", "east": "west", "south": "north", "west": "east"}


def load_terrain_grid(path: Path) -> list[list[int]]:
    value = json.loads(path.read_text(encoding="utf-8"))
    grid = value.get("terrain_rows") if isinstance(value, dict) else value
    if not isinstance(grid, list) or not grid or not all(isinstance(row, list) and row for row in grid):
        raise BuildError("Input must be a non-empty JSON grid or contain 'terrain_rows'.")
    width = len(grid[0])
    if not all(len(row) == width and all(isinstance(tile, int) for tile in row) for row in grid):
        raise BuildError("Terrain grid must be rectangular and contain only integers.")
    return grid


def derive_visual_plan(terrain: list[list[int]], profiles: dict, theme: str = "auto") -> dict:
    """Convert gameplay terrain into orientation-aware visual roles.

    The role grid is deliberately separate from the input terrain grid. A
    gameplay MOUNTAIN is still just one integer, but its visual role becomes
    ``highland_0f_0`` or ``highland_03_8`` depending on its neighbours.

    The first hexadecimal field is a cardinal-edge mask; the second is a
    diagonal-corner mask.  This makes a one-cell gameplay terrain language
    sufficient for an art compiler that needs to distinguish an outer corner
    from an inner corner.
    """
    height, width = len(terrain), len(terrain[0])
    configured_families = profiles.get("terrain_families", {})
    configured_profiles = profiles.get("terrain_profiles", {})
    defined_profiles = profiles.get("profiles", {})
    if theme != "auto" and theme not in defined_profiles:
        choices = ", ".join(["auto"] + sorted(defined_profiles))
        raise BuildError("Unknown visual theme '{}'. Available themes: {}.".format(theme, choices))

    family_rows: list[list[str]] = []
    profile_rows: list[list[str]] = []
    for row in terrain:
        family_rows.append([configured_families.get(str(value), "terrain_" + str(value)) for value in row])
        # This is a semantic fallback profile, *not* the map palette. The
        # palette is selected globally by resolve_theme below. Keeping the two
        # concepts separate is what prevents a mountain from switching the
        # entire map to a different visual language.
        profile_rows.append([configured_profiles.get(str(value), "temperate") for value in row])

    # Cardinal bits are N=1, E=2, S=4, W=8.  Diagonal bits are NE=1, SE=2,
    # SW=4, NW=8.  The current FE8 reference metadata contains direct
    # cardinal relationships, so cardinal roles drive tile choice today;
    # diagonal roles are emitted as first-class data for the corner/prefab
    # catalog and prevent the gameplay API from ever needing angle IDs.
    cardinal_neighbours = ((1, 0, -1), (2, 1, 0), (4, 0, 1), (8, -1, 0))
    diagonal_neighbours = ((1, 1, -1), (2, 1, 1), (4, -1, 1), (8, -1, -1))
    role_rows: list[list[str]] = []
    for y in range(height):
        row: list[str] = []
        for x in range(width):
            family = family_rows[y][x]
            cardinal_mask = 0
            diagonal_mask = 0
            for bit, dx, dy in cardinal_neighbours:
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and 0 <= ny < height and family_rows[ny][nx] == family:
                    cardinal_mask |= bit
            for bit, dx, dy in diagonal_neighbours:
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and 0 <= ny < height and family_rows[ny][nx] == family:
                    diagonal_mask |= bit
            row.append("{}_{:02x}_{:x}".format(family, cardinal_mask, diagonal_mask))
        role_rows.append(row)

    return {"role_rows": role_rows, "family_rows": family_rows, "profile_rows": profile_rows}


def resolve_theme(
    domains: list[list[set[str]]], references: dict[str, dict], profiles: dict, requested_theme: str
) -> tuple[str, dict[str, int]]:
    """Select one FE-style palette pack for the entire map.

    A theme pack is a small set of FE8 source maps that share an intended
    visual language.  ``auto`` selects the pack that can express the greatest
    number of input cells. An explicit theme is never silently replaced.
    """
    theme_definitions = profiles.get("profiles", {})
    if requested_theme != "auto" and requested_theme not in theme_definitions:
        choices = ", ".join(["auto"] + sorted(theme_definitions))
        raise BuildError("Unknown visual theme '{}'. Available themes: {}.".format(requested_theme, choices))

    scores: dict[str, int] = {}
    for name, definition in theme_definitions.items():
        origins = set(definition.get("origins", []))
        scores[name] = sum(
            1
            for row in domains
            for candidates in row
            if any(origins.intersection(references[tile_hash]["originFilePaths"]) for tile_hash in candidates)
        )
    if requested_theme != "auto":
        return requested_theme, scores
    if not scores:
        raise BuildError("No visual themes are configured.")
    return max(scores, key=lambda name: (scores[name], name)), scores


def constrain_to_theme(
    domains: list[list[set[str]]], references: dict[str, dict], profiles: dict, theme: str
) -> tuple[list[list[set[str]]], list[list[bool]]]:
    """Restrict all cells to the selected theme pack before any tile solving.

    Some special FE8 objects occur in only one source map. Those individual
    cells retain their raw candidates as a documented fallback; ordinary
    terrain never borrows randomly from another palette.
    """
    origins = set(profiles.get("profiles", {}).get(theme, {}).get("origins", []))
    constrained: list[list[set[str]]] = []
    fallback_rows: list[list[bool]] = []
    for row in domains:
        constrained_row: list[set[str]] = []
        fallback_row: list[bool] = []
        for candidates in row:
            themed = {
                tile_hash
                for tile_hash in candidates
                if origins.intersection(references[tile_hash]["originFilePaths"])
            }
            constrained_row.append(themed if themed else set(candidates))
            fallback_row.append(not bool(themed))
        constrained.append(constrained_row)
        fallback_rows.append(fallback_row)
    return constrained, fallback_rows


def cardinal_role_mask(role: str) -> int:
    """Extract the cardinal mask from ``family_cc_dd`` visual-role text."""
    try:
        return int(role.rsplit("_", 2)[1], 16)
    except (IndexError, ValueError) as error:
        raise BuildError("Invalid visual role: {}".format(role)) from error


def oriented_candidates(
    raw_candidates: set[str],
    target_mask: int,
    family_candidates: set[str],
    references: dict[str, dict],
) -> set[str]:
    """Return sprite candidates whose observed FE8 edge role fits a cell.

    A sprite's source-map role is reconstructed from the tile editor's actual
    neighbour graph.  For example, a mountain sprite that was observed next
    to highland terrain east and south, but not north/west, has role ``0x06``.
    Selecting an exact matching role is the part that turns an integer blob
    into correctly angled terrain rather than a random pile of mountain art.

    Exact matches are preferred. If an original chapter never used a requested
    shape, return the nearest available role instead of rejecting the map.
    """
    if not raw_candidates:
        return set()

    def candidate_mask(tile_hash: str) -> int:
        mask = 0
        for bit, direction in zip((1, 2, 4, 8), DIRS):
            if any(neighbour in family_candidates for neighbour in references[tile_hash][direction]):
                mask |= bit
        return mask

    masks = {tile_hash: candidate_mask(tile_hash) for tile_hash in raw_candidates}
    exact = {tile_hash for tile_hash, mask in masks.items() if mask == target_mask}
    if exact:
        return exact

    # Hamming distance is a clear, deterministic fallback: it preserves the
    # most important facing edges when a requested configuration is absent.
    distance = min((mask ^ target_mask).bit_count() for mask in masks.values())
    return {
        tile_hash
        for tile_hash, mask in masks.items()
        if (mask ^ target_mask).bit_count() == distance
    }


def build_visual_chunks(
    family_rows: list[list[str]], profile_rows: list[list[str]]
) -> tuple[list[list[int]], list[dict]]:
    """Split connected terrain families into visual-material chunks.

    Geometry and material are separate concerns.  A mountain's shape is
    determined by the visual role, while its *look* must remain consistent
    across the whole mountain range.  This pass labels 4-connected regions so
    the next pass can lock each region to one coherent FE8 source palette.
    """
    height, width = len(family_rows), len(family_rows[0])
    chunk_rows = [[-1 for _ in range(width)] for _ in range(height)]
    chunks: list[dict] = []

    for start_y in range(height):
        for start_x in range(width):
            if chunk_rows[start_y][start_x] != -1:
                continue
            family = family_rows[start_y][start_x]
            chunk_id = len(chunks)
            queue = deque([(start_x, start_y)])
            chunk_rows[start_y][start_x] = chunk_id
            cells: list[tuple[int, int]] = []
            min_x = max_x = start_x
            min_y = max_y = start_y
            while queue:
                x, y = queue.popleft()
                cells.append((x, y))
                min_x, max_x = min(min_x, x), max(max_x, x)
                min_y, max_y = min(min_y, y), max(max_y, y)
                for dx, dy in DIRS.values():
                    nx, ny = x + dx, y + dy
                    if (
                        0 <= nx < width
                        and 0 <= ny < height
                        and chunk_rows[ny][nx] == -1
                        and family_rows[ny][nx] == family
                    ):
                        chunk_rows[ny][nx] = chunk_id
                        queue.append((nx, ny))
            chunks.append(
                {
                    "id": chunk_id,
                    "family": family,
                    "profile": profile_rows[start_y][start_x],
                    "cells": cells,
                    "bounds": [min_x, min_y, max_x, max_y],
                }
            )
    return chunk_rows, chunks


def choose_map_material_origin(
    domains: list[list[set[str]]],
    references: dict[str, dict],
    profiles: dict,
    theme: str,
) -> str | None:
    """Choose one source palette that covers as much of the whole map as possible.

    This is intentionally before per-region locking. A map may contain plains,
    forest, rivers, and mountains, but they should read as one world rather
    than four unrelated screenshots pasted together.  A named theme restricts
    this choice to its source list; ``auto`` chooses the most broadly usable
    FE8 origin for the supplied terrain layout.
    """
    allowed_origins = None
    if theme != "auto":
        allowed_origins = set(profiles.get("profiles", {}).get(theme, {}).get("origins", []))

    coverage: dict[str, int] = {}
    for row in domains:
        for candidates in row:
            origins_here = {
                origin
                for tile_hash in candidates
                for origin in references[tile_hash]["originFilePaths"]
                if allowed_origins is None or origin in allowed_origins
            }
            for origin in origins_here:
                coverage[origin] = coverage.get(origin, 0) + 1
    if not coverage:
        return None
    return max(coverage, key=lambda origin: (coverage[origin], origin))


def choose_family_material_origins(
    domains: list[list[set[str]]],
    family_rows: list[list[str]],
    profile_rows: list[list[str]],
    references: dict[str, dict],
    profiles: dict,
    theme: str,
) -> dict[str, str]:
    """Choose one material origin per terrain family across the whole map.

    This is the missing layer between a map theme and physical chunks. All
    highland cells (MOUNTAIN, PEAK, and CLIFF) share a material decision even
    when a river or plain splits them into separate connected components.
    """
    profile_definitions = profiles.get("profiles", {})
    coverage_by_family: dict[str, dict[str, int]] = {}
    preferred_by_family: dict[str, set[str]] = {}

    for y, row in enumerate(domains):
        for x, candidates in enumerate(row):
            family = family_rows[y][x]
            profile = theme if theme != "auto" else profile_rows[y][x]
            preferred_by_family.setdefault(family, set()).update(
                profile_definitions.get(profile, {}).get("origins", [])
            )
            origins_here = {
                origin
                for tile_hash in candidates
                for origin in references[tile_hash]["originFilePaths"]
            }
            family_coverage = coverage_by_family.setdefault(family, {})
            for origin in origins_here:
                family_coverage[origin] = family_coverage.get(origin, 0) + 1

    selections: dict[str, str] = {}
    for family, coverage in coverage_by_family.items():
        preferred = {
            origin: count
            for origin, count in coverage.items()
            if origin in preferred_by_family.get(family, set())
        }
        pool = preferred if preferred else coverage
        if pool:
            selections[family] = max(pool, key=lambda origin: (pool[origin], origin))
    return selections


def lock_chunk_materials(
    domains: list[list[set[str]]],
    chunks: list[dict],
    references: dict[str, dict],
    profiles: dict,
    selected_theme: str,
    map_origin: str | None = None,
    family_origins: dict[str, str] | None = None,
) -> tuple[list[list[set[str]]], list[dict]]:
    """Restrict each visual chunk to one source palette where possible.

    The Tile Map Editor's reference set combines art from many FE8 maps. Cell
    by cell selection can accidentally take a mountain interior from one map
    and its cliff edge from another. A chunk therefore stays inside the
    already-selected global theme whenever that theme contains a valid piece.
    """
    locked = [[set(cell) for cell in row] for row in domains]
    theme_origins = set(profiles.get("profiles", {}).get(selected_theme, {}).get("origins", []))
    family_origins = family_origins or {}
    output_chunks: list[dict] = []

    for chunk in chunks:
        origin_coverage: dict[str, int] = {}
        for x, y in chunk["cells"]:
            # Count once per cell, not once per candidate. One palette that
            # contains several variants of a tile should not receive a bias.
            origins_here = {
                origin
                for tile_hash in domains[y][x]
                for origin in references[tile_hash]["originFilePaths"]
            }
            for origin in origins_here:
                origin_coverage[origin] = origin_coverage.get(origin, 0) + 1

        family_origin = family_origins.get(chunk["family"])
        family_coverage = origin_coverage.get(family_origin, 0) if family_origin is not None else 0
        map_coverage = origin_coverage.get(map_origin, 0) if map_origin is not None else 0
        # Do not force a global material into a chunk it cannot actually
        # express. A 70% threshold keeps the map visually unified while still
        # allowing a special shrine/castle/rare edge to use its own prefab.
        # A family has priority: all mountain pieces should look like one
        # mountain type even when the individual ranges are disconnected.
        if family_origin is not None and family_coverage * 2 >= len(chunk["cells"]):
            chosen_origin = family_origin
        elif map_origin is not None and map_coverage * 10 >= len(chunk["cells"]) * 7:
            chosen_origin = map_origin
        themed_origins = {
            origin: coverage
            for origin, coverage in origin_coverage.items()
            if origin in theme_origins
        }
        if family_origin is not None and family_coverage * 2 >= len(chunk["cells"]):
            pass
        elif map_coverage * 10 >= len(chunk["cells"]) * 7:
            pass
        elif themed_origins:
            # The selected theme is the first decision; coverage decides
            # between sources inside that one palette pack.
            chosen_origin = max(
                themed_origins,
                key=lambda origin: (themed_origins[origin], origin),
            )
        elif origin_coverage:
            chosen_origin = max(
                origin_coverage,
                key=lambda origin: (
                    origin_coverage[origin],
                    origin,
                ),
            )
        else:
            chosen_origin = None

        matched_cells = 0
        if chosen_origin is not None:
            for x, y in chunk["cells"]:
                palette_candidates = {
                    tile_hash
                    for tile_hash in locked[y][x]
                    if chosen_origin in references[tile_hash]["originFilePaths"]
                }
                if palette_candidates:
                    locked[y][x] = palette_candidates
                    matched_cells += 1

        output_chunks.append(
            {
                "id": chunk["id"],
                "family": chunk["family"],
                "profile": chunk["profile"],
                "bounds": chunk["bounds"],
                "cell_count": len(chunk["cells"]),
                "origin": chosen_origin,
                "family_origin": family_origin,
                "palette_coverage": matched_cells,
            }
        )
    return locked, output_chunks


def compile_tile_map(
    terrain: list[list[int]],
    support: dict[str, list[str]],
    references: dict[str, dict],
    profiles: dict,
    rng: random.Random,
    max_backtracks: int,
    theme: str = "auto",
) -> tuple[list[list[str]], dict]:
    # The API asset pack is already FE8-only. The input map chooses the visual
    # category, so users never need to supply a source chapter or theme path.
    themed = {tile_hash: entry for tile_hash, entry in references.items() if entry["group"] != "EMPTY"}
    if not themed:
        raise BuildError("The bundled asset pack contains no usable tile images.")

    by_group: dict[str, set[str]] = {}
    for tile_hash, entry in themed.items():
        by_group.setdefault(entry["group"], set()).add(tile_hash)

    height, width = len(terrain), len(terrain[0])
    domains: list[list[set[str]]] = []
    for y, row in enumerate(terrain):
        domain_row: list[set[str]] = []
        for x, terrain_id in enumerate(row):
            groups = support.get(str(terrain_id))
            if groups is None:
                raise BuildError(f"Cell ({x}, {y}) uses unsupported terrain id {terrain_id}.")
            candidates: set[str] = set()
            for group in groups:
                candidates.update(by_group.get(group, set()))
            if not candidates:
                options = ", ".join(groups)
                raise BuildError(
                    f"The asset pack has no visual tiles for terrain id {terrain_id} "
                    f"(expected one of: {options})."
                )
            domain_row.append(candidates)
        domains.append(domain_row)

    selected_theme, theme_scores = resolve_theme(domains, references, profiles, theme)
    domains, theme_fallback_rows = constrain_to_theme(domains, references, profiles, selected_theme)
    visual_plan = derive_visual_plan(terrain, profiles, selected_theme)
    visual_plan["selected_theme"] = selected_theme
    visual_plan["theme_scores"] = theme_scores
    visual_plan["theme_fallback_rows"] = theme_fallback_rows
    family_rows = visual_plan["family_rows"]
    profile_rows = visual_plan["profile_rows"]
    profile_definitions = profiles.get("profiles", {})
    selected_theme_origins = set(profile_definitions.get(selected_theme, {}).get("origins", []))
    family_domains: dict[str, set[str]] = {}
    for y in range(height):
        for x in range(width):
            family_domains.setdefault(family_rows[y][x], set()).update(domains[y][x])

    chunk_rows, chunks = build_visual_chunks(family_rows, profile_rows)
    visual_plan["chunk_rows"] = chunk_rows

    # A completely uniform region is the common case for a freshly generated
    # plain, sea, or floor.  Solving every cell independently is needlessly
    # expensive and can introduce boundary/decorative variants (for example,
    # road-edge art in the editor's broad PLAIN group).  A self-compatible tile
    # is valid against a copy of itself on every side, so one such tile fills
    # the region exactly and produces a clean, seamless base layer.
    first_domain = domains[0][0]
    if all(domain == first_domain for row in domains for domain in row):
        repeatable = sorted(
            tile_hash
            for tile_hash in first_domain
            if all(tile_hash in references[tile_hash][direction] for direction in DIRS)
        )
        if repeatable:
            chosen = rng.choice(repeatable)
            # A uniform map is necessarily one visual-material chunk. The
            # repeatable sprite itself locks its appearance perfectly.
            chunk = chunks[0]
            visual_plan["chunks"] = [{
                "id": chunk["id"],
                "family": chunk["family"],
                "profile": chunk["profile"],
                "bounds": chunk["bounds"],
                "cell_count": len(chunk["cells"]),
                "origin": None,
                "palette_coverage": len(chunk["cells"]),
            }]
            visual_plan["map_material_origin"] = None
            visual_plan["family_material_origins"] = {}
            return [[chosen for _ in range(width)] for _ in range(height)], visual_plan

    # Tighten each gameplay cell's broad sprite pool to the FE8 sprites whose
    # observed edge orientation matches the cell's derived role.  A role can
    # ask for a configuration absent from the original maps, so
    # ``oriented_candidates`` has a nearest-role fallback rather than making a
    # user-generated map impossible to render.
    oriented_domains: list[list[set[str]]] = []
    for y in range(height):
        oriented_row: list[set[str]] = []
        for x in range(width):
            oriented_row.append(
                oriented_candidates(
                    domains[y][x],
                    cardinal_role_mask(visual_plan["role_rows"][y][x]),
                    family_domains[family_rows[y][x]],
                    references,
                )
            )
        oriented_domains.append(oriented_row)
    map_material_origin = choose_map_material_origin(oriented_domains, references, profiles, selected_theme)
    family_material_origins = choose_family_material_origins(
        oriented_domains, family_rows, profile_rows, references, profiles, selected_theme
    )
    domains, chunk_metadata = lock_chunk_materials(
        oriented_domains, chunks, references, profiles, selected_theme, map_material_origin, family_material_origins
    )
    visual_plan["map_material_origin"] = map_material_origin
    visual_plan["family_material_origins"] = family_material_origins
    visual_plan["chunks"] = chunk_metadata

    def propagate(state: list[list[set[str]]]) -> bool:
        queue = deque(
            (x, y, direction)
            for y in range(height)
            for x in range(width)
            for direction, (dx, dy) in DIRS.items()
            if 0 <= x + dx < width and 0 <= y + dy < height
        )
        while queue:
            x, y, direction = queue.popleft()
            dx, dy = DIRS[direction]
            nx, ny = x + dx, y + dy
            before = state[y][x]
            kept = {
                tile_hash
                for tile_hash in before
                if any(neighbor in references[tile_hash][direction] for neighbor in state[ny][nx])
            }
            if not kept:
                return False
            if kept != before:
                state[y][x] = kept
                # Current domain changed: neighbours must be revised against it.
                for neighbor_direction, (ndx, ndy) in DIRS.items():
                    px, py = x + ndx, y + ndy
                    if 0 <= px < width and 0 <= py < height:
                        queue.append((px, py, OPPOSITE[neighbor_direction]))
        return True

    def clone(state: list[list[set[str]]]) -> list[list[set[str]]]:
        return [[set(cell) for cell in row] for row in state]

    def greedy_assign() -> list[list[str]]:
        """Always produce a coherent best-effort visual assignment.

        Procedural terrain can request arrangements that never occurred in an
        original FE8 chapter (for example, a lone castle gate in open grass).
        The strict solver quite correctly rejects those maps, but a sandbox
        renderer should still be able to show them. This fallback favours
        matches with already placed north/west neighbours and candidates that
        can plausibly connect to the remaining cells.
        """
        result: list[list[str]] = [["" for _ in range(width)] for _ in range(height)]
        candidate_limit = 48

        # A village is not a single 16x16 image. This is the FE8 Chapter 6
        # three-by-three village footprint expressed in the sandbox's existing
        # gameplay IDs: SPECIAL_2E around a VILLAGE interaction cell.
        village_pattern = ((46, 46, 46), (46, 46, 46), (46, 9, 46))
        village_tiles = (
            ("ce22d0ebf146eb54bede809bcc4aba72", "cc866e3b0b5d75fcefd3ed963b074304", "c271f984334cfe5dcd501103b7eb9583"),
            ("3857252a2cc4b18f4a3f5ea064c279f0", "969c4f54dbb6c3a73ae01817507b7cbb", "1421c34fb8c979b2a538034b081ed642"),
            ("36f10399c917c62f1875a6572434a70a", "499499e04fc3cc40b14f3a46bc16df74", "907564d07eaeb476ff1b46ff43efd2d6"),
        )
        for y in range(height - 2):
            for x in range(width - 2):
                if all(terrain[y + dy][x + dx] == village_pattern[dy][dx] for dy in range(3) for dx in range(3)):
                    for dy in range(3):
                        for dx in range(3):
                            result[y + dy][x + dx] = village_tiles[dy][dx]

        def degree(tile_hash: str) -> int:
            return sum(len(references[tile_hash][direction]) for direction in DIRS)

        def preferred_origins(x: int, y: int) -> set[str]:
            return selected_theme_origins

        def expected_domain(x: int, y: int, nx: int, ny: int) -> set[str]:
            if family_rows[y][x] == family_rows[ny][nx]:
                return family_domains[family_rows[y][x]]
            return domains[ny][nx]

        for y in range(height):
            for x in range(width):
                if result[y][x]:
                    continue
                candidates = list(domains[y][x])
                rng.shuffle(candidates)
                origins = preferred_origins(x, y)

                def static_score(tile_hash: str) -> int:
                    score = degree(tile_hash)
                    score += 40 * len(origins.intersection(references[tile_hash]["originFilePaths"]))
                    for direction, (dx, dy) in DIRS.items():
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < width and 0 <= ny < height:
                            desired = expected_domain(x, y, nx, ny)
                            score += 500 if any(neighbor in desired for neighbor in references[tile_hash][direction]) else 0
                    return score

                candidates.sort(key=static_score, reverse=True)
                candidates = candidates[:candidate_limit]
                best_tile = candidates[0]
                best_score = -1
                for tile_hash in candidates:
                    score = static_score(tile_hash)
                    for direction, (dx, dy) in DIRS.items():
                        nx, ny = x + dx, y + dy
                        if not (0 <= nx < width and 0 <= ny < height):
                            continue
                        if result[ny][nx]:
                            if result[ny][nx] in references[tile_hash][direction]:
                                score += 10000
                    if score > best_score:
                        best_tile, best_score = tile_hash, score
                result[y][x] = best_tile
        return result

    attempts = 0

    def solve(state: list[list[set[str]]]) -> list[list[set[str]]] | None:
        nonlocal attempts
        if not propagate(state):
            return None
        choices = [(len(state[y][x]), x, y) for y in range(height) for x in range(width) if len(state[y][x]) > 1]
        if not choices:
            return state
        _, x, y = min(choices)
        values = list(state[y][x])
        rng.shuffle(values)
        # Prefer tiles that have many valid neighbours in this asset pack. The
        # shuffle keeps equal-scoring choices seed-dependent, while this sort
        # avoids wasting branches on visually isolated edge fragments.
        origins = selected_theme_origins
        values.sort(
            key=lambda tile_hash: (
                40 * len(origins.intersection(references[tile_hash]["originFilePaths"]))
                + sum(
                    sum(neighbor in themed for neighbor in references[tile_hash][direction])
                    for direction in DIRS
                )
            ),
            reverse=True,
        )
        for tile_hash in values:
            attempts += 1
            if attempts > max_backtracks:
                raise BuildError(
                    f"Stopped after {max_backtracks} search branches. Simplify the terrain boundaries "
                    "or use more specific VISUAL_* terrain ids."
                )
            branch = clone(state)
            branch[y][x] = {tile_hash}
            result = solve(branch)
            if result is not None:
                return result
        return None

    # Large procedural maps are better served by the predictable linear-time
    # fallback than an exponential global search.
    if width * height > 256:
        return greedy_assign(), visual_plan

    result = solve(domains)
    if result is None:
        return greedy_assign(), visual_plan
    return [[next(iter(result[y][x])) for x in range(width)] for y in range(height)], visual_plan


def paeth(left: int, above: int, upper_left: int) -> int:
    prediction = left + above - upper_left
    dl, da, du = abs(prediction - left), abs(prediction - above), abs(prediction - upper_left)
    return left if dl <= da and dl <= du else above if da <= du else upper_left


def read_rgba_png(path: Path) -> tuple[int, int, bytes]:
    raw = path.read_bytes()
    if raw[:8] != b"\x89PNG\r\n\x1a\n":
        raise BuildError(f"Not a PNG tile: {path}")
    chunks: dict[bytes, list[bytes]] = {}
    offset = 8
    while offset < len(raw):
        size = struct.unpack_from(">I", raw, offset)[0]
        kind = raw[offset + 4:offset + 8]
        chunks.setdefault(kind, []).append(raw[offset + 8:offset + 8 + size])
        offset += 12 + size
    width, height, depth, color_type, compression, filtering, interlace = struct.unpack(
        ">IIBBBBB", chunks[b"IHDR"][0]
    )
    if (depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
        raise BuildError(f"Expected a non-interlaced 8-bit RGBA tile: {path}")
    stride = width * 4
    packed = zlib.decompress(b"".join(chunks[b"IDAT"]))
    cursor = 0
    previous = bytearray(stride)
    rows: list[bytes] = []
    for _ in range(height):
        filter_type = packed[cursor]
        cursor += 1
        encoded = packed[cursor:cursor + stride]
        cursor += stride
        decoded = bytearray(stride)
        for index, value in enumerate(encoded):
            left = decoded[index - 4] if index >= 4 else 0
            above = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 0:
                decoded[index] = value
            elif filter_type == 1:
                decoded[index] = (value + left) & 255
            elif filter_type == 2:
                decoded[index] = (value + above) & 255
            elif filter_type == 3:
                decoded[index] = (value + ((left + above) // 2)) & 255
            elif filter_type == 4:
                decoded[index] = (value + paeth(left, above, upper_left)) & 255
            else:
                raise BuildError(f"Unsupported PNG filter {filter_type}: {path}")
        rows.append(bytes(decoded))
        previous = decoded
    return width, height, b"".join(rows)


def png_chunk(kind: bytes, body: bytes) -> bytes:
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)


def write_rgb_png(path: Path, width: int, height: int, pixels: bytearray) -> None:
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw.extend(pixels[y * width * 3:(y + 1) * width * 3])
    output = b"\x89PNG\r\n\x1a\n"
    output += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    output += png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    output += png_chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(output)


def render(tile_rows: list[list[str]], references: dict[str, dict], asset_root: Path, output: Path, scale: int) -> None:
    if scale < 1:
        raise BuildError("Scale must be at least 1.")
    cells_h, cells_w = len(tile_rows), len(tile_rows[0])
    width, height = cells_w * 16, cells_h * 16
    pixels = bytearray(width * height * 3)
    tile_cache: dict[str, bytes] = {}
    for y, row in enumerate(tile_rows):
        for x, tile_hash in enumerate(row):
            entry = references[tile_hash]
            if tile_hash not in tile_cache:
                path = asset_root / "images" / entry["group"] / f"{tile_hash}.png"
                tile_w, tile_h, rgba = read_rgba_png(path)
                if (tile_w, tile_h) != (16, 16):
                    raise BuildError(f"Tile is not 16x16: {path}")
                tile_cache[tile_hash] = rgba
            rgba = tile_cache[tile_hash]
            for ty in range(16):
                for tx in range(16):
                    source = (ty * 16 + tx) * 4
                    if rgba[source + 3] == 0:
                        continue
                    target = ((y * 16 + ty) * width + x * 16 + tx) * 3
                    pixels[target:target + 3] = rgba[source:source + 3]
    if scale > 1:
        scaled_w, scaled_h = width * scale, height * scale
        scaled = bytearray(scaled_w * scaled_h * 3)
        for y in range(height):
            for x in range(width):
                colour = pixels[(y * width + x) * 3:(y * width + x + 1) * 3]
                for sy in range(scale):
                    for sx in range(scale):
                        target = (((y * scale + sy) * scaled_w) + x * scale + sx) * 3
                        scaled[target:target + 3] = colour
        pixels, width, height = scaled, scaled_w, scaled_h
    write_rgb_png(output, width, height, pixels)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--map", type=Path, required=True, help="JSON terrain grid or {terrain_rows: ...}.")
    parser.add_argument("--asset-root", type=Path, required=True, help="Bundled FE8 tile asset directory.")
    parser.add_argument("--support", type=Path, required=True, help="terrain_support.json from this API.")
    parser.add_argument(
        "--profiles",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "data" / "visual_profiles.json",
        help="Automatic visual-theme and topology profile data.",
    )
    parser.add_argument("--output-json", type=Path, required=True, help="Builder tile-hash map output.")
    parser.add_argument("--output-png", type=Path, required=True, help="Rendered PNG output.")
    parser.add_argument("--scale", type=int, default=3, help="Nearest-neighbour scale factor.")
    parser.add_argument("--seed", type=int, default=1, help="Deterministic tile-choice seed.")
    parser.add_argument(
        "--theme",
        default="auto",
        help="Sprite palette preference: auto, temperate, highland, coastal, desert, or fortress.",
    )
    parser.add_argument("--max-backtracks", type=int, default=20000, help="Search budget for visual constraint solving.")
    args = parser.parse_args()

    try:
        terrain = load_terrain_grid(args.map)
        support = json.loads(args.support.read_text(encoding="utf-8"))["terrain_groups"]
        profiles = json.loads(args.profiles.read_text(encoding="utf-8"))
        reference_path = args.asset_root / "tileReferences.json"
        references = {entry["tileHash"]: entry for entry in json.loads(reference_path.read_text(encoding="utf-8"))}
        tile_rows, visual_plan = compile_tile_map(
            terrain, support, references, profiles, random.Random(args.seed), args.max_backtracks, args.theme
        )
        result = {
            "version": 2,
            "asset_pack": "fe8_consolidated",
            "width": len(tile_rows[0]),
            "height": len(tile_rows),
            "requested_theme": args.theme,
            "theme": visual_plan.get("selected_theme", args.theme),
            "theme_scores": visual_plan.get("theme_scores", {}),
            "theme_fallback_rows": visual_plan.get("theme_fallback_rows", []),
            "map_material_origin": visual_plan.get("map_material_origin"),
            "family_material_origins": visual_plan.get("family_material_origins", {}),
            "visual_role_rows": visual_plan["role_rows"],
            "visual_family_rows": visual_plan["family_rows"],
            "visual_profile_rows": visual_plan["profile_rows"],
            "visual_chunk_rows": visual_plan.get("chunk_rows", []),
            "visual_chunks": visual_plan.get("chunks", []),
            "tile_hash_rows": tile_rows,
        }
        args.output_json.parent.mkdir(parents=True, exist_ok=True)
        args.output_json.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        render(tile_rows, references, args.asset_root, args.output_png, args.scale)
        print(f"Built {result['width']}x{result['height']} map -> {args.output_png}")
    except (BuildError, KeyError, OSError, json.JSONDecodeError) as error:
        raise SystemExit(f"Map build failed: {error}")


if __name__ == "__main__":
    main()
