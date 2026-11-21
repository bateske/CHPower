"""docs/cart.png, USB HOST's picture in the visual menu (docs/cover-art.md).
`chgame boxart` redraws it; edit this, not the PNG.

THE BRIEF
  Message: the handheld is the host. A CHGame stands in blue light on a
  grid floor and powers a keyboard through one USB-C cable; a key goes
  down and the handheld's screen, the app's KEYS view, says HELLO.

  Composition (the secret-agent look of the apps, as CHSDtoUSB's and
  CHStlView's: a gadget in a dark room, a grid floor, gold chrome
  lettering on calm black), in the app's own blues:

      +----------------------------+
      |       U S B   H O S T      |   the title: gold chrome on calm black
      |          .--------.        |   black void; a blue glow hugs the device
      |          | HELLO_ |        |   the screen: the KEYS view, the text typed
      |       __/| + oo   |        |   the cable leaves its left side, power
      |      /   '--------'        |   pulses running down it to the keyboard
      |  [#][#][*][#][#][#][#][#]  |   a backlit keyboard across the foreground,
      |  [#][#][#][#][#][#][#][#]  |   the pressed key amber with a star
      +----------------------------+

  - Focal point: the screen, the brightest and most contrasty shape: a
    status bar, the tabs (KEYS lit), the text field's corner brackets, HELLO
    in pale capitals (3x5, stamped square to the picture: stepped along the
    screen's slight slant the letters broke) and the amber cursor. The eye
    goes from the title to the screen, down the cable to the amber key and
    its star, and back up.
  - The camera looks down at about 30 degrees, so the keys' rows part
    (lower, they merged into stripes); the horizon is above the picture, and
    the grid fades into the dark with distance, so the title sits on black.
  - The device (a CHGame handheld, the same model as CHSDtoUSB's, a fifth
    larger) stands on the floor turned 20 degrees, its left side, with the
    USB-C port, toward the keyboard: gunmetal faces, a sheen in the front's
    top-left corner, a grey lip along the top, a blue rim of backlight down
    its right edge, the cross pad, red buttons, start/select and the
    speaker stamped by hand.
  - The keyboard: a dark deck, 13 x 3 gunmetal keycaps and a bottom row
    with the space bar (each a rounded box: a grey back lip toward the key
    light, an m1 top, an m0 front), the deck's blue backlight in every gap
    (it is powered: the light says so), the pressed key amber with a cream
    lip, its light spilling into the gaps round it, a four-point star (b3
    tips) on its back corner. It runs out of the frame at the left and the
    bottom: the foreground.
  - The cable: dark rubber with a lit top line, out of the device's left
    side, down to the floor and into the keyboard's back, a plug at each
    end. Two pulses (a cream core, an amber tail) run along it toward the
    keyboard: the handheld gives the power.
  - Depth: the keyboard in front, the device in the middle, the grid
    fading behind; the device's shadow on the floor to the right.
  - Light: the house key from the top left; the device's blue glow as a
    back light; the screen and the backlit keys as their own light.

PALETTE (11 own + cream, grey, black, red) - USB HOST's screen palette
  b0 b1 b2 b3 b4   the app's blues (BG, GRID/PANEL, MID, LIVE, PALE) on one
                   ramp from black to cream: the glow, the grid, the
                   keyboard's backlight, the screen, the star's tips
  m0 m1            gunmetal (black, m0, m1, grey, cream): the device, the
                   keycaps, the cable, the plugs
  am               the app's amber, for what moves: the pressed key, the
                   cursor, the power pulses
  gold0 gold1 gold2  the title's own: nothing else uses them
  red              the buttons

FONT  title.txt (USB HOST): CHARSET-DNS_FONT 5 from the bmf collection (a
      `?` face: author and terms not stated), the face the other apps'
      titles use, so they match. Set at the font's own size (95 px), the S
      redrawn as STL VIEWER's clean squared S, the word space closed to 7
      px. The sister apps' gold chrome: bands of gold2 and gold1 (sky,
      horizon, ground) with dithered seams on the wide strokes, a
      gold0-to-blue extrusion (black in the counters), a black outline and
      drop shadow, a cream and gold0 bevel on the outer contour, and one
      cream glint on the U's top-left corner.
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[9] / "tools"))     # the repository's tools/
import numpy as np  # noqa: E402
import artkit as ak  # noqa: E402
from artkit import render3d as R  # noqa: E402

TITLE = HERE / "art" / "title.txt"
TITLE_Y = 4
TITLE_ROWS = ["gold2"] * 6 + ["gold1"] * 3 + ["gold2"] * 2 + ["gold1"] * 5   # chrome: sky, horizon, ground
TITLE_SEAMS = ((5, "gold1"), (11, "gold2"))     # rows (of the face) dithered half into the next band
TITLE_EXTRUDE = ["gold0", "b1"]
TITLE_GLINT = (1, 1)                            # the key glinting on the U's top-left corner (from the title's)

COL = {
    # USB HOST's own screen blues (CHUSBHost.ino's PALETTE): blue-black,
    # grid, mid, live teal-blue, pale; its amber for what moves
    "b0": "#020A1E", "b1": "#0A2A4C", "b2": "#2A6C92", "b3": "#4CCCE4", "b4": "#C4F6FF",
    "m0": "#141A28", "m1": "#3A4858",
    "am": "#FFB430",
    "gold0": "#7A3408", "gold1": "#E4991C", "gold2": "#FFE27A",
}
BLUE = ["black", "b0", "b1", "b2", "b3", "b4", "cream"]
METAL = ["black", "m0", "m1", "grey", "cream"]

YY, XX = np.mgrid[0:128, 0:128]
X1, Y1 = XX + 0.5, YY + 0.5

# ---- the scene (world: x right, y up, z toward us; the floor at y 0) ---------------------

CAM = dict(pos=(0.0, 3.0, 3.6), target=(0.2, 0.45, -0.6), fov=46, shift=(0, 16))
DEV = dict(pos=(0.55, 1.02, -1.1), turn=20, half=(0.62, 0.85, 0.17), r=0.09, scale=1.2)
SCREEN = (-0.47, 0.47, 0.10, 0.68)           # x0, x1, y0, y1 on the front (local)
BEZEL = 0.06
PAD = (-0.30, -0.42)                         # the cross pad's centre (local)
BUTTONS = [(0.04, -0.44), (0.36, -0.24)]     # the red buttons' centres
PILLS = [(-0.10, -0.70), (0.10, -0.70)]      # start / select
GRILLE = (0.36, -0.66)                       # the speaker's slots
PORT = (-0.45, 0.0)                          # the USB-C port on its left side (local y, z)
KB = dict(pos=(-0.10, 0.0, 0.55), turn=-6, half=(1.55, 0.05, 0.52), r=0.05)
PITCH = (0.228, 0.235)                       # the keys' pitch across and back to front
COLS, ROWS = 13, 3                           # the letter keys; a row with the space bar under them
KEYCAP = (0.086, 0.03, 0.088)                # a key's half size
SPACE = 2                                    # the space bar takes the bottom row's middle 2 * SPACE + 1 keys
PRESSED = (1, 4)                             # the key being pressed (row, column)
STAR = (3, 3, 3, 2)                          # its star's arms: left, right, up, down
KB_PORT = -1.15                              # where the cable plugs into the keyboard's back (local x)
CABLE_R = 0.065
KEY = np.array([-0.55, 0.75, 0.45])
KEY = KEY / np.linalg.norm(KEY)
GRID = 0.5                                   # the floor's grid pitch
GLOW = (0, 5, 14, 2.1)                       # the blue light round the device: -, half-way (px), reach (px), level
CALM = (24, 34)                              # rows over which that glow dies away under the title

TYPED = "HELLO"


def key_centres():
    x0 = -(COLS - 1) / 2 * PITCH[0]
    z0 = -ROWS / 2 * PITCH[1] - 0.02
    return x0, z0


def keys_sdf(p):
    """The keycaps: a grid of rounded boxes (one distance by the nearest
    cell); in the bottom row the space bar, a key either side of it
    (keyboard's frame, the keys' mid-height at y 0)."""
    x0, z0 = key_centres()
    mid = (COLS - 1) / 2
    col = np.clip(np.round((p[:, 0] - x0) / PITCH[0]), 0, COLS - 1)
    row = np.clip(np.round((p[:, 2] - z0) / PITCH[1]), 0, ROWS)
    q = p - np.stack([x0 + col * PITCH[0], np.zeros(len(p)), z0 + row * PITCH[1]], 1)
    d = R.box(KEYCAP, 0.012)(q)
    d = np.where((row == ROWS) & (np.abs(col - mid) <= SPACE), 9.0, d)
    sp = p - np.array([0.0, 0.0, z0 + ROWS * PITCH[1]])
    bar = R.box(((SPACE + 0.5) * PITCH[0] - (PITCH[0] / 2 - KEYCAP[0]), KEYCAP[1], KEYCAP[2]), 0.012)(sp)
    return np.minimum(d, bar)


def catmull(pts, n):
    """A Catmull-Rom curve through the points, n samples a span."""
    pts = [np.asarray(p, float) for p in pts]
    pts = [2 * pts[0] - pts[1]] + pts + [2 * pts[-1] - pts[-2]]
    out = []
    for i in range(1, len(pts) - 2):
        a, b, c, d = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        for t in np.linspace(0, 1, n, endpoint=False):
            out.append(0.5 * (2 * b + (c - a) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (3 * b - a - 3 * c + d) * t ** 3))
    out.append(pts[-2])
    return np.array(out)


def camera():
    return R.Camera(CAM["pos"], CAM["target"], fov=CAM["fov"], shift=CAM["shift"])


def scene():
    hx, hy, hz = DEV["half"]
    rot = R.rot_y(DEV["turn"])
    pos = np.asarray(DEV["pos"], float)
    S = DEV["scale"]
    body = R.prim(R.box((hx, hy, hz), DEV["r"]), 0)
    krot = R.rot_y(KB["turn"])
    kx, ky, kz = KB["half"]
    kpos = np.asarray(KB["pos"], float) + np.array([0, ky, 0])
    deck = R.prim(R.box(KB["half"], KB["r"]), 1)
    keys = R.xf(R.prim(keys_sdf, 2), (0, ky + KEYCAP[1] - 0.01, 0.0))
    # the cable: out of the port in the device's left side, down to the
    # floor and across it to the keyboard's back edge
    p0 = pos + S * (rot @ np.array([-hx, PORT[0], PORT[1]]))
    p1 = pos + S * (rot @ np.array([-hx - 0.14, PORT[0], PORT[1]]))
    p2 = pos + S * (rot @ np.array([-hx - 0.34, -hy, PORT[1] + 0.12])) + np.array([0, CABLE_R, 0])
    k_port = kpos + krot @ np.array([KB_PORT, 0.0, -kz])
    k_out = kpos + krot @ np.array([KB_PORT, 0.0, -kz - 0.14])
    k_out[1] = CABLE_R
    mid = (p2 + k_out) / 2 + np.array([0.15, 0, -0.55])
    mid[1] = CABLE_R
    cab = catmull([p0, p1, p2, mid, k_out], 6)
    caps = [R.prim(R.capsule(tuple(a), tuple(b), CABLE_R), 3) for a, b in zip(cab[:-1], cab[1:])]
    plugs = [R.prim(R.capsule(tuple(p0 + rot @ np.array([-0.02, 0, 0])), tuple(p1), CABLE_R * 1.7), 4),
             R.prim(R.capsule(tuple(k_out), tuple(k_port + krot @ np.array([0, 0, -0.02])), CABLE_R * 1.7), 4)]
    node = R.U(R.xf(body, pos, rot, S), R.xf(R.U(deck, keys), kpos, krot), *caps, *plugs)
    return dict(node=node, rot=rot, pos=pos, krot=krot, kpos=kpos, cable=cab)


def geometry(cam, sc):
    """Per pixel: the material most of its rays hit (-1 none), and at the
    pixel's centre the normal and the hit point; where its ray meets the
    floor, and the shadow the scene throws there."""
    cv = ak.Canvas("#000000")
    mats = [R.Mat("#FFFFFF", spec=0.0)] * 5
    out = R.render(cv, sc["node"], cam, mats, ss=2, shadows=False, ao=False)
    mat = R.majority(out, cv.s)
    one = ak.Canvas("#000000", ss=1)
    o1 = R.render(one, sc["node"], cam, mats, lights=[(tuple(KEY), "#FFFFFF", 1.0)],
                  ambient="#000000", shadows=False, ao=False)
    o, d = cam.rays(X1.reshape(-1).astype(np.float64), Y1.reshape(-1).astype(np.float64))
    dep = o1["depth"].reshape(-1)
    z = np.where(np.isfinite(dep), dep, 0)
    p = (o + d * z[:, None]).reshape(128, 128, 3)
    n = o1["normal"].astype(np.float64)
    n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-9)
    dn = d[:, 1]
    tf = np.where(dn < -1e-6, -o[:, 1] / np.where(dn < -1e-6, dn, -1), np.inf)
    shadow = ak.px_mean(R.ground(cv, sc["node"], cam, y=0.0, light=tuple(KEY), ss=2, k=5.0, ao=False), cv.s)
    return dict(mat=mat, p=p, n=n, tf=tf.reshape(128, 128), shadow=shadow)


def smooth(a, b, v):
    t = np.clip((np.asarray(v, float) - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


DIM = {"cream": "grey", "grey": "m1", "m1": "m0", "m0": "black", "b4": "b3", "b3": "b2", "b2": "b1", "b1": "b0",
       "b0": "black", "am": "b2", "red": "m1"}


def dim(pic, mask, n=1):
    """Every colour in `mask` n steps down its ramp toward black."""
    lut = np.arange(256, dtype=pic.idx.dtype)
    for a, b in DIM.items():
        lut[pic.pal[a]] = pic.pal[b]
    for _ in range(n):
        pic.idx[mask] = lut[pic.idx[mask]]


def line_mask(pts):
    m = np.zeros((128, 128), bool)
    for x, y in ak.line_px(pts):
        if 0 <= x < 128 and 0 <= y < 128:
            m[y, x] = True
    return m


def cross_stamp():
    """The cross pad: 3-px arms, slate, its top and left edges grey, a black
    lip under and right of it."""
    C = np.zeros((7, 7), bool)
    C[:, 2:5] = True
    C[2:5, :] = True
    rows = []
    for y in range(8):
        r = ""
        for x in range(8):
            inside = y < 7 and x < 7 and C[y, x]
            if inside:
                lit = (y == 0 or not C[y - 1, x]) or (x == 0 or not C[y, x - 1])
                r += "h" if lit else "m"
            else:
                up_ = y > 0 and x < 7 and C[y - 1, x]
                lf_ = x > 0 and y < 7 and C[y, x - 1]
                r += "k" if (up_ or lf_) else "."
        rows.append(r)
    return rows


BUTTON = [".rrrr.",
          "rcrrrr",
          "rrrrrb",
          "rrrrrb",
          "rrrrbb",
          ".bbbb."]
SPEAKER = ["...k.k.k",
           "..k.k.k.",
           ".k.k.k..",
           "k.k.k..."]
# 3x5 capitals for what was typed
GLYPHS = {
    "H": ["#.#", "#.#", "###", "#.#", "#.#"],
    "E": ["###", "#..", "##.", "#..", "###"],
    "L": ["#..", "#..", "#..", "#..", "###"],
    "O": ["###", "#.#", "#.#", "#.#", "###"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
}


# ---- the picture ------------------------------------------------------------------------

def draw():
    P = ak.Palette(COL, ramps=[BLUE, METAL, ["black", "am", "cream"], ["gold0", "gold1", "gold2", "cream"]])
    pic = ak.Picture.blank(P, "black")
    cam = camera()
    sc = scene()
    G = geometry(cam, sc)
    rot, pos, krot, kpos = sc["rot"], sc["pos"], sc["krot"], sc["kpos"]
    hx, hy, hz = DEV["half"]
    dev = G["mat"] == 0
    deck = G["mat"] == 1
    keys = G["mat"] == 2
    cab = G["mat"] == 3
    plug = G["mat"] == 4
    kb = deck | keys
    things = G["mat"] >= 0
    keep = np.zeros((128, 128), bool)                      # deliberate glints: despeckling leaves them

    def dev_at(lx, ly, lz=0.0):
        return np.array(cam.project(pos + DEV["scale"] * (rot @ np.array([lx, ly, lz], float))))

    def kb_at(lx, ly, lz=0.0):
        return np.array(cam.project(kpos + krot @ np.array([lx, ly, lz], float)))

    # ---- the room: black void, a blue glow hugging the device and the
    # keyboard (calm under the title), the floor hazing over into the dark
    floor = np.isfinite(G["tf"]) & ~things
    dist = np.where(np.isfinite(G["tf"]), G["tf"], 99.0)
    near = ak.distance_px(dev, GLOW[2])
    lv = np.interp(near, [0, GLOW[1], GLOW[2]], [GLOW[3], GLOW[3] - 0.7, 0.0]) * smooth(CALM[0], CALM[1], Y1)
    lv = np.maximum(lv, np.interp(ak.distance_px(kb, 6), [0, 2, 6], [1.2, 0.8, 0.0]))
    lv = np.where(floor, np.maximum(np.interp(dist, [3.0, 5.0, 7.0, 9.0], [0.9, 0.7, 0.35, 0.0]), lv), lv)
    shade = G["shadow"] * floor
    bg = np.clip(lv - shade * 1.6, 0, 4.0)
    plateau = np.floor(ak.terrace(bg, 0.12) + 0.5)
    ak.by_level(pic, ak.terrace(bg, 0.12), BLUE, ~things)

    # ---- the floor's grid: 1-px lines one level over the ground, fading
    # into the dark with distance
    lines = np.zeros((128, 128), bool)
    zc = CAM["pos"][2]
    for k in range(-14, 16):
        lines |= line_mask([cam.project((k * GRID, 0.0, zc - 0.6)), cam.project((k * GRID, 0.0, -9.0))])
    for j in range(0, 18):
        z_ = zc - 0.9 - j * GRID
        lines |= line_mask([cam.project((-14.0, 0.0, z_)), cam.project((14.0, 0.0, z_))])
    lines &= floor & (dist < 8.5)
    glv = np.clip(np.maximum(plateau + 1, 1 + (dist < 5.5) + (dist < 4.2)) - (shade > 0.5), 0, 3)
    glv = np.where(dist > 7.0, np.minimum(glv, plateau + 1), glv)
    for k, c in ((1, "b0"), (2, "b1"), (3, "b2")):
        pic.put(lines & (glv == k), c)

    # ---- the keyboard: a dark deck, gunmetal keycaps lit from the top
    # left, blue backlight between them; the key being pressed in amber
    kl = (G["p"] - kpos) @ krot
    kn = G["n"] @ krot
    x0, z0 = key_centres()
    kcol = np.round((kl[..., 0] - x0) / PITCH[0])
    krow = np.round((kl[..., 2] - z0) / PITCH[1])
    top = keys & (kn[..., 1] > 0.7)
    pic.put(deck, "m0")
    pic.put(deck & (kn[..., 1] > 0.7), "b2")                              # the backlight between the keys
    rim = deck & (kn[..., 1] > 0.7) & ((np.abs(kl[..., 0]) > KB["half"][0] - 0.10) | (np.abs(kl[..., 2]) > KB["half"][2] - 0.08))
    pic.put(rim, "m1")
    pic.put(deck & (kn[..., 2] > 0.5) & (kn[..., 1] < 0.7), "m0")          # the front edge
    pic.put(keys, "m0")                                                   # the keys' sides and fronts
    pic.put(top, "m1")
    top_edge = top & ~ak.shift(top, 0, 1)                                 # the back lip of each key, toward the key light
    pic.put(top_edge, "grey")
    K = keys & (krow == PRESSED[0]) & (kcol == PRESSED[1])
    pic.put(ak.dilate(K, 1) & ~K & kb & ~top, "b3")                      # its light spilling into the gaps
    pic.put(K, "am")
    pic.put(K & ~ak.shift(K, 0, 1), "cream")
    keep |= K
    ak.outline(pic, kb, "black", where=~things)

    # ---- the cable and its plugs: dark rubber with a lit top line; power
    # pulses running from the device to the keyboard
    cl = np.clip(G["n"] @ KEY, 0, 1)
    ak.by_level(pic, np.round(0.7 + 1.6 * cl ** 1.5), METAL, cab)
    ak.by_level(pic, np.round(1.6 + 2.0 * cl), METAL, plug)
    ak.outline(pic, cab | plug, "black", where=floor)
    cpts = sc["cable"]
    seg = np.linalg.norm(np.diff(cpts, axis=0), axis=1)
    arc = np.concatenate([[0], np.cumsum(seg)])
    hp = G["p"][cab]
    nr = np.argmin(((hp[:, None, :] - cpts[None, :, :]) ** 2).sum(-1), axis=1)
    s_px = np.zeros((128, 128))
    s_px[cab] = arc[nr]
    for s0 in (arc[-1] * 0.45, arc[-1] * 0.80):
        core = cab & (np.abs(s_px - s0) < 0.05)
        trail = cab & (s_px < s0 - 0.05) & (s_px > s0 - 0.20)
        pic.put(trail, "am")
        pic.put(core, "cream")
        keep |= core

    # ---- the device: gunmetal, each face flat by how it meets the key
    loc = (G["p"] - pos) @ rot / DEV["scale"]
    ln = G["n"] @ rot
    lx, ly, lz = loc[..., 0], loc[..., 1], loc[..., 2]
    ax = np.argmax(np.abs(ln), -1)
    srt = np.sort(np.abs(ln), -1)
    flat = srt[..., 1] < 0.3
    front = dev & flat & (ax == 2) & (ln[..., 2] > 0)
    left = dev & flat & (ax == 0) & (ln[..., 0] < 0)
    rnd = dev & ~(front | left)
    nx_, ny_, nz_ = ln[..., 0], ln[..., 1], ln[..., 2]
    fu = np.clip((lx + hx) / (2 * hx), 0, 1)
    fv = np.clip((hy - ly) / (2 * hy), 0, 1)
    dl = np.zeros((128, 128))
    dl[front] = np.interp(fu * 0.5 + fv, [0.0, 0.16, 0.30], [2.0, 1.5, 1.0])[front]
    dl[left] = np.interp(fv, [0.0, 0.45, 0.62, 1.0], [3.0, 3.0, 2.0, 2.0])[left]
    edge = np.full((128, 128), 1.0)
    edge[ny_ > 0.3] = 3.0
    edge[(np.abs(ny_) <= 0.3) & (nx_ < -0.3) & (nz_ > 0.3)] = 2.0
    edge[(nx_ > 0.3) & (ny_ > -0.3)] = 1.0
    edge[ny_ < -0.3] = 0.5
    dl[rnd] = edge[rnd]
    ak.by_level(pic, ak.terrace(dl, 0.25), METAL, dev)
    ring_d = ak.dilate(dev, 1) & ~dev & ~things
    pic.put(ring_d & (ak.shift(dev, 1, 0) | ak.shift(dev, 0, 1)), "black")
    right_rim = dev & ~ak.shift(dev, -1, 0) & ak.shift(dev, 1, 0) & (XX > dev_at(0, 0)[0])
    pic.put(right_rim & (Y1 > dev_at(hx, hy)[1] + 3), "b2")

    # the screen: the KEYS view in whole pixels (status bar, tabs, the text
    # field with what was typed and the amber cursor, the modifier row)
    sx0, sx1, sy0, sy1 = SCREEN
    bez = front & (lx > sx0 - BEZEL) & (lx < sx1 + BEZEL) & (ly > sy0 - BEZEL) & (ly < sy1 + BEZEL)
    scr = front & (lx > sx0) & (lx < sx1) & (ly > sy0) & (ly < sy1)
    pic.put(bez, "black")
    u = (lx - sx0) / (sx1 - sx0)
    v = (sy1 - ly) / (sy1 - sy0)
    pic.put(scr, "b0")
    H = int(round(dev_at(sx0, sy0, hz)[1] - dev_at(sx0, sy1, hz)[1]))
    Wd = int(round(dev_at(sx1, (sy0 + sy1) / 2, hz)[0] - dev_at(sx0, (sy0 + sy1) / 2, hz)[0]))
    r = np.floor(v * H).astype(int)
    c = np.floor(u * Wd).astype(int)

    def cell(r0, r1, c0, c1, colour, extra=True):
        pic.put(scr & (r >= r0) & (r <= r1) & (c >= c0) & (c <= c1) & extra, colour)
    cell(0, 2, 0, Wd - 1, "b1")                                    # the status bar
    cell(1, 1, 1, 2, "b3")
    cell(1, 1, 4, Wd // 2, "b3")
    cell(1, 1, Wd - 5, Wd - 2, "b2")
    cell(4, 4, 1, 4, "b3")                                         # the tabs, KEYS lit
    cell(4, 4, 6, 9, "b1")
    cell(4, 4, 11, 13, "b1")
    # the text field: its corner brackets, the text, the cursor
    f0, f1 = 6, H - 5
    for rr, cc, dr, dc in ((f0, 1, 1, 1), (f0, Wd - 2, 1, -1), (f1, 1, -1, 1), (f1, Wd - 2, -1, -1)):
        cell(rr, rr, min(cc, cc + dc), max(cc, cc + dc), "b2")                # an L at each corner
        cell(min(rr, rr + dr), max(rr, rr + dr), cc, cc, "b2")
    tx, ty = 3, f0 + 2
    # the text stamped square to the picture (the screen's slant is a pixel
    # or two across it: letters stepped along it would break)
    ox, oy = dev_at(sx0 + (tx + 0.5) / Wd * (sx1 - sx0), sy1 - (ty + 0.5) / H * (sy1 - sy0), hz)
    ox, oy = int(np.floor(ox)), int(np.floor(oy))
    text = np.zeros((128, 128), bool)
    for k, ch in enumerate(TYPED):
        for j, row_ in enumerate(GLYPHS[ch]):
            for i, b_ in enumerate(row_):
                if b_ == "#":
                    text[oy + j, ox + 4 * k + i] = True
    pic.put(text & scr, "b4")
    cx_ = ox + 4 * len(TYPED)
    pic.put(scr & (YY >= oy) & (YY <= oy + 4) & (XX >= cx_) & (XX <= cx_ + 1), "am")
    cell(H - 3, H - 2, 1, Wd - 2, "b1")                            # the modifiers' row
    keep |= scr

    def stamp(local, rows, key_):
        x_, y_ = dev_at(*local, hz)
        h_, w_ = len(rows), len(rows[0])
        x0_, y0_ = int(round(x_ - w_ / 2)), int(round(y_ - h_ / 2))
        ak.patch(pic, x0_, y0_, rows, key_)
        for j, r_ in enumerate(rows):
            for i, ch in enumerate(r_):
                if ch in key_ and 0 <= y0_ + j < 128 and 0 <= x0_ + i < 128:
                    keep[y0_ + j, x0_ + i] = True
    stamp(PAD, cross_stamp(), {"h": "grey", "m": "m1", "d": "m0", "k": "black"})
    for b_ in BUTTONS:
        stamp(b_, BUTTON, {"r": "red", "c": "cream", "b": "m1", "k": "black"})
    for p_ in PILLS:
        stamp(p_, ["mmm", "kkk"], {"m": "m1", "k": "black"})
    stamp(GRILLE, SPEAKER, {"k": "black"})

    ak.despeckle(pic, need=5, keep=keep)
    ak.despeckle(pic, need=3, keep=keep, within=ak.dilate(things, 1), passes=2)
    ak.despeckle(pic, need=2, keep=keep, passes=2)
    frame = (XX < 2) | (XX > 125) | (YY > 125)
    dim(pic, frame, 2)
    ak.despeckle(pic, need=2, within=ak.dilate(frame, 1))

    # ---- the keystroke: a star on the pressed key's back corner
    ky_, kx_ = np.nonzero(K)
    gx, gy = int(kx_.min()) + 1, int(ky_.min())
    ak.glint(pic, gx, gy, 3, tip="b3", arms=STAR)

    # ---- the title: gold chrome (the sister apps' treatment), a
    # gold0-to-blue extrusion, a black drop shadow, a bevel, one glint
    ck = ak.checker()
    m = ak.load_mask(TITLE)
    tx0 = ak.centred_x(m)
    t = ak.title(pic, m, tx0, TITLE_Y, fill=None, rows=TITLE_ROWS, hi=None, lo=None,
                 extrude=dict(dx=1, dy=1, depth=2, colours=TITLE_EXTRUDE), shadow=dict(dx=1, dy=2, colour="black"))
    F = t["face"]
    wide = ak.thick(F, 2)
    for r_, c_ in TITLE_SEAMS:
        pic.put(F & wide & (YY == TITLE_Y + r_) & ck, c_)
    ak.despeckle(pic, need=3, within=t["extrude"] | t["outline"])     # lone extrusion pixels in the counters
    # the counters between the bars stay open: the back depth goes black there
    ext = t["extrude"] & pic.where(TITLE_EXTRUDE[-1])
    above = ak.shift(F, 0, 1) | ak.shift(F, 0, 2)
    below = ak.shift(F, 0, -1) | ak.shift(F, 0, -2)
    pic.put(ext & above & below, "black")
    ak.bevel_contour(pic, F, "cream", "gold0")
    ak.despeckle(pic, need=3, within=t["extrude"] | t["outline"] | (F & ~ak.erode(F, 1)))   # no lone pixels in the counters
    ak.glint(pic, tx0 + TITLE_GLINT[0], TITLE_Y + TITLE_GLINT[1], 3, tip="gold2")
    return pic.image()


if __name__ == "__main__":
    import boxart
    boxart.save(draw(), HERE.parent / "docs" / "cart.png")
