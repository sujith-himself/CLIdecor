#!/usr/bin/env python3
"""
CLI DECOR image renderer helper.
Usage: imgrender.py <image_path> <target_width_cols> <style: color|ascii> [pixel_size]

pixel_size controls the deliberate pixelation block size (default 1 = no extra pixelation).
Set pixel_size=2 or 3 in config for chunky pixel-art look.
"""
import sys
from PIL import Image

ASCII_RAMP = " .,:;+*?%#@"

# Terminal background to composite transparent pixels onto
TERM_BG = (0, 0, 0)


def open_image(path):
    """Open image, composite alpha onto terminal background color."""
    img = Image.open(path)

    if img.mode in ("RGBA", "LA") or (img.mode == "P" and "transparency" in img.info):
        img = img.convert("RGBA")
        bg = Image.new("RGBA", img.size, TERM_BG + (255,))
        bg.paste(img, mask=img.split()[3])  # alpha channel as mask
        img = bg.convert("RGB")
    else:
        img = img.convert("RGB")

    return img


def pixelate(img, block_size):
    """Downsample then upsample with NEAREST for crisp chunky pixel art look."""
    if block_size <= 1:
        return img
    small = img.resize(
        (max(1, img.width // block_size), max(1, img.height // block_size)),
        Image.NEAREST
    )
    return small.resize(img.size, Image.NEAREST)


def load_and_prepare(path, width_cols, rows_multiplier=2, block_size=1):
    img = open_image(path)

    # optional pixelation before resize
    if block_size > 1:
        img = pixelate(img, block_size)

    aspect = img.height / img.width
    target_h = max(2, int(width_cols * aspect * 0.55 * rows_multiplier))
    # force even number of rows so half-block pairing always works
    if target_h % 2 != 0:
        target_h += 1

    # NEAREST for crisp edges (no blurry interpolation)
    img = img.resize((width_cols, target_h), Image.NEAREST)
    return img


def is_bg(pixel, threshold=20):
    """True if pixel is close enough to terminal background to treat as transparent."""
    return all(abs(pixel[i] - TERM_BG[i]) <= threshold for i in range(3))


def render_color(path, width_cols, block_size=1):
    img = load_and_prepare(path, width_cols, rows_multiplier=2, block_size=block_size)
    w, h = img.size
    px = img.load()
    out_lines = []

    for y in range(0, h, 2):
        line = []
        for x in range(w):
            top = px[x, y]
            bot = px[x, y + 1] if y + 1 < h else top

            top_bg = is_bg(top)
            bot_bg = is_bg(bot)

            if top_bg and bot_bg:
                # both transparent — emit a plain space, no color
                line.append(" ")
            elif top_bg:
                # top is bg, bottom has color — use lower half block
                line.append(
                    f"\033[38;2;{bot[0]};{bot[1]};{bot[2]}m▄\033[0m"
                )
            elif bot_bg:
                # bottom is bg, top has color — use upper half block
                line.append(
                    f"\033[38;2;{top[0]};{top[1]};{top[2]}m▀\033[0m"
                )
            else:
                # both have color
                line.append(
                    f"\033[38;2;{top[0]};{top[1]};{top[2]}m"
                    f"\033[48;2;{bot[0]};{bot[1]};{bot[2]}m▀\033[0m"
                )
        out_lines.append("".join(line))

    return out_lines


def render_ascii(path, width_cols, block_size=1):
    img = load_and_prepare(path, width_cols, rows_multiplier=1, block_size=block_size)
    img = img.convert("L")
    w, h = img.size
    px = img.load()
    out_lines = []
    for y in range(h):
        line = []
        for x in range(w):
            b = px[x, y] / 255
            idx = int(b * (len(ASCII_RAMP) - 1))
            line.append(ASCII_RAMP[idx])
        out_lines.append("".join(line))
    return out_lines


def main():
    if len(sys.argv) < 3:
        print("usage: imgrender.py <image> <width_cols> [color|ascii] [block_size]",
              file=sys.stderr)
        sys.exit(1)

    path       = sys.argv[1]
    width_cols = int(sys.argv[2])
    style      = sys.argv[3] if len(sys.argv) > 3 else "color"
    block_size = int(sys.argv[4]) if len(sys.argv) > 4 else 1

    try:
        if style == "ascii":
            lines = render_ascii(path, width_cols, block_size)
        else:
            lines = render_color(path, width_cols, block_size)
    except Exception as e:
        print(f"[imgrender error: {e}]", file=sys.stderr)
        sys.exit(1)

    for line in lines:
        print(line)


if __name__ == "__main__":
    main()
