r"""docs/cart.png, PD TRIGGER's picture in the visual menu
(docs/cover-art.md). `chgame boxart` redraws it; edit this, not the PNG.

THE BRIEF
  Message: a charger becomes a bench supply. A CHGame stands in a dark room
  with its trigger page on the screen (20 V, the scope stepping up to it),
  a USB-C cable feeds it from a charger brick at the back, and its output
  leads (CN1) run across the floor to a lamp, which blazes: the power the
  handheld asked the charger for, switched on.

  Composition (the secret-agent look of the apps, as CHSDtoUSB's and
  CHStlView's: a gadget in a dark room, a grid floor, gold chrome lettering),
  but in the app's own warm amber instead of the reader's green:

      +----------------------------+
      |  P D   T R I G G E R       |   the title: gold chrome on calm black
      |              .--------.    |   black void; an amber glow hugs the device
      |   [brick]    |[20V ~~]|    |   the charger at the back, its cable to the
      |  ( * )   \   |[scope ]|    |   device; the lamp's glass ablaze, a star
      |  ( | )    \  |[ + oo ]|    |   on its filament; the grid in amber
      |  [===]=====leads======]    |   the red and black leads from CN1 to the lamp
      +----------------------------+

  - Focal point: the lamp, front left: the brightest thing, a pear of glass
    lit from inside (an amber core, a2 at its rim, the key's a3 on its top
    left, a cream specular), a cream filament with a four-point star whose
    arms reach out of the glass, a brass screw (threads in rust, the key
    down its left), a dark socket on the floor. Its light is the second
    light: a glow in the air round it, a pool on the floor, and an amber rim
    down the device's side that faces it.
  - The device (a CHGame handheld, rendered in 3D and painted face by face,
    as the other apps' covers) stands middle right turned 34 degrees, its
    front gunmetal. The screen is the app's trigger page in whole pixels:
    the status bar, the cyan current bar, the set voltage, 20V, big in the
    live amber, the ON and amps readouts, the scope (a dotted grid, the
    volts stepping up in amber, the amps following in cyan) and the SET and
    LIMIT boxes. The cross pad, red buttons, start/select and speaker are
    stamped by hand.
  - The charger: a dark brick at the back between the two, its lit bolt
    emblem in amber, its plug in its side; the USB-C cable runs from it to
    the device, two pulses (a cream core, an amber tail) on it running
    toward the device. The leads leave CN1, a black header low on the
    device's left side, cross the floor in front and go into the socket;
    one pulse runs along the red to the lamp.
  - Depth: the lamp in front, the device in the middle, the charger far
    back; the grid's lines brighten toward us and fade into a band of haze
    along the horizon. The frame's outer 2 px step down into the dark.
  - Light: the house key from the top left, the lamp as the warm second
    light, the glow round the device as a back light.

PALETTE (11 own + cream, grey, black, red)
  a0 a1 a2 a3      the amber ramp (black, a0..a3, cream), the app's live
                   amber: the glow, haze, grid, the lamp, the screen, pulses
  m0 m1            warm gunmetal (black, m0, m1, grey, cream): the device,
                   the charger, the cable, the socket, the black lead
  cyan             the app's amps colour: the screen's current readouts
  br0              rust: the red lead's and buttons' shade, the title's
                   back depth
  gold0 gold1 gold2  the title's own: nothing else uses them
  red              the red lead, the buttons

FONT  title.txt (PD TRIGGER): CHARSET-DNS_FONT 5 from the bmf collection
      (author and terms not stated: a `?` face), the face of the other apps'
      titles, at its own size, the word space closed to 7 px by hand as STL
      VIEWER's. Its chrome as STL VIEWER's: bands of gold2 and gold1, a
      gold0-to-rust extrusion, a black drop shadow, the 1-2 px pockets
      closed in black, a cream and gold0 bevel, a cream glint on the P.
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
TITLE_GLINT = (1, 1)                         # the key glinting on the P's corner (from the title's top-left)
TITLE_EXTRUDE = ["gold0", "br0"]

COL = {
    "a0": "#1C0A04", "a1": "#4E2410", "a2": "#B4641C", "a3": "#FFC040",
    "m0": "#1E1814", "m1": "#4A4038",
    "cyan": "#38D4F8",
    "br0": "#62200E",
    "gold0": "#7A3408", "gold1": "#E4991C", "gold2": "#FFE27A",
}
AMBER = ["black", "a0", "a1", "a2", "a3", "cream"]
METAL = ["black", "m0", "m1", "grey", "cream"]
RED = ["black", "br0", "red", "cream"]

YY, XX = np.mgrid[0:128, 0:128]
X1, Y1 = XX + 0.5, YY + 0.5

# ---- the scene (world: x right, y up, z toward us; the floor at y 0) ---------------------

CAM = dict(pos=(0.30, 1.22, 3.7), target=(0.30, 0.95, 0.0), fov=40, shift=(2, 6))
DEV = dict(pos=(0.80, 0.85, -0.40), turn=34, half=(0.62, 0.85, 0.17), r=0.09)
SCREEN = (-0.47, 0.47, 0.10, 0.68)           # x0, x1, y0, y1 on the front (local)
BEZEL = 0.06
CN1 = (-0.52, 0.05)                          # the output's header on the left side (local y, z)
PAD = (-0.30, -0.42)
BUTTONS = [(0.04, -0.44), (0.36, -0.24)]
PILLS = [(-0.10, -0.70), (0.10, -0.70)]
GRILLE = (0.36, -0.66)
PORT = (0.25, -0.70)                         # the USB-C port in the back (local x, y)
CABLE_R = 0.05
CABLE = [(0.22, 0.05, -1.10), (-0.05, 0.05, -3.90)]   # its path on the floor to the charger (world): from, a quadratic's control
CHARGER = dict(pos=(-0.55, 0.0, -6.60), turn=24, half=(0.40, 0.30, 0.40), r=0.07)
BULB = dict(pos=(-0.42, 0.0, 0.40), scale=0.74)
GLOBE = (0.36, 0.54)                         # the globe's radius and centre above the screw (of the lamp's scale)
LEAD_R = 0.026
KEY = np.array([-0.55, 0.75, 0.45])
KEY = KEY / np.linalg.norm(KEY)
GRID = 0.5

GLOW = (0, 4, 12, 1.45)
CALM = (34, 46)
HAZE = ([6.0, 11.0, 28.0, 90.0], [0.0, 0.5, 0.95, 1.35])
PULSES = [(1.0, 1), (2.0, 1)]                # power on the USB-C cable (arc length from the device, the tail's side)
LEAD_PULSES = [(0.72, -1)]                    # and on the red lead (from CN1), running to the lamp

DEVICE, GLASS, CAB, PLUG, BRICK, SCREW, SOCKET, REDL, BLKL = range(9)


def catmull(pts, n):
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


def bulb_profile(s):
    """The lamp's glass in its own frame (radius, y), from the screw's top:
    a short neck flaring into the globe."""
    pts = [(0.0, 0.0), (0.13, 0.0), (0.14, 0.08)]
    for a in np.linspace(-52, 90, 24):
        t = np.radians(a)
        pts.append((GLOBE[0] * np.cos(t), GLOBE[1] + GLOBE[0] * np.sin(t)))
    pts.append((0.0, GLOBE[1] + GLOBE[0]))
    return [(r * s, y * s) for r, y in pts]


def scene():
    hx, hy, hz = DEV["half"]
    rot = R.rot_y(DEV["turn"])
    pos = np.asarray(DEV["pos"], float)
    body = R.prim(R.box((hx, hy, hz), DEV["r"]), DEVICE)
    # the USB-C cable: from the charger at the back, across the floor, into
    # the port in the device's back
    p0 = pos + rot @ np.array([PORT[0], PORT[1], -hz])
    p1 = pos + rot @ np.array([PORT[0], PORT[1], -hz - 0.14])
    p2 = pos + rot @ np.array([PORT[0] - 0.10, -hy + CABLE_R, -hz - 0.35])
    crot = R.rot_y(CHARGER["turn"])
    cpos = np.asarray(CHARGER["pos"], float)
    bw, bh, bd = CHARGER["half"]
    port = cpos + crot @ np.array([bw + 0.01, 0.55 * bh, 0.0])
    plug_out = cpos + crot @ np.array([bw + 0.16, 0.55 * bh, 0.0])
    a_, c_ = (np.array(q, float) for q in CABLE)
    e_ = plug_out.copy()
    e_[1] = CABLE_R
    bend = [(1 - t) ** 2 * a_ + 2 * (1 - t) * t * c_ + t * t * e_ for t in np.linspace(0, 1, 10)]
    cab = np.concatenate([catmull([p0, p1, p2, bend[0]], 6)[:-1], np.array(bend), [plug_out]])
    caps = [R.prim(R.capsule(tuple(a), tuple(b), CABLE_R), CAB) for a, b in zip(cab[:-1], cab[1:])]
    plug = R.prim(R.capsule(tuple(plug_out), tuple(port), CABLE_R * 1.7), PLUG)
    brick = R.xf(R.prim(R.box((bw, bh, bd), CHARGER["r"]), BRICK), cpos + np.array([0, bh, 0]), crot)
    # the lamp: a socket on the floor, the screw, the glass
    s = BULB["scale"]
    bpos = np.asarray(BULB["pos"], float)
    sock = R.xf(R.prim(R.cylinder(0.24 * s, 0.09 * s, 0.03 * s), SOCKET), bpos + np.array([0, 0.09 * s, 0]))
    screw = R.xf(R.prim(R.cylinder(0.14 * s, 0.12 * s, 0.02 * s), SCREW), bpos + np.array([0, 0.30 * s, 0]))
    glass = R.xf(R.prim(R.lathe(bulb_profile(s)), GLASS), bpos + np.array([0, 0.42 * s, 0]))
    # the output's leads: out of CN1 on the left side, down to the floor,
    # across to the socket's terminals
    leads, lpaths = [], []
    for k, (mat, dz) in enumerate(((REDL, 0.05), (BLKL, -0.05))):
        q0 = pos + rot @ np.array([-hx + 0.02, CN1[0], CN1[1] + dz])
        q1 = pos + rot @ np.array([-hx - 0.16, CN1[0] - 0.05, CN1[1] + dz])
        q2 = pos + rot @ np.array([-hx - 0.34, -hy + LEAD_R, CN1[1] + dz + 0.10])
        q4 = bpos + np.array([0.24 * s + 0.02, 0.12 * s, 0.07 * (1 if k == 0 else -1)])
        q3 = (q2 + q4) / 2 + np.array([0.0, 0.0, 0.10 + 0.06 * k])
        q3[1] = LEAD_R
        path = catmull([q0, q1, q2, q3, q4], 6)
        lpaths.append(path)
        leads += [R.prim(R.capsule(tuple(a), tuple(b), LEAD_R), mat) for a, b in zip(path[:-1], path[1:])]
    node = R.U(R.xf(body, pos, rot), plug, brick, sock, screw, glass, *caps, *leads)
    return dict(node=node, rot=rot, pos=pos, cable=cab, crot=crot, cpos=cpos, bpos=bpos, leads=lpaths)


def geometry(cam, sc):
    cv = ak.Canvas("#000000")
    mats = [R.Mat("#FFFFFF", spec=0.0)] * 9
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
    fp = (o + d * np.where(np.isfinite(tf), tf, 0)[:, None]).reshape(128, 128, 3)
    shadow = ak.px_mean(R.ground(cv, sc["node"], cam, y=0.0, light=tuple(KEY), ss=2, k=5.0, ao=False), cv.s)
    return dict(mat=mat, p=p, n=n, floor=fp, tf=tf.reshape(128, 128), shadow=shadow)


def pulses(pic, G, mask, pts, at, keep, tail=("a2", "a3")):
    """Power running along a cable (its centre line `pts`), each pulse a
    cream core at arc length `at` from the line's start with a comet tail
    behind it (toward larger arc lengths: it runs toward the start)."""
    seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
    arc = np.concatenate([[0], np.cumsum(seg)])
    hp = G["p"][mask]
    near = np.argmin(((hp[:, None, :] - pts[None, :, :]) ** 2).sum(-1), axis=1)
    s_px = np.zeros((128, 128))
    s_px[mask] = arc[near]
    for s0, d in at:
        core = mask & (np.abs(s_px - s0) < 0.06)
        trail = mask & ((s_px - s0) * d > 0.06) & ((s_px - s0) * d < 0.36)
        pic.put(trail, tail[0])
        pic.put(trail & ((s_px - s0) * d < 0.18), tail[1])
        pic.put(core, "cream")
        keep |= core | trail


def smooth(a, b, v):
    t = np.clip((np.asarray(v, float) - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


DIM = {"cream": "grey", "grey": "m1", "m1": "m0", "m0": "black", "a3": "a2", "a2": "a1", "a1": "a0", "a0": "black",
       "cyan": "m1", "br0": "black", "red": "br0"}


def dim(pic, mask, n=1):
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
FILAMENT = [".....",
            ".ccc.",
            "ccccc",
            ".ccc."]                    # the filament's white-hot coil, the star's heart
STAR = (9, 9, 11, 5)                          # the burst on the filament: its arms (left, right, up, down)
BOLT = ["..a",
        ".a.",
        "aaa",
        ".a.",
        "a.."]
SPEC = ["..c",
        ".c.",
        "c.."]
DIGITS = {"2": ["####", "...#", "...#", "####", "#...", "#...", "####"],
          "0": ["####", "#..#", "#..#", "#..#", "#..#", "#..#", "####"],
          "V": ["...", "...", "...", "#.#", "#.#", "#.#", ".#."]}


def draw():
    P = ak.Palette(COL, ramps=[AMBER, METAL, RED, ["black", "cyan", "cream"], ["gold0", "gold1", "gold2", "cream"]])
    pic = ak.Picture.blank(P, "black")
    cam = camera()
    sc = scene()
    G = geometry(cam, sc)
    rot, pos = sc["rot"], sc["pos"]
    hx, hy, hz = DEV["half"]
    M = G["mat"]
    dev, glass, cab, plug, brick = M == DEVICE, M == GLASS, M == CAB, M == PLUG, M == BRICK
    screw, sock, redl, blkl = M == SCREW, M == SOCKET, M == REDL, M == BLKL
    things = M >= 0
    keep = np.zeros((128, 128), bool)
    cl = np.clip(G["n"] @ KEY, 0, 1)

    def dev_at(lx, ly, lz=0.0):
        return np.array(cam.project(pos + rot @ np.array([lx, ly, lz], float)))

    horizon = cam.project((CAM["pos"][0], CAM["pos"][1], -500.0))[1]
    s = BULB["scale"]
    bpos = sc["bpos"]
    gc = bpos + np.array([0, (0.42 + GLOBE[1]) * s, 0])                        # the globe's centre
    fil = np.array(cam.project(gc))
    rad = abs(cam.project(gc + np.array([GLOBE[0] * s, 0, 0]))[0] - fil[0])

    # ---- the room
    sky = ~things & ~np.isfinite(G["tf"])
    floor = np.isfinite(G["tf"]) & ~things
    dist = np.where(np.isfinite(G["tf"]), G["tf"], 99.0)
    near_dev = ak.distance_px(dev, GLOW[2])
    lv = np.interp(near_dev, [0, GLOW[1], GLOW[2]], [GLOW[3], GLOW[3] - 0.6, 0.0]) * smooth(CALM[0], CALM[1], Y1)
    lv = np.maximum(lv, np.interp(np.abs(Y1 - horizon), [0, 3, 9], [1.2, 0.8, 0.0]))
    lv = np.where(floor, np.maximum(np.interp(dist, HAZE[0], HAZE[1]), lv * smooth(4.0, 9.0, dist)), lv)
    # the lamp's pool on the floor, and its glow in the air
    fd = np.hypot(G["floor"][..., 0] - bpos[0], G["floor"][..., 2] - bpos[2])
    lv = np.where(floor, np.maximum(lv, np.interp(fd, [0, 0.35, 0.8, 1.4], [2.6, 2.0, 1.0, 0.0])), lv)
    gd = np.hypot(X1 - fil[0], Y1 - fil[1]) / rad
    lv = np.maximum(lv, np.interp(gd, [1.0, 1.3, 2.0, 2.7], [2.2, 1.6, 0.8, 0.0]) * smooth(CALM[0], CALM[1], Y1))
    shade = G["shadow"] * floor
    bg = np.clip(lv - shade * 1.6, 0, 4.0)
    plateau = np.floor(ak.terrace(bg, 0.12) + 0.5)
    ak.by_level(pic, ak.terrace(bg, 0.12), AMBER, ~things & ~sky)
    ak.by_level(pic, ak.terrace(bg, 0.16), AMBER, sky)

    # ---- the grid
    lines = np.zeros((128, 128), bool)
    zc = CAM["pos"][2]
    for k in range(-14, 16):
        a_, b_ = cam.project((k * GRID, 0.0, zc - 0.6)), cam.project((k * GRID, 0.0, -9.0))
        lines |= line_mask([a_, b_])
    for j in range(0, 14):
        z_ = zc - 0.9 - j * GRID
        if zc - z_ > 9.0:
            break
        a_, b_ = cam.project((-14.0, 0.0, z_)), cam.project((14.0, 0.0, z_))
        lines |= line_mask([a_, b_])
    lines &= floor & (dist < 9.5)
    glv = np.clip(np.maximum(plateau + 1, 1 + (dist < 7.0) + (dist < 4.5)) - (shade > 0.5) * 1, 0, 3)
    glv = np.where(dist > 7.5, np.minimum(glv, plateau + 0.0), glv)
    for k, c in ((1, "a0"), (2, "a1"), (3, "a2")):
        pic.put(lines & (glv == k), c)

    # ---- the charger at the back
    crot = sc["crot"]
    cn = G["n"] @ crot
    pic.put(brick, "m0")
    pic.put(brick & (cn[..., 1] > 0.5), "m1")
    pic.put(brick & (cn[..., 0] < -0.5), "m1")
    pic.put(brick & (cn[..., 1] > 0.5) & ak.shift(~brick, 0, 1), "grey")       # its top's far lip, toward the key
    ak.outline(pic, brick, "black", where=~things)
    bx, by = cam.project(sc["cpos"] + crot @ np.array([0.0, CHARGER["half"][1], CHARGER["half"][2]]))
    bx, by = int(round(bx)) - 1, int(round(by)) - 2
    ak.patch(pic, bx, by, BOLT, {"a": "a3"})                                  # its emblem, lit
    keep[by:by + len(BOLT), bx:bx + 3] = True

    # ---- the USB-C cable
    ak.by_level(pic, np.round(0.7 + 1.6 * cl ** 1.5), METAL, cab)
    ak.outline(pic, cab, "black", where=floor)
    top_line = cab & ~ak.shift(cab, 0, 1) & ak.shift(cab, 0, -1)
    pic.put(top_line & (cl > 0.30), "m1")
    ak.by_level(pic, np.round(1.6 + 2.0 * cl), METAL, plug)
    ak.outline(pic, plug, "black", where=floor)
    pulses(pic, G, cab, sc["cable"], PULSES, keep)

    # ---- the leads: red and black, out of CN1 to the lamp; power on the red
    ak.by_level(pic, np.round(1.2 + 1.2 * cl), RED, redl)
    ak.by_level(pic, np.round(0.8 + 1.4 * cl), METAL, blkl)
    ak.outline(pic, redl | blkl, "black", where=~things)
    pulses(pic, G, redl, sc["leads"][0], LEAD_PULSES, keep)

    # ---- the device
    loc = (G["p"] - pos) @ rot
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
    vc_ = (np.abs(ny_) <= 0.3) & (nx_ < -0.3) & (nz_ > 0.3)
    edge[vc_] = 2.0
    edge[(nx_ > 0.3) & (ny_ > -0.3)] = 1.0
    edge[ny_ < -0.3] = 0.5
    dl[rnd] = edge[rnd]
    ak.by_level(pic, ak.terrace(dl, 0.25), METAL, dev)
    ring_d = ak.dilate(dev, 1) & ~dev & ~things
    pic.put(ring_d & (ak.shift(dev, 1, 0) | ak.shift(dev, 0, 1)), "black")
    right_rim = dev & ~ak.shift(dev, -1, 0) & (ak.shift(dev, 1, 0)) & (XX > dev_at(0, 0)[0])
    pic.put(right_rim & (Y1 > dev_at(hx, hy)[1] + 3) & (Y1 < horizon + 14), "a1")
    # CN1: a black header on the left side, the leads going in
    # the lamp's light on the side facing it: an amber rim down its edge
    lamp_rim = dev & ~ak.shift(dev, 1, 0) & (Y1 > dev_at(-hx, hy)[1] + 6)
    pic.put(lamp_rim, "a2")
    pic.put(lamp_rim & (Y1 > fil[1] - rad) & (Y1 < fil[1] + rad * 1.5), "a3")
    hdr = left & (np.abs(ly - CN1[0]) < 0.08) & (np.abs(lz - CN1[1]) < 0.13)
    pic.put(hdr, "black")

    # the screen: the app's trigger page
    sx0, sx1, sy0, sy1 = SCREEN
    bez = front & (lx > sx0 - BEZEL) & (lx < sx1 + BEZEL) & (ly > sy0 - BEZEL) & (ly < sy1 + BEZEL)
    scr = front & (lx > sx0) & (lx < sx1) & (ly > sy0) & (ly < sy1)
    pic.put(bez, "black")
    u = (lx - sx0) / (sx1 - sx0)
    v = (sy1 - ly) / (sy1 - sy0)
    pic.put(scr, "a0")
    H = int(round(dev_at(sx0, sy0, hz)[1] - dev_at(sx0, sy1, hz)[1]))
    Wd = int(round(dev_at(sx1, (sy0 + sy1) / 2, hz)[0] - dev_at(sx0, (sy0 + sy1) / 2, hz)[0]))
    r = np.floor(v * H).astype(int)
    c = np.floor(u * Wd).astype(int)

    def cell(r0, r1, c0, c1, colour, extra=True):
        pic.put(scr & (r >= r0) & (r <= r1) & (c >= c0) & (c <= c1) & extra, colour)

    def glyph(rows, r0, c0, colour):
        for j, row in enumerate(rows):
            for i, ch in enumerate(row):
                if ch == "#":
                    cell(r0 + j, r0 + j, c0 + i, c0 + i, colour)
    cell(1, 1, 1, Wd - 2, "a1")                                        # the status bar
    cell(1, 1, 1, 6, "a3")
    cell(3, 3, 1, int(Wd * 0.45), "cyan")                              # the current against the limit
    for k, ch in enumerate("20"):
        glyph(DIGITS[ch], 5, 1 + 5 * k, "a3")
    glyph(DIGITS["V"], 5, 11, "a2")
    cell(5, 6, Wd - 9, Wd - 2, "a2")                                   # ON
    cell(8, 9, Wd - 9, Wd - 2, "cyan")                                 # the amps
    g0r, g1r = 14, H - 5
    for rr in range(g0r, g1r + 1, 3):
        cell(rr, rr, 1, Wd - 2, "a1", (XX % 2) == 0)
    # the traces: volts (amber) step up to the set voltage; amps (cyan) follow
    stepc = int(Wd * 0.45)
    hi_r, lo_r = g0r + 1, g1r - 1
    cell(lo_r, lo_r, 1, stepc, "a2")
    cell(hi_r, lo_r, stepc, stepc, "a3")
    cell(hi_r, hi_r, stepc, Wd - 2, "a3")
    cell(g1r - 3, g1r - 3, stepc + 2, Wd - 2, "cyan")
    cell(g1r - 3, lo_r, stepc + 2, stepc + 2, "cyan")
    cell(H - 3, H - 2, 1, Wd // 2 - 1, "a2")                           # SET
    cell(H - 3, H - 2, Wd // 2 + 2, Wd - 2, "cyan")                    # LIMIT
    keep |= scr

    def stamp(local, rows, key):
        x_, y_ = dev_at(*local, hz)
        h_, w_ = len(rows), len(rows[0])
        x0, y0 = int(round(x_ - w_ / 2)), int(round(y_ - h_ / 2))
        ak.patch(pic, x0, y0, rows, key)
        for j, r_ in enumerate(rows):
            for i, ch in enumerate(r_):
                if ch in key and 0 <= y0 + j < 128 and 0 <= x0 + i < 128:
                    keep[y0 + j, x0 + i] = True
    stamp(PAD, cross_stamp(), {"h": "grey", "m": "m1", "d": "m0", "k": "black"})
    for b_ in BUTTONS:
        stamp(b_, BUTTON, {"r": "red", "c": "cream", "b": "br0", "k": "black"})
    for p_ in PILLS:
        stamp(p_, ["mmm", "kkk"], {"m": "m1", "k": "black"})
    stamp(GRILLE, SPEAKER, {"k": "black"})

    # ---- the lamp: the socket, the brass screw (its threads), the glass
    # lit from inside (hot in the middle, amber at the rim), the filament a
    # cream burst
    ak.by_level(pic, np.round(0.8 + 1.6 * cl), METAL, sock)
    bl = (G["p"] - bpos) / s
    pic.put(screw, "a2")
    pic.put(screw & (YY % 2 == 1), "a1")                                        # the threads
    pic.put(screw & ~ak.shift(screw, 1, 0) & (YY % 2 == 0), "a3")             # the key down its left
    ak.outline(pic, sock | screw, "black", where=~things)
    gl = np.interp(gd, [0.0, 0.70, 0.92, 1.0], [4.0, 4.0, 3.4, 3.0])
    ak.by_level(pic, ak.terrace(gl, 0.25), AMBER, glass)
    neck = glass & (bl[..., 1] < 0.42 + GLOBE[1] - GLOBE[0] * 0.75)
    pic.put(neck, "a2")
    rim_g = glass & ~ak.erode(glass, 1)
    pic.put(rim_g & (X1 > fil[0] - 1) & (Y1 > fil[1] - 1), "a1")             # its shadow side
    pic.put(rim_g & (X1 < fil[0] - 2) & (Y1 < fil[1] - 2), "a3")             # the key on the glass
    pic.put(ak.dilate(glass, 1) & ~things, "a1")                             # the glass glows into the air
    fx, fy = int(round(fil[0])), int(round(fil[1])) + 1
    for dx in (-2, 2):                                                     # the support wires
        pic.put((XX == fx + dx) & (YY > fy) & glass & ~ak.shift(~glass, 0, -1) & ~neck, "a2")
    ak.patch(pic, fx - 2, fy - 3, FILAMENT, {"c": "cream"})
    ak.glint(pic, fx, fy - 1, 7, tip="a3", arms=STAR)
    keep[fy - 1 - STAR[2]:fy + STAR[3] + 1, fx - STAR[0]:fx + STAR[1] + 1] = True
    sx_, sy_ = int(round(fil[0] - rad * 0.55)), int(round(fil[1] - rad * 0.55))
    ak.patch(pic, sx_, sy_, SPEC, {"c": "cream"})                         # the glass's specular
    keep[sy_:sy_ + 3, sx_:sx_ + 3] = True

    # ---- clean-up and the frame
    ak.despeckle(pic, need=5, keep=keep)
    ak.despeckle(pic, need=3, keep=keep, within=YY > 84, passes=2)          # the knot of leads and the lamp's foot
    dim(pic, (XX < 2) | (XX > 125) | (YY > 125), 2)

    # ---- the title: clean house gold, an extrusion and a drop shadow
    m = ak.load_mask(TITLE)
    tx = ak.centred_x(m)
    t = ak.title(pic, m, tx, TITLE_Y, fill=None, rows=TITLE_ROWS, hi=None, lo=None,
                 extrude=dict(dx=1, dy=1, depth=2, colours=TITLE_EXTRUDE), shadow=dict(dx=1, dy=2, colour="black"))
    # the 1-2 px pockets inside and between letters (the G's slot, the
    # counters, the gaps) closed in black: nothing busy in them
    F = t["face"]
    rows_ = (YY >= TITLE_Y) & (YY < TITLE_Y + m.shape[0])
    one = ~F & ak.shift(F, 1, 0) & ak.shift(F, -1, 0)
    two = ~F & ~ak.shift(F, -1, 0) & ak.shift(F, 1, 0) & ak.shift(F, -2, 0)
    pic.put((one | two | ak.shift(two, 1, 0)) & rows_, "black")
    ak.despeckle(pic, need=4, within=(YY < TITLE_Y + m.shape[0] + 4) & ~F)
    # the bevel (cream toward the key, gold0 away), a lit step on edges
    # running down to the right, and the key's glint on the P
    _, lo_ = ak.bevel_contour(pic, F, "cream", "gold0")
    out = ak.outside_of(F)
    pic.put(lo_ & ak.shift(out, 1, 0) & ak.shift(out, 0, -1) & ak.shift(F, -1, -1), "cream")
    ak.glint(pic, tx + TITLE_GLINT[0], TITLE_Y + TITLE_GLINT[1], 3, tip="gold2")
    return pic.image()


if __name__ == "__main__":
    import boxart
    boxart.save(draw(), HERE.parent / "docs" / "cart.png")
