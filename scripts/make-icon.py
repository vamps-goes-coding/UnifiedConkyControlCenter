#!/usr/bin/env python3
"""
Convert resources/icon.png into a real transparent PNG.

The shipped asset is a JPEG (mislabelled .png) with a grey checkerboard baked
in where transparency should be. This floods the checkerboard from the image
border, cuts it out, and writes proper RGBA PNGs at 512px (in-app / Qt
resource) and 256px (hicolor icon theme slot).

Run from the repository root:  python3 scripts/make-icon.py
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

SRC = Path("resources/icon.png")
OUT_APP = Path("resources/icon.png")        # 512px, used by resources.qrc
OUT_THEME = Path("resources/icon-256.png")  # 256px, installed into hicolor
OUT_THEME_SIZE = 256
OUT_APP_SIZE = 512

# The checkerboard is achromatic (border saturation maxes out around 0.08,
# luminance clusters at ~60 and ~115) while nothing inside the badge matches
# that profile - measured: 0 badge pixels with sat<0.08 and 40<lum<180.
SAT_STRICT, LUM_LO, LUM_HI = 0.10, 38, 165   # definitely checkerboard
SAT_LOOSE = 0.30                              # includes anti-aliased fringe
LUM_LOOSE, LUM_HI_LOSE = 25, 220
SEED_BORDER = 12  # only seed from the outer ring - interior greys stay opaque


def sat_lum(rgb: np.ndarray):
    mx = rgb.max(axis=-1)
    mn = rgb.min(axis=-1)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1), 0.0)
    lum = rgb.mean(axis=-1)
    return sat, lum


def flood(seed: np.ndarray, allowed: np.ndarray) -> np.ndarray:
    """Grow `seed` through `allowed` using 4-connected propagation."""
    cur = seed.copy()
    for _ in range(1024):
        nxt = cur.copy()
        nxt[1:, :] |= cur[:-1, :]
        nxt[:-1, :] |= cur[1:, :]
        nxt[:, 1:] |= cur[:, :-1]
        nxt[:, :-1] |= cur[:, 1:]
        nxt &= allowed
        if np.array_equal(nxt, cur):
            return cur
        cur = nxt
    return cur


def fill_from_subject(rgb: np.ndarray, bg: np.ndarray, passes: int = 4) -> np.ndarray:
    """
    Paint background pixels with the colour of their nearest non-background
    neighbour. Without this, downscaling averages the badge against whatever
    colour happens to sit at alpha=0 and leaves a grey fringe.
    """
    out = rgb.copy()
    known = ~bg
    for _ in range(passes):
        grown = known.copy()
        grown[1:, :] |= known[:-1, :]
        grown[:-1, :] |= known[1:, :]
        grown[:, 1:] |= known[:, :-1]
        grown[:, :-1] |= known[:, 1:]
        newly = grown & bg
        if not newly.any():
            break
        # Average the known neighbours for each newly-filled pixel.
        acc = np.zeros_like(out, dtype=np.int32)
        cnt = np.zeros(bg.shape, dtype=np.int32)
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            shifted_known = np.roll(known, (dy, dx), axis=(0, 1))
            shifted_val = np.roll(out, (dy, dx), axis=(0, 1))
            valid = shifted_known & newly
            acc += shifted_val * valid[..., None]
            cnt += valid
        has = cnt[..., None] > 0
        fill = np.where(has, acc // np.maximum(cnt[..., None], 1), out)
        out = np.where(newly[..., None], fill, out)
        known = grown
    return out


def main() -> int:
    if not SRC.exists():
        print(f"missing {SRC}", file=sys.stderr)
        return 1

    src = Image.open(SRC)
    src_format = src.format
    rgb = np.asarray(src.convert("RGB")).astype(np.int16)
    h, w, _ = rgb.shape
    print(f"source: {SRC} format={src_format} size={w}x{h}")

    sat, lum = sat_lum(rgb.astype(np.float64))
    strict = (sat <= SAT_STRICT) & (lum >= LUM_LO) & (lum <= LUM_HI)
    loose = (sat <= SAT_LOOSE) & (lum >= LUM_LOOSE) & (lum <= LUM_HI_LOSE)

    # Seed strictly from the border ring, then flood through the looser rule so
    # the anti-aliased halo around the badge gets swallowed too.
    seed = np.zeros((h, w), dtype=bool)
    seed[:SEED_BORDER, :] = strict[:SEED_BORDER, :]
    seed[-SEED_BORDER:, :] = strict[-SEED_BORDER:, :]
    seed[:, :SEED_BORDER] |= strict[:, :SEED_BORDER]
    seed[:, -SEED_BORDER:] |= strict[:, -SEED_BORDER:]

    # Anything inside the badge that trips the loose rule is only reachable by
    # crossing the (high-saturation) ring, so it stays opaque.
    bg = flood(seed, loose)
    pct = 100.0 * bg.sum() / bg.size
    print(f"background removed: {pct:.1f}% of pixels")
    if pct < 35 or pct > 75:
        print("!! suspicious background coverage - refusing to write", file=sys.stderr)
        return 1

    filled = fill_from_subject(rgb, bg)

    alpha = np.where(bg, 0, 255).astype(np.uint8)
    rgba = np.dstack([filled.astype(np.uint8), alpha])
    img = Image.fromarray(rgba, "RGBA")

    # Soften the cut-out edge; the fill above keeps the fringe badge-coloured.
    a_channel = img.getchannel("A").filter(ImageFilter.GaussianBlur(0.7))
    img.putalpha(a_channel)

    img.resize((OUT_APP_SIZE, OUT_APP_SIZE), Image.LANCZOS).save(OUT_APP, "PNG")
    img.resize((OUT_THEME_SIZE, OUT_THEME_SIZE), Image.LANCZOS).save(OUT_THEME, "PNG")

    for p in (OUT_APP, OUT_THEME):
        chk = Image.open(p)
        print(f"wrote {p}: format={chk.format} mode={chk.mode} size={chk.size}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
