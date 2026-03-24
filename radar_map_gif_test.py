#!/usr/bin/env python3
"""
Build a local radar GIF for a location using:
- OpenStreetMap raster tiles as the labeled basemap
- RainViewer transparent radar tiles as the overlay

Default center is Bavaria (Bayern).

Dependencies:
  pip install requests pillow

Example:
  python radar_map_gif_test.py
  python radar_map_gif_test.py --lat 48.9 --lon 11.45 --zoom 5 --size 540 500
"""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass
from datetime import datetime, timezone
from io import BytesIO
from pathlib import Path
from typing import Iterable

import requests
from PIL import Image, ImageDraw, ImageEnhance


OSM_TILE_URL = "https://basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}.png"
RAINVIEWER_API_URL = "https://api.rainviewer.com/public/weather-maps.json"
USER_AGENT = "Tab5RadarTest/1.0"
TILE_SIZE = 256


@dataclass(frozen=True)
class TileBounds:
    left_px: float
    top_px: float
    x0: int
    x1: int
    y0: int
    y1: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create a RainViewer + OSM radar GIF.")
    parser.add_argument("--lat", type=float, default=48.9, help="Center latitude")
    parser.add_argument("--lon", type=float, default=11.45, help="Center longitude")
    parser.add_argument("--zoom", type=int, default=5, help="Tile zoom level. Free RainViewer supports up to 7.")
    parser.add_argument(
        "--size",
        type=int,
        nargs=2,
        metavar=("WIDTH", "HEIGHT"),
        default=(540, 500),
        help="Output size in pixels",
    )
    parser.add_argument("--frames", type=int, default=0, help="How many latest radar frames to use. 0 means all available past frames.")
    parser.add_argument(
        "--output",
        default="radar_bayern.gif",
        help="Output GIF path",
    )
    parser.add_argument(
        "--label",
        default="Bayern",
        help="Label rendered on frames",
    )
    parser.add_argument(
        "--duration-ms",
        type=int,
        default=450,
        help="GIF frame duration in milliseconds",
    )
    parser.add_argument(
        "--time",
        default="",
        help="Local target time in HH:MM, e.g. 18:30. Picks the nearest RainViewer frame.",
    )
    return parser.parse_args()


def latlon_to_world_pixels(lat: float, lon: float, zoom: int) -> tuple[float, float]:
    scale = TILE_SIZE * (2**zoom)
    x = (lon + 180.0) / 360.0 * scale
    lat_rad = math.radians(lat)
    y = (1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * scale
    return x, y


def tile_bounds(lat: float, lon: float, zoom: int, width: int, height: int) -> TileBounds:
    cx, cy = latlon_to_world_pixels(lat, lon, zoom)
    left = cx - width / 2.0
    top = cy - height / 2.0
    right = cx + width / 2.0
    bottom = cy + height / 2.0
    return TileBounds(
        left_px=left,
        top_px=top,
        x0=math.floor(left / TILE_SIZE),
        x1=math.floor((right - 1) / TILE_SIZE),
        y0=max(0, math.floor(top / TILE_SIZE)),
        y1=max(0, math.floor((bottom - 1) / TILE_SIZE)),
    )


def iter_tiles(bounds: TileBounds) -> Iterable[tuple[int, int]]:
    for ty in range(bounds.y0, bounds.y1 + 1):
        for tx in range(bounds.x0, bounds.x1 + 1):
            yield tx, ty


def fetch_json(session: requests.Session, url: str) -> dict:
    resp = session.get(url, timeout=20)
    resp.raise_for_status()
    return resp.json()


def fetch_png(session: requests.Session, url: str) -> Image.Image:
    resp = session.get(url, timeout=20)
    resp.raise_for_status()
    return Image.open(BytesIO(resp.content)).convert("RGBA")


def blank_tile() -> Image.Image:
    return Image.new("RGBA", (TILE_SIZE, TILE_SIZE), (0, 0, 0, 0))


def build_osm_mosaic(
    session: requests.Session, zoom: int, bounds: TileBounds, width: int, height: int
) -> Image.Image:
    world_tiles = 2**zoom
    mosaic = Image.new(
        "RGBA",
        ((bounds.x1 - bounds.x0 + 1) * TILE_SIZE, (bounds.y1 - bounds.y0 + 1) * TILE_SIZE),
    )
    for tx, ty in iter_tiles(bounds):
        wrapped_x = tx % world_tiles
        if ty < 0 or ty >= world_tiles:
            tile = Image.new("RGBA", (TILE_SIZE, TILE_SIZE), (235, 235, 235, 255))
        else:
            url = OSM_TILE_URL.format(z=zoom, x=wrapped_x, y=ty)
            tile = fetch_png(session, url)
        mosaic.alpha_composite(tile, ((tx - bounds.x0) * TILE_SIZE, (ty - bounds.y0) * TILE_SIZE))
    crop_left = int(round(bounds.left_px - bounds.x0 * TILE_SIZE))
    crop_top = int(round(bounds.top_px - bounds.y0 * TILE_SIZE))
    return mosaic.crop((crop_left, crop_top, crop_left + width, crop_top + height))


def enhance_basemap(image: Image.Image) -> Image.Image:
    rgb = image.convert("RGB")
    rgb = ImageEnhance.Brightness(rgb).enhance(0.78)
    rgb = ImageEnhance.Contrast(rgb).enhance(1.18)
    rgb = ImageEnhance.Color(rgb).enhance(0.85)
    return rgb.convert("RGBA")


def enhance_radar_overlay(image: Image.Image) -> Image.Image:
    return image


def build_rainviewer_frame(
    session: requests.Session,
    host: str,
    frame_path: str,
    zoom: int,
    bounds: TileBounds,
    width: int,
    height: int,
) -> Image.Image:
    world_tiles = 2**zoom
    mosaic = Image.new(
        "RGBA",
        ((bounds.x1 - bounds.x0 + 1) * TILE_SIZE, (bounds.y1 - bounds.y0 + 1) * TILE_SIZE),
    )
    for tx, ty in iter_tiles(bounds):
        wrapped_x = tx % world_tiles
        if ty < 0 or ty >= world_tiles:
            tile = blank_tile()
        else:
            url = f"{host}{frame_path}/256/{zoom}/{wrapped_x}/{ty}/2/1_1.png"
            tile = fetch_png(session, url)
        mosaic.alpha_composite(tile, ((tx - bounds.x0) * TILE_SIZE, (ty - bounds.y0) * TILE_SIZE))
    crop_left = int(round(bounds.left_px - bounds.x0 * TILE_SIZE))
    crop_top = int(round(bounds.top_px - bounds.y0 * TILE_SIZE))
    return mosaic.crop((crop_left, crop_top, crop_left + width, crop_top + height))


def frame_label(unix_ts: int) -> str:
    dt = datetime.fromtimestamp(unix_ts, tz=timezone.utc).astimezone()
    return dt.strftime("%d.%m.%Y %H:%M")


def pick_frames_by_time(all_frames: list[dict], target_hhmm: str) -> list[dict]:
    if not target_hhmm:
        return all_frames
    try:
        hh, mm = [int(part) for part in target_hhmm.split(":", 1)]
    except Exception as exc:
        raise SystemExit(f"Invalid --time value: {target_hhmm}") from exc

    best = None
    best_diff = None
    for item in all_frames:
        dt = datetime.fromtimestamp(item["time"], tz=timezone.utc).astimezone()
        diff = abs((dt.hour * 60 + dt.minute) - (hh * 60 + mm))
        if best is None or diff < best_diff:
            best = item
            best_diff = diff
    return [best] if best else []


def draw_footer(frame: Image.Image, title: str, timestamp_text: str) -> Image.Image:
    out = frame.copy()
    draw = ImageDraw.Draw(out, "RGBA")
    footer_h = 34
    draw.rectangle((0, out.height - footer_h, out.width, out.height), fill=(0, 0, 0, 150))
    draw.text((12, out.height - footer_h + 8), title, fill=(255, 255, 255, 255))
    right_text_x = max(12, out.width - 8 - len(timestamp_text) * 6)
    draw.text((right_text_x, out.height - footer_h + 8), timestamp_text, fill=(255, 255, 255, 255))
    return out


def main() -> None:
    args = parse_args()
    width, height = args.size
    if args.zoom > 7:
        raise SystemExit("RainViewer free access currently supports zoom <= 7.")

    out_path = Path(args.output).resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    session = requests.Session()
    session.headers.update({"User-Agent": USER_AGENT})

    api = fetch_json(session, RAINVIEWER_API_URL)
    host = api["host"]
    past_frames = api["radar"]["past"]
    if not past_frames:
        raise SystemExit("RainViewer returned no past radar frames.")
    frames = pick_frames_by_time(past_frames, args.time) if args.time else (past_frames if args.frames <= 0 else past_frames[-args.frames :])

    bounds = tile_bounds(args.lat, args.lon, args.zoom, width, height)
    base = build_osm_mosaic(session, args.zoom, bounds, width, height)

    gif_frames: list[Image.Image] = []
    for item in frames:
        overlay = enhance_radar_overlay(build_rainviewer_frame(session, host, item["path"], args.zoom, bounds, width, height))
        composed = Image.alpha_composite(base.copy(), overlay)
        composed = draw_footer(composed, args.label, frame_label(item["time"]))
        gif_frames.append(composed.convert("P", palette=Image.Palette.ADAPTIVE))

    if not gif_frames:
        raise SystemExit("No frames rendered.")

    gif_frames[0].save(
        out_path,
        save_all=True,
        append_images=gif_frames[1:],
        duration=args.duration_ms,
        loop=0,
        optimize=False,
        disposal=2,
    )

    print(f"Saved GIF: {out_path}")


if __name__ == "__main__":
    main()
