# CHUSBHost: USB Host

Plug a keyboard, a mouse or a gamepad into the Rev 1.5 and it powers it
from its own battery and reads it, on the port or through a hub (a hub
behind a hub too), or runs on a powered USB-C hub's charger and charges:
type into a text field, chase a cursor round a grid, watch every button of
a controller. A secret agent's watch, in blue, with a log of what each
device said for a PC to read afterwards.

![CHUSBHost gameplay](docs/gameplay.gif)

## Controls

| Button | Action |
|---|---|
| LEFT / RIGHT | The views: KEYS, MOUSE, PAD, LOG |
| A | FORCE 5V, when a device is plugged in but not seen (below) |
| B | PC mode: host off, the port a PC's (serial and the log); B again: host |
| UP / DOWN | Scroll the log |
| SELECT | Clear the view |
| START (hold 3 s) | Host power off, then the SD game menu |

When a device arrives, its view comes up by itself.

## What it reads

- **KEYS**: a text field (Esc clears it), the modifiers lit while held, the
  last key's code. Caps Lock is kept here and lights the keyboard's LED.
- **MOUSE**: a cursor on a grid; clicks ping (left amber, right cyan,
  middle blue), the wheel moves the bar on the right.
- **PAD**: a controller with its d-pad, face buttons, shoulders, triggers
  and both sticks; every button by number, and its report as sent.
  - HID pads are read through their own report descriptor (most number
    their buttons as drawn).
  - Xbox 360 pads (XInput, not HID) are read by their fixed report, with
    A B X Y on the face buttons and analog triggers.
  - Switch Pro pads (and pads in Switch mode, 057E:2009) get the handshake
    they wait for (without it they leave the bus after 2 s), then their
    full reports, with B A Y X on the face buttons, digital ZL/ZR, and the
    player 1 light.
  - Xbox One and Series pads (GIP) are not read.
- **LOG**: what the host did with each device: its ids (`VID 057E PID
  2009`), its interfaces and endpoints (`IF00 03 00 00`, ` EP81 03 40 08`),
  every request with its result (`80 06 01 0000 0012 >00 12`: the setup,
  then the result, 00 for success, and how many bytes came back), each
  status with its time; and each endpoint's polls (OK, NAK, errors) with
  the last report.

Along the bottom: what is plugged in, how long the 5 V took to come up,
plugs, and faults (5 V falling under load, over-temperature); on a powered
hub, its contract and CHARGING instead. Along the top, CONNECTING while a
hub with power is being looked at (up to 1.5 s), and SWITCHING while a
hub's power swap brings its devices back: the view, the text typed and
the sounds stay as they were until they are back.

## How to use it

It needs a **Rev 1.5** board: the CHGame Rev1 board with the power
section. On a plain Rev 1 it says NO POWER KIT; for a Rev0 it does not
build. Install the CHGame board package (0.4.0 or later) and this
repository's libraries ([Get started](../../../../README.md#-get-started)),
pick *Tools > Board > CHGame Rev1* and open *File > Examples > CHUSBHost >
CHUSBHost*, or run `chgame upload` in this folder with the CHGame tools.
`chgame export` makes it a `.chgame` cart for the SD game menu (USB HOST).

**A powered USB-C hub** (one with its own charger, such as Apple's USB-C
Digital AV Multiport Adapter) powers the board and charges it while the
board stays the host: plug it in with its charger on, or plug the charger
into the hub while it is already in use (its devices drop out for a
moment, then come back by themselves: SWITCHING, about 2 s). The bottom
line then says `HUB POWER 5.0V 0.1A CHARGING`. Hubs chain: a hub on the
port and up to two more behind it, seven devices in all (the app names
`host::ALL`, which has `HUBS`).

Host mode starts by itself whenever nothing is powering the port: plug a
device in (USB-C, or USB-A through an OTG adapter or a hub). The board
switches its 5 V on only once the device's Rd shows on CC (after a
moment, under half a second, as a sink: a powered hub then powers it), and off within
milliseconds of an unplug. A device without CC resistors (some keyboards
on an A-to-C adapter) shows a **[A] FORCE 5V** button: A powers the port
anyway, never while 5 V is already there, and off again when the device
leaves. With nothing plugged in, it looks for a PC or a charger every few
seconds; when it finds one, host mode **pauses** (the battery charges, the
serial port comes back) until the port is free again.

To read the log on a PC: unplug the device, press B (PC mode), plug the
board into the PC with any cable, then

```bash
python libraries/CHUSBHost/examples/CHUSBHost/tools/usblog.py --out out/usblog.txt
```

The text is the USB log, each endpoint's polls, then the PD log: what a
powered hub offered (with its raw flags), each role swap, and each time the
host cut its 5 V and why (`5V OFF: CC GONE CC3/3 35MS`: CC1/CC2's levels,
3 open; how long it was gone). A hub that misbehaves is best tried with
[CHPDHubProbe](../CHPDHubProbe/README.md), which steps through it.

Host mode borrows the USB port: no serial and no upload while it runs. B
(PC mode) or START held 3 s gives it back.

## Developer notes

- **The host is the CHUSBHost library** ([its README](../../README.md)),
  on CHPower's power section: enumeration, a hub and up to two hubs behind it,
  boot keyboards and mice, HID pads through their report descriptor,
  XInput and Switch Pro pads, each a driver of its own. This app shows
  them all, so it names `host::ALL`; a sketch that wants a keyboard alone
  names `host::KEYBOARD` and links none of the rest. For powered hubs it
  also names `host::PD_SWAP`, turns on Try.SNK and runs the PD sink when
  VBUS is on the port ([HostUi.cpp](HostUi.cpp)'s `autoHost()`); the
  library's README has the details. Otherwise the PD sink is never
  linked into it: `pwr::pausePd()` stops it only if a sketch started it.
- **The safety is the library's, not the app's.** Host power never meets
  another source's 5 V (`pwr::pa0High()` refuses while the CH211 sees VBUS
  or CN1 is on), goes off on an unplug, and every exit through the CHGame
  library turns it off first (CHPower's `chgame_beforeExit()`).
  [HostUi.cpp](HostUi.cpp) only asks.
- **The log** is the library's probe (`host::probeLine()`, the `LOG`
  driver); [CHUSBHost.ino](CHUSBHost.ino) prints it over USB serial when a
  PC sends a byte.
- **The secret agent chrome** ([Agent.h](Agent.h)) is the apps' shared
  file (CHSDtoUSB's instrument panel), in this app's blue palette
  ([CHUSBHost.ino](CHUSBHost.ino)).
- In the simulator (`chgame sim`, then `chgame run tools/scripts/screens.txt
  out/screens`) the devices are made up: a keyboard typing, a mouse going
  round, a pad behind a hub.

## Credits

By bateske, MIT ([LICENSE](LICENSE)). Built on the CHGame library, CHGfx,
CHPower and CHUSBHost, whose USB host comes from WCH's HOST_KM example (its
notice is in the library's files and the repository's NOTICE).
