#!/usr/bin/env python3
"""Rasterize design/*.svg via Chrome, quantize to BWRY 2bpp, emit C header."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "firmware" / "include" / "icon_bitmaps.h"
CHROME = Path("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")

WX_SRC = 90
WEB_SRC = 50
CHAR_SRC = 24  # design/character/*.png (年 / 月)


def nearest(r: int, g: int, b: int, a: int) -> int | None:
    if a < 40:
        return None
    if r > 180 and g > 140 and b < 90:
        return 2  # yellow
    if r > 180 and g < 100 and b < 100:
        return 3  # red
    luma = 0.299 * r + 0.587 * g + 0.114 * b
    return 0 if luma < 140 else 1


def chrome_png(svg: Path, size: int, png: Path, work: Path, *, black_bg: bool = False) -> None:
    if not CHROME.exists():
        raise FileNotFoundError(f"need Chrome to rasterize SVG: {CHROME}")
    bg = "#000000" if black_bg else "#ffffff"
    bg_arg = "000000ff" if black_bg else "ffffffff"
    html = work / "icon.html"
    html.write_text(
        "<!doctype html><html><head><meta charset=utf-8>"
        f"<style>*{{margin:0;padding:0}}html,body{{width:{size}px;height:{size}px;"
        f"background:{bg};overflow:hidden}}"
        f"img{{width:{size}px;height:{size}px;display:block;object-fit:contain}}"
        f"</style></head><body><img src=\"{svg.resolve().as_uri()}\"></body></html>",
        encoding="utf-8",
    )
    subprocess.run(
        [
            str(CHROME),
            "--headless=new",
            "--disable-gpu",
            "--hide-scrollbars",
            f"--default-background-color={bg_arg}",
            f"--window-size={size},{size}",
            f"--screenshot={png}",
            html.resolve().as_uri(),
        ],
        check=True,
        capture_output=True,
    )


def _is_empty_pixel(r: int, g: int, b: int, a: int, *, black_bg: bool) -> bool:
    if a < 40:
        return True
    if black_bg:
        return r < 28 and g < 28 and b < 28
    return r > 248 and g > 248 and b > 248


def crop_to_content(im: Image.Image, size: int, *, black_bg: bool = False) -> Image.Image:
    pix = im.load()
    w, h = im.size
    xs, ys = [], []
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if _is_empty_pixel(r, g, b, a, black_bg=black_bg):
                continue
            xs.append(x)
            ys.append(y)
    fill = (0, 0, 0, 255) if black_bg else (255, 255, 255, 255)
    if not xs:
        return im.resize((size, size), Image.Resampling.NEAREST)
    pad = 2
    x0 = max(0, min(xs) - pad)
    y0 = max(0, min(ys) - pad)
    x1 = min(w, max(xs) + pad + 1)
    y1 = min(h, max(ys) + pad + 1)
    crop = im.crop((x0, y0, x1, y1))
    cw, ch = crop.size
    scale = min(size / cw, size / ch)
    nw, nh = max(1, int(cw * scale)), max(1, int(ch * scale))
    crop = crop.resize((nw, nh), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (size, size), fill)
    canvas.paste(crop, ((size - nw) // 2, (size - nh) // 2), crop)
    return canvas


def pack_row(pixels: list[int], *, pad_color: int = 1) -> list[int]:
    w = len(pixels)
    bpr = (w + 3) // 4
    # 2bpp pad: 0x00=black…, 0x55=white…
    pad_byte = 0x00 if pad_color == 0 else 0x55
    out = [pad_byte] * bpr
    for x, v in enumerate(pixels):
        if v is None:
            v = pad_color
        byte_i = x >> 2
        shift = 6 - 2 * (x & 3)
        out[byte_i] = (out[byte_i] & ~(0x03 << shift)) | ((v & 3) << shift)
    return out


def bitmap_from_png(
    png: Path, size: int, *, black_bg: bool = False, exact: bool = False
) -> list[int]:
    im = Image.open(png).convert("RGBA")
    if exact:
        if im.size != (size, size):
            raise ValueError(f"{png}: expected {size}x{size}, got {im.size}")
    else:
        im = crop_to_content(im, size, black_bg=black_bg)
    pad = 0 if black_bg else 1
    rows: list[int] = []
    for y in range(size):
        pix = []
        for x in range(size):
            r, g, b, a = im.getpixel((x, y))
            q = nearest(r, g, b, a)
            pix.append(pad if q is None else q)
        rows.extend(pack_row(pix, pad_color=pad))
    return rows


def c_array(name: str, data: list[int], per=16) -> str:
    lines = [f"static const uint8_t {name}[] PROGMEM = {{"]
    for i in range(0, len(data), per):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i : i + per])
        lines.append(f"  {chunk},")
    lines.append("};")
    return "\n".join(lines)


WEATHER = [
    ("sunny", "Sunny"),
    ("cloudy", "Cloudy"),
    ("overcast", "Overcast"),
    ("rain", "Rain"),
    ("sprinkle", "Sprinkle"),
    ("thunderstorm", "Thunder"),
    ("snow", "Snow"),
    ("fog", "Fog"),
    ("smog", "Smog"),
    ("sandstorm", "Sandstorm"),
    ("tornado", "Tornado"),
]

WEB = [
    ("ap", "Ap"),
    ("lan", "Lan"),
]

# Already-PNG glyphs (black on transparent); no Chrome pass.
CHARS = [
    ("year", "Year"),
    ("month", "Month"),
]


def main() -> None:
    with tempfile.TemporaryDirectory() as td:
        tdir = Path(td)
        parts: list[str] = [
            "// Generated by tools/rasterize_icons.py — do not edit by hand.",
            "#pragma once",
            "#include <Arduino.h>",
            "#include <pgmspace.h>",
            "",
            f"static constexpr int WX_ICON_SRC = {WX_SRC};",
            f"static constexpr int WEB_ICON_SRC = {WEB_SRC};",
            f"static constexpr int CHAR_ICON_SRC = {CHAR_SRC};",
            "",
        ]
        wx_names = []
        for slug, ident in WEATHER:
            svg = ROOT / "design" / "weather" / f"{slug}_active.svg"
            png = tdir / f"{slug}.png"
            chrome_png(svg, WX_SRC, png, tdir)
            data = bitmap_from_png(png, WX_SRC)
            arr = f"kWx{ident}"
            wx_names.append(arr)
            parts.append(c_array(arr, data))
            parts.append("")
        parts.append("static const uint8_t *const kWxIconBits[] = {")
        for n in wx_names:
            parts.append(f"  {n},")
        parts.append("};")
        parts.append("")

        # AP / LAN：透明底改为黑（配置页黑底 banner）
        for slug, ident in WEB:
            svg = ROOT / "design" / "icon" / f"{slug}_active.svg"
            png = tdir / f"{slug}.png"
            chrome_png(svg, WEB_SRC, png, tdir, black_bg=True)
            data = bitmap_from_png(png, WEB_SRC, black_bg=True)
            parts.append(c_array(f"kWeb{ident}", data))
            parts.append("")

        # Pixel-art glyphs: keep 1:1, no crop/scale.
        for slug, ident in CHARS:
            png = ROOT / "design" / "character" / f"{slug}.png"
            data = bitmap_from_png(png, CHAR_SRC, exact=True)
            parts.append(c_array(f"kChar{ident}", data))
            parts.append("")

        OUT.write_text("\n".join(parts) + "\n", encoding="utf-8")
        print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
