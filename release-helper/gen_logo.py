#!/usr/bin/env python3
"""Regenerates src/ui/startup/hometiles_logo.cpp from docs/images/logo.svg.

The SVG only contains rects and axis-aligned bars, so instead of pulling in
a full SVG renderer this just re-draws the same coordinates with Pillow at
high resolution and downsamples for antialiasing. If docs/images/logo.svg
ever changes, update the coordinates below to match and re-run this script.

Usage: python3 release-helper/gen_logo.py
Requires: pip install Pillow
"""

import os
from PIL import Image, ImageDraw

# Exact geometry from docs/images/logo.svg (viewBox 0 0 48 48).
SQUARES = [
    (4, 4, 21, 21, 4),   # x0, y0, x1, y1, corner radius
    (27, 4, 44, 21, 4),
    (4, 27, 21, 44, 4),
]
WHITE = (255, 255, 255, 255)
TEAL = (0x26, 0xA6, 0x9A, 255)
V_BAR = (33, 26, 38, 44)
H_BAR = (26.5, 32.5, 44.5, 37.5)

NATIVE = 144            # final asset size (3x of the 48-unit design)
SUPERSAMPLE = 8         # supersample factor for antialiasing
WORK = NATIVE * SUPERSAMPLE
SCALE = WORK / 48.0


def render():
    img = Image.new("RGBA", (WORK, WORK), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    for (x0, y0, x1, y1, rx) in SQUARES:
        box = [x0 * SCALE, y0 * SCALE, x1 * SCALE, y1 * SCALE]
        draw.rounded_rectangle(box, radius=rx * SCALE, fill=WHITE)
    vx0, vy0, vx1, vy1 = V_BAR
    draw.rectangle([vx0 * SCALE, vy0 * SCALE, vx1 * SCALE, vy1 * SCALE], fill=TEAL)
    hx0, hy0, hx1, hy1 = H_BAR
    draw.rectangle([hx0 * SCALE, hy0 * SCALE, hx1 * SCALE, hy1 * SCALE], fill=TEAL)
    return img.resize((NATIVE, NATIVE), Image.BOX)


def to_c_source(img):
    w, h = img.size
    pixels = img.load()
    raw = bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            raw += bytes([b, g, r, a])  # lv_color32_t byte order: blue, green, red, alpha

    per_line = 20
    lines = [
        ", ".join(f"0x{b:02x}" for b in raw[i:i + per_line])
        for i in range(0, len(raw), per_line)
    ]
    body = ",\n    ".join(lines)

    return f'''#include "src/ui/startup/hometiles_logo.h"

// Generated from docs/images/logo.svg (viewBox 0 0 48 48), rendered at {w}x{h}
// with {SUPERSAMPLE}x supersampling and box-filter downsampling for clean antialiased
// edges. Pixel format matches lv_color32_t (blue, green, red, alpha bytes),
// i.e. LV_COLOR_FORMAT_ARGB8888. Regenerate via release-helper/gen_logo.py
// if the source SVG ever changes.
static const uint8_t hometiles_logo_map[] = {{
    {body}
}};

// Positional (not designated) init on purpose: nested designated initializers
// for the embedded lv_image_header_t bitfield struct are less portable across
// the compiler versions the three device targets build with.
const lv_image_dsc_t hometiles_logo_dsc = {{
    {{LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_ARGB8888, 0, {w}, {h}, {w * 4}, 0}},
    sizeof(hometiles_logo_map),
    hometiles_logo_map,
    nullptr,
    nullptr,
}};
'''


def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_path = os.path.join(repo_root, "src", "ui", "hometiles_logo.cpp")
    img = render()
    with open(out_path, "w", newline="\n") as f:
        f.write(to_c_source(img))
    print(f"Wrote {out_path} ({img.size[0]}x{img.size[1]})")


if __name__ == "__main__":
    main()
