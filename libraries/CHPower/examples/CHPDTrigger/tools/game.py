"""CHPDTrigger: what the CHGame tools need to know (the schema is the CHGame
repository's tools/gamecfg.py; `chgame build`, `upload`, `check` and
`export` read this).

The Rev 1.5 board's USB-PD trigger, on the CHPower library. Rev 1.5 only,
which is the rev1 board (BOARDS): the tools build and export it for rev1
alone, and a card for rev0 leaves it off. It runs on the board; the
simulator shows its screens with made-up readings."""
BOARDS = ("rev1",)
# USB serial on: any byte from a PC gets the PD log as text
# (tools/pdlog.py), and an upload needs no buttons.
FQBN = "CHGame:ch32v:rev1:opt=oslto,rtlib=nano,periph=game,usb=serial"
