"""CHPower's pictures for its README and GitHub, written to docs/brand/,
in the CHGame brand's look (the CHGame repository's docs/brand/README.md:
neobrutalism in its palette, ink outlines, hard shadows, pixel lettering).

    python tools/brand/make.py             # everything
    python tools/brand/make.py banner      # docs/brand/banner.gif, the README's banner
    python tools/brand/make.py social      # docs/brand/social.png, GitHub's link preview (upload it by hand)
    python tools/brand/make.py buttons     # docs/brand/btn-*.svg, the README's link buttons
    python tools/brand/make.py warning     # docs/brand/warning.svg, the README's caution
    python tools/brand/make.py banner --still out/banner.png   # one frame, to look at

It draws with the CHGame repository's tools/brand/make.py (its shapes, its
Bitrimus lettering, the CHGame badge and the handheld), so it needs a
CHGame checkout: $CHGAME_TOOLS, or a CHGame folder beside this repository.
The handheld's screen shows the two apps' box art (each example's
docs/cart.png), DOWN paging from one to the other as the visual menu does.
"""
from __future__ import annotations

import argparse
import importlib.util
import os
import pathlib
import sys

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "docs" / "brand"
COVERS = [ROOT / "libraries/CHPower/examples/CHPDTrigger/docs/cart.png",
          ROOT / "libraries/CHUSBHost/examples/CHUSBHost/docs/cart.png"]


def chgame_brand():
    """The CHGame repository's tools/brand/make.py, as a module."""
    env = os.environ.get("CHGAME_TOOLS")
    for tools in ([pathlib.Path(env)] if env else []) + [ROOT.parent / "CHGame" / "tools"]:
        p = tools / "brand" / "make.py"
        if p.exists():
            spec = importlib.util.spec_from_file_location("chgame_brand", p)
            m = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(m)
            return m
    sys.exit("the CHGame repository was not found: set CHGAME_TOOLS to its tools folder, "
             "or put a CHGame checkout beside this repository")


B = chgame_brand()
W, H = B.W, B.H
MAGENTA = "#FF00FF"                         # the CHGAME pixel logo's own shadow colour

# A plug: the add-on's icon on its button.
B.ICONS["plug"] = B.art("""
    ..#...#..
    ..#...#..
    #########
    #.......#
    #.......#
    .#.....#.
    ..#####..
    ....#....
    ....#....
""")


def dotted(c, w, h):
    dots = np.zeros((h, w), dtype=bool)
    for dy in (0, 1):
        for dx in (0, 1):
            dots[12 + dy::24, 12 + dx::24] = True
    c.mask(dots, 0, 0, B.DOTS)


def draw_static(c):
    """The left column: the badge, the CHPower wordmark, what it is, pills."""
    dotted(c, W, H)
    LX, LY = B.LX, B.LY
    logo = Image.open(B.OUT / "logo.png").convert("RGBA")
    a = np.array(logo)[:, :, 3] > 128
    c.mask(a, LX + 10, LY + 10, B.INK)
    c.image(logo, LX, LY)

    face = B.Face(B.BITRIMUS)
    word = B.scaled(face.mask("CHPower"), 6)
    wx = LX + logo.width + 26
    wy = LY + (logo.height - word.shape[0]) // 2 - 4
    c.mask(word, wx + 6, wy + 6, MAGENTA)     # the pixel logo's magenta shadow
    c.mask(word, wx, wy, B.INK)

    y = LY + logo.height + 34
    for line in ("Power for the CHGame Rev 1.5.", "Pay only for what you call."):
        m = B.scaled(face.mask(line), 3)
        c.mask(m, LX, y, B.INK)
        y += m.shape[0] + 10

    ph, gap = 50, 16
    pills, x, py = [], LX, y + 18
    for label, fill in (("USB-PD", B.LAVENDER), ("USB HOST", B.MINT), ("CN1 CUT-OFF", B.SAGE)):
        m = B.scaled(face.mask(label), 3)
        w = m.shape[1] + 40
        if x > LX and x + w > LX + B.COL_W:
            x, py = LX, py + ph + gap
        pills.append((x, py, w, m, fill))
        x += w + gap + 2

    def pill_shapes(d, k):
        for x, py, w, m, fill in pills:
            B.card(d, k, x, py, w, ph, fill, r=ph // 2, bw=4, shadow=5)
    c.shapes(pill_shapes)
    for x, py, w, m, fill in pills:
        c.mask(m, x + 20, py + (ph - 21) // 2 - 3, B.INK)

    def frame(d, k):
        d.rectangle([0, 0, k * W - 1, k * H - 1], outline=B.rgb(B.INK), width=k * 6)
    c.shapes(frame)


HOLD_MS = 2600


def banner_frames():
    """[(RGB image, ms)]: PD TRIGGER's box art, then DOWN brings USB HOST's up
    from the bottom (as the visual menu does), and back."""
    base = B.Canvas(W, H, B.PAPER)
    draw_static(base)
    states = {}
    for pressed in (False, True):
        c = B.Canvas(W, H, B.PAPER)
        c.im = base.im.copy()
        B.draw_board(c, pressed="down" if pressed else None, led=pressed)
        states[pressed] = c.im
    covers = [B.cover(p) for p in COVERS]
    steps = []
    for i in range(len(covers)):
        prev, new = covers[i], covers[(i + 1) % len(covers)]
        steps.append((prev, False, HOLD_MS))
        for rows in B.SLIDE:
            pic = prev.copy()
            pic.paste(new.crop((0, 0, B.SCR, rows * 2)), (0, B.SCR - rows * 2))
            steps.append((pic, True, B.STEP_MS))
        steps.append((new, True, B.STEP_MS))
    frames = []
    for pic, down, ms in steps:
        im = states[down].copy()
        im.paste(pic, (B.SX, B.SY))
        frames.append((im, ms))
    return frames


def social_png(path):
    """GitHub's social preview, 1280 x 640: the banner's first picture on the
    same dotted paper, framed again. Uploaded by hand (Settings > Social
    preview)."""
    first = banner_frames()[0][0]
    c = B.Canvas(W, 640, B.PAPER)
    dotted(c, W, 640)
    c.im.paste(first.crop((6, 6, W - 6, H - 6)), (6, 80 + 6))

    def frame(d, k):
        d.rectangle([0, 0, k * W - 1, k * 640 - 1], outline=B.rgb(B.INK), width=k * 6)
    c.shapes(frame)
    c.im.save(path, optimize=True)


BUTTONS = [
    # file, label, icon, fill
    ("btn-chgame", "CHGame", "pad", B.YELLOW),
    ("btn-start", "Get started", "play", B.LAVENDER),
    ("btn-design", "The design", "page", B.MINT),
    ("btn-forum", "Forum", "chat", B.CREAM),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("what", nargs="*", default=["banner", "social", "buttons", "warning"])
    ap.add_argument("--still", help="write one frame of the banner as a PNG here instead")
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if "banner" in a.what:
        frames = banner_frames()
        if a.still:
            frames[0][0].save(a.still)
            print(a.still)
        else:
            p = OUT / "banner.gif"
            B.write_gif(frames, p)
            print(f"{p.relative_to(ROOT).as_posix()}: {len(frames)} frames, {p.stat().st_size:,} B")
    if "social" in a.what:
        p = OUT / "social.png"
        social_png(p)
        print(f"{p.relative_to(ROOT).as_posix()}: {p.stat().st_size:,} B")
    if "buttons" in a.what:
        for name, label, icon, fill in BUTTONS:
            (OUT / f"{name}.svg").write_text(B.button_svg(label, icon, fill), encoding="utf-8", newline="\n")
        print(f"docs/brand/btn-*.svg: {len(BUTTONS)} buttons")
    if "warning" in a.what:
        B.WARNING_TITLE = "REV 1.5 ONLY: THE POWER SECTION"
        B.WARNING_LINES = ["For the CHGame Rev1 board with its power section.",
                           "CN1 can carry up to 20 V: the library guards it."]
        p = OUT / "warning.svg"
        p.write_text(B.warning_svg(), encoding="utf-8", newline="\n")
        print(p.relative_to(ROOT).as_posix())


if __name__ == "__main__":
    main()
