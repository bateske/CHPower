"""Drive CHUSBHost in the simulator with a script.

    chgame run <script> <outdir>

The CHGame repository's tools/chsim/chdrivelib.py does the driving and has the
script commands (wait, tap, hold, snap, gif, rec ...). In the simulator the
power section and the devices are made up (CHPower's and CHUSBHost's
CHSIM parts: a keyboard, mouse and pad behind a hub), so a run shows
the screens, not the board. --id names the handshake reply (default CHUH).
"""
import sys
from pathlib import Path


def _tools():
    """The CHGame repository's tools/: $CHGAME_TOOLS (the chgame command sets
    it for the scripts it runs), or a CHGame checkout above this sketch or
    beside a folder above it (D:/src/CHGame next to D:/src/CHPower)."""
    import os
    env = os.environ.get("CHGAME_TOOLS")
    if env and (Path(env) / "chsim" / "chdrivelib.py").exists():
        return Path(env)
    for up in Path(__file__).resolve().parents:
        for repo in (up, up / "CHGame"):
            if (repo / "tools" / "chsim" / "chsim.py").exists() and (repo / "platform").is_dir():
                return repo / "tools"
    raise SystemExit(f"{Path(__file__).name}: the CHGame repository's tools/ was not found: run this through "
                     "the chgame command, or set CHGAME_TOOLS to a CHGame checkout's tools folder")


sys.path.insert(0, str(_tools() / "chsim"))
from chdrivelib import Driver, main  # noqa: E402

DRIVER, IDENT = Driver, "CHUH"      # what the shared tools load from this file

if __name__ == "__main__":
    main(DRIVER, ident=IDENT)
