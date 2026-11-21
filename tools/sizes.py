"""What each piece of CHPower and CHUSBHost costs, measured.

    python tools/sizes.py [--markdown]

Writes small sketches into out/sizes/ (one per row below: the same frame
loop, each calling one more piece), builds each for the rev1 board with the
CHGame repository's `chgame build` (release options: -Os, LTO), and prints
flash and RAM against the first, which uses neither library. The README's
table comes from --markdown; a row marked readme=False is left out of it.
The absolute flash, image and RAM of every row go to stderr as each is
built.

Needs the CHGame board package (0.4.0 or later) and a CHGame checkout: the
`chgame` command, or $CHGAME_TOOLS, or a CHGame folder beside this one.
"""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "out" / "sizes"

# name, what it shows, includes (a header, or a list of them), setup lines, loop lines,
# and optionally readme=False: not in the README's table (--markdown)
ROWS = [
    ("Base", "the CHGame library's frame loop alone", "", "", ""),
    ("Power", "pwr::begin() and poll(): the CH211, PA0, VBUS, the safeties", "CHPower.h",
     "pwr::begin();", "pwr::poll(); sink += pwr::vbusMv();"),
    ("PdSink", "+ pd::start(): the USB-PD sink (fixed and PPS supplies)", "CHPower.h",
     "pwr::begin(); pd::start();", "pd::service(); sink += pd::contractMv();"),
    ("PdCn1", "+ CN1: enableCn1(), tele(), its checks", "CHPower.h",
     "pwr::begin(); pd::start();",
     "pd::service(); if (pd::contractMv() && pwr::out() == pwr::OUT_OFF) pwr::enableCn1(500); "
     "sink += pwr::tele().ma;"),
    ("PdCn1Log", "+ plog::begin(): the PD log", "CHPower.h",
     "pwr::begin(); plog::begin(); pd::start();",
     "pd::service(); if (pd::contractMv() && pwr::out() == pwr::OUT_OFF) pwr::enableCn1(500); "
     "sink += pwr::tele().ma + plog::count(false) + plog::at(false, 0).b;"),
    ("HostKeyboard", "pwr::begin(), host::begin(KEYBOARD): a keyboard", "CHUSBHost.h",
     "pwr::begin(); host::begin(host::KEYBOARD); host::enter();",
     "host::service(); host::Key k; while (host::nextKey(k)) sink += k.ch;"),
    ("HostMouse", "host::begin(MOUSE): a mouse", "CHUSBHost.h",
     "pwr::begin(); host::begin(host::MOUSE); host::enter();",
     "host::service(); int16_t x, y; int8_t w; host::takeMouse(x, y, w); sink += x + y + w;"),
    ("HostKbMouseHub", "KEYBOARD, MOUSE and HUB: both, on a hub", "CHUSBHost.h",
     "pwr::begin(); host::begin(host::KEYBOARD | host::MOUSE | host::HUB); host::enter();",
     "host::service(); host::Key k; while (host::nextKey(k)) sink += k.ch; int16_t x, y; int8_t w; "
     "host::takeMouse(x, y, w); sink += x + y + w;"),
    ("HostPdHub", "KEYBOARD, MOUSE, HUB and PD_SWAP, with Try.SNK and the PD sink: powered hubs", "CHUSBHost.h",
     "pwr::begin(); host::begin(host::KEYBOARD | host::MOUSE | host::HUB | host::PD_SWAP); "
     "host::setTrySink(true); host::enter();",
     "host::service(); host::Key k; while (host::nextKey(k)) sink += k.ch; int16_t x, y; int8_t w; "
     "host::takeMouse(x, y, w); sink += x + y + w; "
     "if (host::state() == host::OFF) { pd::allowDataSwap(true); pd::start(); pd::service(); "
     "if (pd::dfp()) host::enterPowered([] { return !pd::dfp(); }); }"),
    ("HostGamepad", "GAMEPAD: HID gamepads", "CHUSBHost.h",
     "pwr::begin(); host::begin(host::GAMEPAD); host::enter();",
     "host::service(); sink += host::pad().buttons;"),
    ("HostPads", "PADS: HID, Xbox 360 and Switch Pro pads", "CHUSBHost.h",
     "pwr::begin(); host::begin(host::PADS); host::enter();",
     "host::service(); sink += host::pad().buttons;"),
    ("HostAll", "ALL: every driver, chained hubs (HUBS) and the probe log", "CHUSBHost.h",
     "pwr::begin(); host::begin(host::ALL); host::enter();",
     "host::service(); host::Key k; while (host::nextKey(k)) sink += k.ch; int16_t x, y; int8_t w; "
     "host::takeMouse(x, y, w); sink += x + y + w + host::pad().buttons + host::probeCount() + "
     "host::probeLine(0)[0];"),
]

SKETCH = """// {name}: a sizing sketch for tools/sizes.py ({what}). Not an example.
#include <CHGame.h>
{include}
volatile uint32_t sink;

void setup() {{
    chgame.boot();
    {setup}
}}

void loop() {{
    if (!chgame.nextFrame()) return;
    chgame.pollButtons();
    {loop}
}}
"""


def chgame():
    """The command line that runs the CHGame repository's chgame."""
    env = os.environ.get("CHGAME_TOOLS")
    for tools in ([Path(env)] if env else []) + [REPO.parent / "CHGame" / "tools"]:
        if (tools / "chgame.py").exists():
            return [sys.executable, str(tools / "chgame.py")]
    if shutil.which("chgame"):
        return ["chgame"]
    raise SystemExit("the chgame command was not found: install the CHGame repository's tools "
                     "(pip install -e .[sim] there), or set CHGAME_TOOLS to its tools folder")


def build(name, what, include, setup, loop, readme=True):
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    inc = "\n".join(f"#include <{i}>" for i in ([include] if isinstance(include, str) else include) if i)
    (d / f"{name}.ino").write_text(SKETCH.format(name=name, what=what, include=inc, setup=setup, loop=loop),
                                   encoding="utf-8", newline="\n")
    r = subprocess.run(chgame() + ["--board", "rev1", "--sketch", str(d), "build"], capture_output=True, text=True)
    text = r.stdout + r.stderr
    if r.returncode:
        raise SystemExit(f"{name}: the build failed\n{text[-3000:]}")
    flash = int(re.search(r"flash \(sections\): (\d+) B", text).group(1))
    ram = int(re.search(r"static RAM: (\d+) B", text).group(1))
    image = int(re.search(r"image: (\d+) B", text).group(1))
    return flash, ram, image


def main():
    md = "--markdown" in sys.argv
    rows = []
    for row in ROWS:
        flash, ram, image = build(*row)
        rows.append((row[0], row[1], flash, ram, row[5] if len(row) > 5 else True))
        print(f"{row[0]:16} {flash:6} B flash  {ram:6} B RAM  {image:6} B image", file=sys.stderr)
    f0, r0 = rows[0][2], rows[0][3]
    if md:
        print("| Sketch | Calls | Flash | RAM |")
        print("|---|---|---:|---:|")
        for name, what, flash, ram, readme in rows[1:]:
            if readme:
                print(f"| {name} | {what} | +{flash - f0:,} B | +{ram - r0:,} B |")
    else:
        for name, what, flash, ram, _ in rows:
            print(f"{name:16} {flash - f0:+7} B flash {ram - r0:+6} B RAM   {what}")


if __name__ == "__main__":
    main()
