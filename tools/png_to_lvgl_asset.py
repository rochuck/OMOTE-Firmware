#!/usr/bin/env python3
"""
Convert a PNG to an LVGL TRUE_COLOR_ALPHA asset block (RGB565 LE + 8-bit alpha).

Usage: png_to_lvgl_asset.py <input.png> <symbol_name> <out_size> [output.c]

Produces a C source fragment compatible with the *_assets.c files in this repo.
If output.c is given, the block is written there. Otherwise it goes to stdout.
"""
import sys
from PIL import Image


def to_rgb565_le(r, g, b):
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return v & 0xFF, (v >> 8) & 0xFF


def convert(png_path, symbol, size):
    img = Image.open(png_path).convert("RGBA")
    img = img.resize((size, size), Image.LANCZOS)
    w, h = img.size

    bytes_out = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            lo, hi = to_rgb565_le(r, g, b)
            row.extend([lo, hi, a])
        bytes_out.append(row)

    define = f"LV_ATTRIBUTE_IMG_{symbol.upper()}"
    lines = []
    lines.append(f"#ifndef {define}")
    lines.append(f"#define {define}")
    lines.append("#endif")
    lines.append("")
    lines.append(
        f"const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST {define} "
        f"uint8_t {symbol}_map[] = {{"
    )
    for row in bytes_out:
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in row) + ",")
    lines.append("};")
    lines.append("")
    lines.append(f"const lv_img_dsc_t {symbol} = {{")
    lines.append("  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,")
    lines.append("  .header.always_zero = 0,")
    lines.append("  .header.reserved = 0,")
    lines.append(f"  .header.w = {w},")
    lines.append(f"  .header.h = {h},")
    lines.append(f"  .data_size = {w * h} * LV_IMG_PX_SIZE_ALPHA_BYTE,")
    lines.append(f"  .data = {symbol}_map,")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) < 4:
        print(__doc__, file=sys.stderr)
        sys.exit(1)
    png_path, symbol, size = sys.argv[1], sys.argv[2], int(sys.argv[3])
    out = sys.argv[4] if len(sys.argv) > 4 else None
    block = convert(png_path, symbol, size)
    if out:
        with open(out, "w") as f:
            f.write(block)
        print(f"Wrote {out} ({size}x{size}, symbol={symbol})")
    else:
        sys.stdout.write(block)


if __name__ == "__main__":
    main()
