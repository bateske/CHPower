"""CHUSBHost: what the CHGame tools need to know (the schema is the CHGame
repository's tools/gamecfg.py; `chgame build`, `upload`, `check` and
`export` read this).

The Rev 1.5 board's battery-powered USB host, on the CHUSBHost and CHPower
libraries. Rev 1.5 only, which is the rev1 board (BOARDS): the tools build
and export it for rev1 alone, and a card for rev0 leaves it off. It runs on
the board; the simulator shows its screens with a made-up keyboard, mouse
and pad."""
BOARDS = ("rev1",)
# USB serial on: with a PC on the port (host mode paused), any byte gets the
# USB log as text (tools/usblog.py), the host's probe (its LOG driver).
FQBN = "CHGame:ch32v:rev1:opt=oslto,rtlib=nano,periph=game,usb=serial"
