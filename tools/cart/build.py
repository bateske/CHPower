"""Builds the CHPower examples cart and its SD card ZIP into out/cart/:

    python tools/cart/build.py [--no-build]

    out/cart/CHPower-Examples.chgame         the multi-cart: PD TRIGGER, USB HOST and
                                             PD HUB PROBE, with the cover (art/cover.png)
    out/cart/CHPower-Examples-sdcard.zip     the same apps as SD card files: unzip them to
                                             the root of a FAT16/FAT32 card

The recipe is tools/cart/cart.json. Each example is built for rev1 (the only
board with the power section) with CHGame's `chgame` command, so it needs
the CHGame board package and the CHGame tools (CHGame's README, "Get
started"). --no-build uses each example's existing build/rev1/release.

The release attaches both files under these names, so the README links to
them through releases/latest/download/.
"""
import argparse
import pathlib
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
RECIPE = ROOT / "tools" / "cart" / "cart.json"
OUT = ROOT / "out" / "cart"
CART = OUT / "CHPower-Examples.chgame"
CARD = OUT / "sdcard"
ZIP = OUT / "CHPower-Examples-sdcard.zip"
BOARD = "rev1"
README = """\
CHPower Examples: SD card files for the CHGame Rev 1.5
======================================================

These apps are for the CHGame Rev 1.5 (a Rev1 board with the power section):
  PD TRIGGER    USB-PD trigger and bench supply (CHPower library)
  USB HOST      keyboards, mice, gamepads and hubs (CHUSBHost library)
  PD HUB PROBE  step-by-step test for a powered USB-C hub (CHUSBHost library)

Copy everything in this zip (the GAMES folder) to the root of a FAT32 (or
FAT16) microSD card. Put the card in the CHGame and switch it on: the game
menu lists the apps. A starts one; holding START for 3 seconds goes back to
the menu. These files replace the card's menu (GAMES/MENU.IDX), so use an
empty card. To combine these apps with other games, use
CHPower-Examples.chgame with the CHGame tools (`chgame cart add`).

The menu is part of the CHGame bootloader. If your board does not show it,
see the CHGame README: https://github.com/bateske/CHGame

The same apps come as one file, CHPower-Examples.chgame, beside this zip.

Source and documentation: https://github.com/bateske/CHPower
"""


def chgame(*args):
    cmd = ["chgame", "--board", BOARD, *args]
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=ROOT, check=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-build", action="store_true", help="use the examples' existing release builds")
    a = ap.parse_args()

    OUT.mkdir(parents=True, exist_ok=True)
    chgame("cart", "build", str(RECIPE.relative_to(ROOT)), str(CART.relative_to(ROOT)),
           *(["--no-build"] if a.no_build else []))
    chgame("cart", "art", str(CART.relative_to(ROOT)))       # a title picture for an app without box art
    chgame("cart", "verify", str(CART.relative_to(ROOT)))

    shutil.rmtree(CARD, ignore_errors=True)
    chgame("cart", "prepare", str(CART.relative_to(ROOT)), str(CARD.relative_to(ROOT)))
    ZIP.unlink(missing_ok=True)
    files = sorted(p for p in CARD.rglob("*") if p.is_file())
    with zipfile.ZipFile(ZIP, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("README.txt", README.replace("\n", "\r\n"))
        for p in files:
            z.write(p, p.relative_to(CARD).as_posix())
    print(f"{CART.relative_to(ROOT).as_posix()}: {CART.stat().st_size} B")
    print(f"{ZIP.relative_to(ROOT).as_posix()}: {len(files)} files, {ZIP.stat().st_size} B")


if __name__ == "__main__":
    sys.exit(main())
