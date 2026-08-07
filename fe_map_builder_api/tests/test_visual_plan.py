#!/usr/bin/env python3
"""Regression checks for gameplay-grid -> visual-role derivation."""

import importlib.util
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("fe_map_builder", ROOT / "tools" / "fe_map_builder.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
PROFILES = json.loads((ROOT / "data" / "visual_profiles.json").read_text(encoding="utf-8"))


# 4=MOUNTAIN and 21=PEAK deliberately share one highland visual family.
plan = MODULE.derive_visual_plan(
    [
        [0, 0, 0, 0, 0],
        [0, 4, 4, 21, 0],
        [0, 4, 4, 4, 0],
        [0, 0, 0, 0, 0],
    ],
    PROFILES,
)

# First hex field = N/E/S/W; second = NE/SE/SW/NW.  The first cell is an
# outer corner, whereas (1, 2) is a straight southern edge with a diagonally
# connected north-east corner.  This confirms that gameplay terrain remains
# one integer while visual angle information is inferred separately.
assert plan["role_rows"][1][1] == "highland_06_2"  # east + south; SE
assert plan["role_rows"][1][2] == "highland_0e_6"  # east + south + west; SE + SW
assert plan["role_rows"][2][2] == "highland_0b_9"  # north + east + west; NE + NW
assert plan["profile_rows"][1][3] == "highland"

# An exact observed role must win over a merely similar candidate.  That is
# the selection rule used by the visual compiler after it derives the roles.
references = {
    "corner": {"north": [], "east": ["same"], "south": ["same"], "west": []},
    "interior": {"north": ["same"], "east": ["same"], "south": ["same"], "west": ["same"]},
    "same": {"north": [], "east": [], "south": [], "west": []},
}
assert MODULE.oriented_candidates({"corner", "interior"}, 0x06, {"same"}, references) == {"corner"}

# Chunk locking first respects the declared theme, then locks all compatible
# cells in a connected component to one source origin. This is what prevents
# a river/mountain region becoming a collage of unrelated FE8 palettes.
chunk_rows, chunks = MODULE.build_visual_chunks(
    [["highland", "highland", "plain"], ["highland", "highland", "plain"]],
    [["highland", "highland", "temperate"], ["highland", "highland", "temperate"]],
)
assert chunk_rows == [[0, 0, 1], [0, 0, 1]]
domains = [[{"h_t", "h_other"}, {"h_t"}, {"p_t"}], [{"h_t"}, {"h_t", "h_other"}, {"p_t"}]]
chunk_references = {
    "h_t": {"originFilePaths": ["highland-source"]},
    "h_other": {"originFilePaths": ["unrelated-source"]},
    "p_t": {"originFilePaths": ["temperate-source"]},
}
locked, chunk_info = MODULE.lock_chunk_materials(
    domains,
    chunks,
    chunk_references,
    {"profiles": {
        "highland": {"origins": ["highland-source"]},
        "temperate": {"origins": ["temperate-source"]},
    }},
    "highland",
)
assert chunk_info[0]["origin"] == "highland-source"
assert locked[0][0] == {"h_t"}
assert locked[1][1] == {"h_t"}

# The map-level pass picks a shared material source before chunk fallback.
assert MODULE.choose_map_material_origin(
    [[{"h_t", "p_t"}, {"h_t"}], [{"h_t"}, {"p_t"}]],
    chunk_references,
    {"profiles": {}},
    "auto",
) == "highland-source"
assert MODULE.choose_family_material_origins(
    domains,
    [["highland", "highland", "plain"], ["highland", "highland", "plain"]],
    [["highland", "highland", "temperate"], ["highland", "highland", "temperate"]],
    chunk_references,
    {"profiles": {
        "highland": {"origins": ["highland-source"]},
        "temperate": {"origins": ["temperate-source"]},
    }},
    "auto",
) == {"highland": "highland-source", "plain": "temperate-source"}

# A map chooses one palette pack before it enters chunk and orientation work.
# Cells with no source in that pack are preserved only as explicit fallbacks.
theme_profiles = {"profiles": {
    "highland": {"origins": ["highland-source"]},
    "temperate": {"origins": ["temperate-source"]},
}}
selected, scores = MODULE.resolve_theme(domains, chunk_references, theme_profiles, "auto")
assert selected == "highland" and scores["highland"] == 4
themed, fallbacks = MODULE.constrain_to_theme(domains, chunk_references, theme_profiles, selected)
assert themed[0][0] == {"h_t"} and fallbacks[0][0] is False
assert themed[0][2] == {"p_t"} and fallbacks[0][2] is True
print("visual role derivation passed")
