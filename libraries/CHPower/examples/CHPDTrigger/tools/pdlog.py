"""Print what CHPDTrigger (its PD log) or CHUSBHost (its USB log, while
host mode is paused) has logged, from the board over USB serial.

    python tools/pdlog.py [--port COMx] [--out FILE]

Sends one byte, which makes the sketch write its PD log (the last run's
events, then this run's) as text, and prints what comes back until the port
has been quiet for a second. With --out the text is also written to FILE.
The board must be running one of them with its serial port up (CHUSBHost:
host mode paused).
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
import serialcap  # noqa: E402  (the repository's tools/serialcap.py)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port")
    ap.add_argument("--out")
    a = ap.parse_args()
    s = serialcap.open_port(a.port)
    time.sleep(0.3)
    s.reset_input_buffer()
    s.write(b"L")
    text = ""
    quiet = time.time() + 3.0
    while time.time() < quiet:
        data = s.read(4096)
        if data:
            text += data.decode("latin-1")
            quiet = time.time() + 1.0
    s.close()
    text = text.replace("\r\n", "\n")
    sys.stdout.write(text)
    if a.out:
        with open(a.out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    if not text.strip():
        raise SystemExit("nothing came back: is CHPDTrigger running?")


if __name__ == "__main__":
    main()
