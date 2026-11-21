"""CHPDHubProbe: what the CHGame tools need to know (the schema is the CHGame
repository's tools/gamecfg.py).

A guided bench test for powered USB-C PD hubs on the Rev 1.5 board, on the
CHUSBHost and CHPower libraries. Rev 1.5 only, which is the rev1 board."""
BOARDS = ("rev1",)
# USB serial on: any byte from a PC gets the three logs as text
# (../CHUSBHost/tools/usblog.py).
FQBN = "CHGame:ch32v:rev1:opt=oslto,rtlib=nano,periph=game,usb=serial"
