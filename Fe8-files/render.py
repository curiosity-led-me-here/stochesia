#!/usr/bin/env python3
"""Render an exported FE8 map layout, or a terrain-id grid, to a PNG.

No third-party image library is required. The renderer decodes the repository's
4-bit indexed map sheets and JASC palettes, then applies FE8's four 8x8 tile
entries for every 16x16 map cell.
"""

from __future__ import annotations

import argparse
import json
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parent
DATA = ROOT / "data"
ASSETS = ROOT / "assets/graphics_map"


def paeth(left: int, above: int, upper_left: int) -> int:
    prediction = left + above - upper_left
    left_distance = abs(prediction - left)
    above_distance = abs(prediction - above)
    upper_left_distance = abs(prediction - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def decode_indexed_png(path: Path) -> tuple[int, int, list[list[int]]]:
    """Decode the non-interlaced 4-bit greyscale PNG sheets stored in this repo."""
    raw = path.read_bytes()
    if raw[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"Not a PNG: {path}")

    chunks: dict[bytes, list[bytes]] = {}
    offset = 8
    while offset < len(raw):
        size = struct.unpack_from(">I", raw, offset)[0]
        kind = raw[offset + 4:offset + 8]
        body = raw[offset + 8:offset + 8 + size]
        chunks.setdefault(kind, []).append(body)
        offset += 12 + size

    width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(">IIBBBBB", chunks[b"IHDR"][0])
    if (bit_depth, color_type, compression, filtering, interlace) != (4, 0, 0, 0, 0):
        raise ValueError(f"Expected a non-interlaced 4-bit greyscale sheet, got {path}")

    stride = (width + 1) // 2
    packed = zlib.decompress(b"".join(chunks[b"IDAT"]))
    if len(packed) != height * (stride + 1):
        raise ValueError(f"Unexpected PNG data size in {path}")

    rows: list[list[int]] = []
    previous = bytearray(stride)
    cursor = 0
    for _ in range(height):
        filter_type = packed[cursor]
        cursor += 1
        encoded = packed[cursor:cursor + stride]
        cursor += stride
        decoded = bytearray(stride)
        for index, value in enumerate(encoded):
            left = decoded[index - 1] if index else 0
            above = previous[index]
            upper_left = previous[index - 1] if index else 0
            if filter_type == 0:
                decoded[index] = value
            elif filter_type == 1:
                decoded[index] = (value + left) & 0xFF
            elif filter_type == 2:
                decoded[index] = (value + above) & 0xFF
            elif filter_type == 3:
                decoded[index] = (value + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                decoded[index] = (value + paeth(left, above, upper_left)) & 0xFF
            else:
                raise ValueError(f"Unsupported PNG filter {filter_type} in {path}")
        pixels = []
        for byte in decoded:
            pixels.extend((byte >> 4, byte & 0x0F))
        rows.append(pixels[:width])
        previous = decoded
    return width, height, rows


def read_jasc_palette(path: Path) -> list[tuple[int, int, int]]:
    lines = path.read_text(encoding="ascii").splitlines()
    if lines[:2] != ["JASC-PAL", "0100"]:
        raise ValueError(f"Not a JASC-PAL file: {path}")
    expected = int(lines[2])
    palette = [tuple(map(int, line.split())) for line in lines[3:]]
    if len(palette) != expected or any(len(color) != 3 for color in palette):
        raise ValueError(f"Malformed palette: {path}")
    return palette


def png_chunk(kind: bytes, body: bytes) -> bytes:
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)


def write_rgb_png(path: Path, width: int, height: int, pixels: bytearray) -> None:
    raw = bytearray()
    stride = width * 3
    for row in range(height):
        raw.append(0)
        raw.extend(pixels[row * stride:(row + 1) * stride])
    encoded = b"\x89PNG\r\n\x1a\n"
    encoded += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    encoded += png_chunk(b"IDAT", zlib.compress(bytes(raw), level=9))
    encoded += png_chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)


def read_grid(path: Path, key: str) -> list[list[int]]:
    value = json.loads(path.read_text(encoding="utf-8"))
    grid = value.get(key) if isinstance(value, dict) else value
    if not isinstance(grid, list) or not grid or not all(isinstance(row, list) and row for row in grid):
        raise ValueError(f"{path} must be a JSON grid or contain '{key}'")
    width = len(grid[0])
    if not all(len(row) == width and all(isinstance(cell, int) for cell in row) for row in grid):
        raise ValueError(f"{path} must contain a rectangular integer grid")
    return grid


def terrain_grid_to_entries(terrain_rows: list[list[int]], tileset: str) -> list[list[int]]:
    catalog = json.loads((DATA / "tilesets.json").read_text(encoding="utf-8"))
    selected = next((entry for entry in catalog["tilesets"] if entry["id"] == tileset), None)
    if selected is None:
        raise ValueError(f"Unknown tileset: {tileset}")
    first_entry_for_terrain = {}
    for metatile, terrain_id in enumerate(selected["terrain_by_metatile_id"]):
        first_entry_for_terrain.setdefault(terrain_id, metatile << 2)
    missing = sorted({terrain_id for row in terrain_rows for terrain_id in row if terrain_id not in first_entry_for_terrain})
    if missing:
        raise ValueError(f"Tileset {tileset} has no visual metatile for terrain ids: {missing}")
    return [[first_entry_for_terrain[terrain_id] for terrain_id in row] for row in terrain_rows]


def render(entry_rows: list[list[int]], tileset: str, object_sheet: str, palette_name: str, output: Path, scale: int) -> None:
    if scale < 1:
        raise ValueError("Scale must be at least 1")
    if any(entry < 0 or entry >= 0x1000 for row in entry_rows for entry in row):
        raise ValueError("Tile-configuration entries must be in the range 0..4095")

    config_path = ASSETS / f"{tileset}.bin"
    sheet_path = ASSETS / f"{object_sheet}.png"
    palette_path = ASSETS / f"{palette_name}.pal"
    config = config_path.read_bytes()
    if len(config) < 0x2000:
        raise ValueError(f"Bad tile configuration: {config_path}")
    sheet_width, sheet_height, sheet = decode_indexed_png(sheet_path)
    if sheet_width % 8 or sheet_height % 8:
        raise ValueError(f"Tile sheet dimensions must be multiples of 8: {sheet_path}")
    palette = read_jasc_palette(palette_path)

    cell_height, cell_width = len(entry_rows), len(entry_rows[0])
    width, height = cell_width * 16, cell_height * 16
    pixels = bytearray(width * height * 3)
    sheet_tiles_wide = sheet_width // 8

    def draw_tile(word: int, dest_x: int, dest_y: int) -> None:
        tile_index = word & 0x03FF
        palette_bank = (word >> 12) & 0x0F
        source_x = (tile_index % sheet_tiles_wide) * 8
        source_y = (tile_index // sheet_tiles_wide) * 8
        if source_y + 8 > sheet_height:
            raise ValueError(f"Tile index {tile_index} is outside {sheet_path}")
        horizontal_flip = bool(word & 0x0400)
        vertical_flip = bool(word & 0x0800)
        for y in range(8):
            source_row = source_y + (7 - y if vertical_flip else y)
            for x in range(8):
                source_column = source_x + (7 - x if horizontal_flip else x)
                color_index = palette_bank * 16 + sheet[source_row][source_column]
                if color_index >= len(palette):
                    raise ValueError(f"Palette index {color_index} is outside {palette_path}")
                red, green, blue = palette[color_index]
                pixel_offset = ((dest_y + y) * width + dest_x + x) * 3
                pixels[pixel_offset:pixel_offset + 3] = bytes((red, green, blue))

    for cell_y, row in enumerate(entry_rows):
        for cell_x, entry in enumerate(row):
            words = struct.unpack_from("<4H", config, entry * 2)
            base_x, base_y = cell_x * 16, cell_y * 16
            draw_tile(words[0], base_x, base_y)
            draw_tile(words[1], base_x + 8, base_y)
            draw_tile(words[2], base_x, base_y + 8)
            draw_tile(words[3], base_x + 8, base_y + 8)

    if scale > 1:
        scaled = bytearray(width * scale * height * scale * 3)
        scaled_width = width * scale
        for y in range(height):
            row = pixels[y * width * 3:(y + 1) * width * 3]
            expanded = b"".join(row[x:x + 3] * scale for x in range(0, len(row), 3))
            for repeat in range(scale):
                start = (y * scale + repeat) * scaled_width * 3
                scaled[start:start + len(expanded)] = expanded
        pixels, width, height = scaled, scaled_width, height * scale
    write_rgb_png(output, width, height, pixels)


def select_variant(map_id: str, chapter_index: int | None) -> tuple[dict, dict]:
    map_path = DATA / "maps" / f"{map_id}.json"
    if not map_path.is_file():
        raise ValueError(f"Unknown layout: {map_id}")
    layout = json.loads(map_path.read_text(encoding="utf-8"))
    variants = layout["variants"]
    if chapter_index is not None:
        variant = next((item for item in variants if item["chapter_index"] == chapter_index), None)
        if variant is None:
            raise ValueError(f"Layout {map_id} has no chapter variant {chapter_index}")
    elif len(variants) == 1:
        variant = variants[0]
    elif variants:
        variant = variants[0]
        print(f"{map_id} has {len(variants)} variants; using chapter {variant['chapter_index']}. Pass --chapter-index to choose.")
    else:
        raise ValueError(f"Layout {map_id} has no chapter visual binding. Supply --tileset, --objects, and --palette.")
    return layout, variant


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map_id", nargs="?", help="Layout id, such as Ch1Map")
    parser.add_argument("--chapter-index", type=int, help="Use this chapter-specific visual variant")
    parser.add_argument("--entry-grid", type=Path, help="Render a JSON grid of tile-configuration entry ids")
    parser.add_argument("--terrain-grid", type=Path, help="Render a JSON grid of terrain ids using the first matching visual metatile")
    parser.add_argument("--tileset", help="TileConfiguration asset id; required for standalone grids")
    parser.add_argument("--objects", help="ObjectType asset id; required for standalone grids")
    parser.add_argument("--palette", help="MapPalette asset id; required for standalone grids")
    parser.add_argument("--output", type=Path, help="PNG output path")
    parser.add_argument("--scale", type=int, default=1, help="Nearest-neighbour output scale (default: 1)")
    args = parser.parse_args()

    if bool(args.entry_grid) and bool(args.terrain_grid):
        raise SystemExit("Use only one of --entry-grid or --terrain-grid")
    if not args.map_id and not (args.entry_grid or args.terrain_grid):
        raise SystemExit("Provide a map_id, --entry-grid, or --terrain-grid")

    layout = variant = None
    if args.map_id:
        layout, variant = select_variant(args.map_id, args.chapter_index)

    tileset = args.tileset or (variant and variant["tile_configuration"])
    objects = args.objects or (variant and variant["object_1"])
    palette = args.palette or (variant and variant["palette"])
    if not all((tileset, objects, palette)):
        raise SystemExit("A tileset, object sheet, and palette are required; pass --tileset, --objects, and --palette")

    if args.entry_grid:
        entries = read_grid(args.entry_grid, "tile_configuration_entry_rows")
        output_stem = args.entry_grid.stem
    elif args.terrain_grid:
        entries = terrain_grid_to_entries(read_grid(args.terrain_grid, "terrain_rows"), str(tileset))
        output_stem = args.terrain_grid.stem
    else:
        entries = layout["tile_configuration_entry_rows"]
        output_stem = args.map_id

    output = args.output or ROOT / "renders" / f"{output_stem}.png"
    render(entries, str(tileset), str(objects), str(palette), output, args.scale)
    print(f"Rendered {output} ({len(entries[0]) * 16 * args.scale}x{len(entries) * 16 * args.scale})")


if __name__ == "__main__":
    main()
