r"""tools/cart/art/cover.png: the cover of the CHPower examples cart, the first
picture the CHGame visual menu shows (CHGame's docs/visual-menu.md). Edit
this and run it, not the PNG:

    python tools/cart/cover.py

Composition: the examples' box-art style (a dark room, a perspective grid
floor, gold chrome lettering). A lightning bolt stands in the middle. The
floor, haze and glow are amber on the left (PD TRIGGER's colour) and blue on
the right (USB HOST's), meeting at the bolt.

Palette (11 own colours, plus the fixed cream, grey, black and red):
  a0 a1 a2 a3        PD TRIGGER's amber ramp
  b0 b1 b2 b3        USB HOST's blue ramp
  gold0 gold1 gold2  the title and the bolt

Font: art/title.txt, CHARSET-DNS_FONT 5, the face of the examples' own
titles (its source and terms are in the file's header).

Needs a CHGame checkout beside this repository (for its tools/artkit), or
$CHGAME_TOOLS set to one's tools folder.
"""
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
TOOLS = pathlib.Path(os.environ.get("CHGAME_TOOLS", ROOT.parent / "CHGame" / "tools"))
sys.path.insert(0, str(TOOLS))
import numpy as np  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402
from scipy.ndimage import distance_transform_edt  # noqa: E402
import artkit as ak  # noqa: E402

TITLE = HERE / "art" / "title.txt"
OUT = HERE / "art" / "cover.png"
TITLE_Y = 5
TITLE_ROWS = ["gold2"] * 6 + ["gold1"] * 3 + ["gold2"] * 2 + ["gold1"] * 5
TITLE_EXTRUDE = ["gold0", "black"]

COL = {
    "a0": "#1C0A04", "a1": "#4E2410", "a2": "#B4641C", "a3": "#FFC040",
    "b0": "#020A1E", "b1": "#0A2A4C", "b2": "#2A6C92", "b3": "#4CCCE4",
    "gold0": "#7A3408", "gold1": "#E4991C", "gold2": "#FFE27A",
}
AMBER = ["black", "a0", "a1", "a2", "a3", "cream"]
BLUE = ["black", "b0", "b1", "b2", "b3", "cream"]

HORIZON = 76
SPLIT = 64                       # amber left of it, blue right
BOLT = [(60, 30), (84, 30), (72, 60), (88, 60), (50, 120), (62, 78), (44, 78)]
DEPTH = (3, 2)                   # the bolt's extrusion, toward the bottom right
STARS = [(12, 34), (26, 48), (104, 38), (118, 52), (96, 26), (20, 64), (110, 66)]

YY, XX = np.mgrid[0:128, 0:128]
X1, Y1 = XX + 0.5, YY + 0.5


def polygon_mask(pts, dx=0, dy=0):
    im = Image.new("1", (128, 128), 0)
    ImageDraw.Draw(im).polygon([(x + dx, y + dy) for x, y in pts], fill=1)
    return np.array(im, bool)


def line_mask(a, b):
    m = np.zeros((128, 128), bool)
    for x, y in ak.line_px([a, b]):
        if 0 <= x < 128 and 0 <= y < 128:
            m[y, x] = True
    return m


def side(pic, mask, level, left_ramp=AMBER, right_ramp=BLUE):
    """Colours `mask` by `level` on each side's ramp."""
    ak.by_level(pic, level, left_ramp, mask & (XX < SPLIT))
    ak.by_level(pic, level, right_ramp, mask & (XX >= SPLIT))


def draw():
    P = ak.Palette(COL, ramps=[AMBER, BLUE, ["gold0", "gold1", "gold2", "cream"]])
    pic = ak.Picture.blank(P, "black")

    face = polygon_mask(BOLT)
    body = face.copy()
    for k in range(1, DEPTH[0] + 1):
        body |= polygon_mask(BOLT, k, k * DEPTH[1] / DEPTH[0])
    extrusion = body & ~face

    # ---- the room: a haze band on the horizon and a glow round the bolt
    sky = YY < HORIZON
    floor = ~sky
    near = distance_transform_edt(~body)
    glow = np.interp(near, [0, 2, 7, 15, 26], [3.0, 2.5, 1.7, 0.9, 0.0])
    haze = np.interp(np.abs(Y1 - HORIZON), [0, 1.5, 4, 8], [2.0, 1.5, 0.7, 0.0])
    level = np.maximum(glow, haze)
    side(pic, ~body, ak.terrace(level, 0.15))

    # ---- the grid floor, brighter toward us
    lines = np.zeros((128, 128), bool)
    for k in range(-12, 13):
        lines |= line_mask((SPLIT + k * 4, HORIZON), (SPLIT + k * 30, 140))
    for z in (1.0, 1.4, 2.0, 2.9, 4.2, 6.2, 9.5):
        y = HORIZON + 52 / z
        lines |= line_mask((0, y), (127, y))
    lines &= floor & ~body
    gl = np.clip(1 + (Y1 > HORIZON + 8) + (Y1 > HORIZON + 22) + (glow > 1.5), 1, 4)
    side(pic, lines, gl)

    # ---- stars in the sky
    for x, y in STARS:
        if not body[y, x]:
            pic.put((XX == x) & (YY == y), "grey")
    ak.glint(pic, 104, 38, 2, tip="cream")
    ak.glint(pic, 20, 64, 2, tip="cream")

    # ---- the bolt: gold face, a gold0 extrusion, a bevel lit from the top left
    pic.put(extrusion, "gold0")
    pic.put(extrusion & (XX >= SPLIT + 6), "b1")
    pic.put(face, "gold1")
    upper = face & (Y1 < 60 + 0.25 * (X1 - 44))
    pic.put(upper, "gold2")
    ak.outline(pic, body, "black", where=~body)
    hi, lo = ak.bevel_contour(pic, face, "cream", "gold0")
    # the sides' light on the bolt's edges: amber on the left, blue on the right
    rim = face & ~ak.erode(face, 1)
    pic.put(rim & ~ak.shift(face, 1, 0) & (XX < SPLIT), "a3")
    pic.put(rim & ~ak.shift(face, -1, 0) & (XX >= SPLIT), "b3")
    pic.put(lo & ~ak.shift(face, -1, 0) & (XX >= SPLIT), "b3")
    ak.glint(pic, 61, 31, 4, tip="gold2")
    ak.glint(pic, 51, 118, 4, tip="cream")

    # ---- the frame's outer 2 px step down into the dark
    edge = (XX < 2) | (XX > 125) | (YY > 125) | (YY < 1)
    pic.put(edge & ~body, "black")

    # ---- the title: house gold, an extrusion and a drop shadow
    m = ak.load_mask(TITLE)
    tx = ak.centred_x(m)
    t = ak.title(pic, m, tx, TITLE_Y, fill=None, rows=TITLE_ROWS, hi=None, lo=None,
                 extrude=dict(dx=1, dy=1, depth=2, colours=TITLE_EXTRUDE), shadow=dict(dx=1, dy=2, colour="black"))
    F = t["face"]
    _, lo_ = ak.bevel_contour(pic, F, "cream", "gold0")
    ak.glint(pic, tx + 1, TITLE_Y + 1, 3, tip="gold2")
    return pic.image()


if __name__ == "__main__":
    import boxart
    print(boxart.save(draw(), OUT))
