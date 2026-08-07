#!/usr/bin/env python3
"""Create the API's self-contained FE8 asset pack from a local editor clone.

This is a one-time import utility.  The runtime API reads only the generated
assets/ directory and never accesses the Tile Map Editor afterwards.
"""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--editor-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    source_tiles = args.editor_root / "tiles"
    references = json.loads((source_tiles / "tileReferences.json").read_text(encoding="utf-8"))
    fe8_entries = [
        entry
        for entry in references
        if any(origin.startswith("Fire Emblem 8/") for origin in entry["originFilePaths"])
    ]
    if not fe8_entries:
        raise SystemExit("No FE8 entries were found in this Tile Map Editor clone.")

    copied = 0
    for entry in fe8_entries:
        source = source_tiles / "images" / entry["group"] / (entry["tileHash"] + ".png")
        destination = args.output / "images" / entry["group"] / source.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not source.is_file():
            raise SystemExit("Missing source tile: " + str(source))
        shutil.copy2(str(source), str(destination))
        copied += 1

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "tileReferences.json").write_text(json.dumps(fe8_entries, indent=2) + "\n", encoding="utf-8")
    print("Bundled {} FE8 tiles into {}".format(copied, args.output))


if __name__ == "__main__":
    main()
