#!/usr/bin/env python3
"""
CLI DECOR image renderer helper.
Called by clidecor.sh — not meant to be run standalone, but works fine if you do.

Usage:
    imgrender.py <image_path> <target_width_cols> <style: color|ascii>

Prints ANSI-colored (or plain ascii) lines to stdout, one per terminal row.
"""
import sys
from PIL import Image

ASCII_RAMP = " .:-=+*#%@"


def load_resized(path, width_cols, rows_multiplier=2):
    img = Image.open(path).convert("RGB")
    # each text row = 2 vertical pixels (half-block trick), except ascii mode = 1:1-ish
    aspect = img.height / img.width
    target_h = max(1, int(width_cols * aspect * 0.55 * rows_multiplier))
    img = img.resize((width_cols, target_h))
    return img


def render_color(path, width_cols):
    img = load_resized(path, width_cols, rows_multiplier=2)
    w, h = img.size
    px = img.load()
    out_lines = []
    # walk two pixel-rows at a time -> one text row using upper half block
    for y in range(0, h - 1, 2):
        line = []
        for x in range(w):
            top = px[x, y]
            bot = px[x, y + 1]
            line.append(
                f"\033[38;2;{top[0]};{top[1]};{top[2]}m"
                f"\033[48;2;{bot[0]};{bot[1]};{bot[2]}m▀"
            )
        line.append("\033[0m")
        out_lines.append("".join(line))
    return out_lines


def render_ascii(path, width_cols):
    img = load_resized(path, width_cols, rows_multiplier=1).convert("L")
    w, h = img.size
    px = img.load()
    out_lines = []
    for y in range(h):
        line = []
        for x in range(w):
            brightness = px[x, y] / 255
            idx = int(brightness * (len(ASCII_RAMP) - 1))
            line.append(ASCII_RAMP[idx])
        out_lines.append("".join(line))
    return out_lines


def main():
    if len(sys.argv) < 3:
        print("usage: imgrender.py <image> <width_cols> [color|ascii]", file=sys.stderr)
        sys.exit(1)

    path = sys.argv[1]
    width_cols = int(sys.argv[2])
    style = sys.argv[3] if len(sys.argv) > 3 else "color"

    try:
        if style == "ascii":
            lines = render_ascii(path, width_cols)
        else:
            lines = render_color(path, width_cols)
    except Exception as e:
        print(f"[imgrender error: {e}]", file=sys.stderr)
        sys.exit(1)

    for line in lines:
        print(line)


if __name__ == "__main__":
    main()
