#!/usr/bin/env python3
"""Convert editor tilemap JSON (layers with tile objects) to game's comma-separated map .txt."""

import argparse
import json
import sys
from pathlib import Path


def encode_cell(cell, cols: int) -> int:
    if isinstance(cell, dict):
        tx = int(cell["tileX"])
        ty = int(cell["tileY"])
        return ty * cols + tx
    if isinstance(cell, bool):
        raise ValueError("unexpected bool in layer data")
    return int(cell)


def flatten_layer(layer_data, width: int, height: int, cols: int) -> list[int]:
    if len(layer_data) != height:
        raise ValueError(f"layer row count {len(layer_data)} != height {height}")
    out: list[int] = []
    for row in layer_data:
        if len(row) != width:
            raise ValueError(f"layer row width {len(row)} != width {width}")
        for cell in row:
            out.append(encode_cell(cell, cols))
    return out


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("input_json", type=Path)
    p.add_argument("-o", "--output", type=Path, required=True)
    p.add_argument(
        "--cols",
        type=int,
        default=7,
        help="tileset columns for tileY*cols+tileX (game Tileset.png uses 7)",
    )
    p.add_argument(
        "--depth",
        type=int,
        default=2,
        help="number of layers to export (first N layers in order)",
    )
    args = p.parse_args()

    data = json.loads(args.input_json.read_text(encoding="utf-8"))
    w, h = int(data["width"]), int(data["height"])
    layers = data["layers"]
    if len(layers) < args.depth:
        print(f"need at least {args.depth} layers, got {len(layers)}", file=sys.stderr)
        return 1

    parts: list[int] = []
    for zi in range(args.depth):
        parts.extend(flatten_layer(layers[zi]["data"], w, h, args.cols))

    expected = w * h * args.depth
    if len(parts) != expected:
        print(f"tile count {len(parts)} != {expected}", file=sys.stderr)
        return 1

    lines = [f"{w},{h},{args.depth},"]
    # One value per line is readable; engine accepts whitespace between numbers.
    chunk = 52
    for i in range(0, len(parts), chunk):
        lines.append(",".join(str(v) for v in parts[i : i + chunk]) + ",")
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {args.output} ({w}x{h}x{args.depth}, {len(parts)} tiles)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
