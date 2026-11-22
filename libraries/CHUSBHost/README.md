# CHUSBHost

Battery-powered USB host for the CHGame Rev 1.5 board: keyboards, mice and
gamepads, on the port directly or behind a hub (or a hub behind a hub). `#include <CHUSBHost.h>`.

It is built on [CHPower](../CHPower/README.md), the power section: the
board's battery boost on PA0 powers the device, the CH211 switches CC to
the MCU's Rp, and every check of CHPower's runs while the host has the
port (5 V never goes onto another source's 5 V; host power goes off within
milliseconds of an unplug). Rev 1.5 only.

The examples are [CHUSBHost](examples/CHUSBHost/README.md), a view for
each kind of device and a log of what each one said, and
[CHPDHubProbe](examples/CHPDHubProbe/README.md), a guided bench test for a
powered USB-C hub.

## A sketch

```cpp
#include <CHGame.h>
#include <CHUSBHost.h>

void setup() {
    chgame.boot();
    pwr::begin();                       // the power section, first
    host::begin(host::KEYBOARD);        // the drivers this sketch links: a keyboard
    host::enter();                      // host mode: waits for a device's Rd, then powers it
}

void loop() {
    if (!chgame.nextFrame()) return;
    host::service();                    // every pass: the port, the device, the power checks
    host::Key k;
    while (host::nextKey(k)) {
        // k.ch is the character on a US layout ('\n' Enter, '\b' Backspace), k.usage the HID code
    }
}
```

`host::enter()` takes the board's USB port from the serial port (no
serial, upload or debug protocol until `host::leave()`), and refuses while
a charger or a PC powers the port.

## The drivers

Each kind of device is a driver of its own, linked only when the sketch
names it in `host::begin()`:

| Driver | Reads |
|---|---|
| `host::KEYBOARD` | boot keyboards, and a keyboard that declares no boot interface (its report descriptor checked); key presses as events (`nextKey()`), the modifiers (`mods()`), the keys held (`keysDown()`), Caps Lock (`capsLock()`) and its LED; an error report (rollover, POST fail, undefined: usages 1-3) changes only the modifiers, so no key is pressed twice |
| `host::MOUSE` | boot mice: movement, buttons, the wheel |
| `host::GAMEPAD` | HID gamepads and joysticks: their report descriptor parsed for the buttons, both sticks, the triggers and the hat |
| `host::XINPUT` | Xbox 360 pads, and the many that pass for one |
| `host::SWITCH_PRO` | Switch Pro pads and pads in Switch mode (057E:2009), after the handshake they wait for |
| `host::HUB` | one hub on the port, up to four of its ports (without it, a hub is not supported) |
| `host::HUBS` | `HUB`, and up to two hubs behind it (a hub plugged into a hub): four ports each, seven devices in all; in `ALL` |
| `host::LOG` | the probe log: every request and its answer, as text lines, and each endpoint's polls |
| `host::PD_SWAP` | PD while the board powers the port: our 5 V offered as a dual-role port, and a powered hub's power role swap (not in `ALL`; it links CHPower's PD code) |
| `host::PADS`, `host::ALL` | the three gamepad drivers; every driver |

`host::begin()` is inline: with the drivers written as constants, as
above, the ones not named are never referenced, so neither their code nor
their RAM is linked. A device no named driver takes still enumerates, and
`host::state()` says UNSUPPORTED. The PD sink is never linked for the
host: it is paused through `pwr::pausePd()`, which stops it only if the
sketch started it. `PD_SWAP` is the exception: it links the PD code, for powered hubs
(below).

What a sketch costs on top of CHPower's core (2.4 KB of flash, 72 B of
RAM), measured by the repository's `tools/sizes.py` (rev1, -Os with LTO):

| `host::begin(...)` | Flash | RAM |
|---|---:|---:|
| `MOUSE` | 6.0 KB | 688 B |
| `KEYBOARD` | 6.9 KB | 736 B |
| `GAMEPAD` | 7.5 KB | 876 B |
| `KEYBOARD \| MOUSE \| HUB` | 8.2 KB | 972 B |
| `PADS` | 8.5 KB | 876 B |
| `ALL`, with `HUBS` (the log's 1.6 KB of RAM included) | 12.7 KB | 3,024 B |
| `KEYBOARD \| MOUSE \| HUB \| PD_SWAP`, Try.SNK and the PD sink: powered hubs | 14.7 KB | 1,304 B |

Before the drivers were split (one library, every driver and the PD sink
in every host sketch), the example app was 2.9 KB larger with everything
on.

## How it enters host mode

The board handoff's five steps ([docs/design.md](../../docs/design.md),
section 7): CN1 off and the PD sink paused; PA0 low, then analog, and VBUS
must be absent (a charger plugged in refuses); CC: the CH211's Rd off and
both paths to the MCU, whose 80 uA Rp (default USB power) then looks for a
sink's Rd, debounced 150 ms; only then PA0 high (the boost), and VBUS must
be ready within 275 ms. Detach, a fault or `host::leave()` drives PA0 low
first.

A USB-C device without its CC resistors (some keyboards on an A-to-C
adapter) shows no Rd: `host::force()` powers the port anyway, after the
same check that VBUS is absent, and turns it off when the device leaves
or after 5 s without one.

## Powered PD hubs

A hub with its own charger (Apple's USB-C Digital AV Multiport Adapter is
the one tested) can power the board, and charge it, while the board stays
the USB host. Three pieces, each opt-in:

- **`host::setTrySink(true)`**: Try.SNK on attach. When a device's Rd
  shows, the board is a sink for up to 0.45 s before it powers the port. A
  powered dual-role hub then becomes the source, and host mode ends with
  `status()` "PORT POWERS US" for the sketch's PD sink. Anything else is
  powered as before, 0.45 s later.
- **`host::enterPowered(lost)`**: host mode on a port someone else powers.
  The sketch runs the PD sink with `pd::allowDataSwap(true)`; the hub hands
  over the host role with a DR_Swap (`pd::dfp()`), and the host then runs
  on the hub's 5 V, CC left to the PD sink and PA0 never driven. `lost()`
  (the contract or the role gone) ends it.
- **`host::PD_SWAP`**: while the board powers a hub, it talks PD as the
  source (5 V 0.5 A, dual-role power and data). A hub that gets its
  charger later asks for a PR_Swap: our 5 V off, VBUS let fall under
  0.8 V, the CH211's Rd on, then the hub's 5 V, with the board still the
  host (status "HUB POWERS US NOW"). The hub's own devices lose power for
  the half second of the swap and enumerate again.

The example app uses all three. A PC or a plain charger never hands over
the host role, so host mode stays paused for them as before.
[CHPDHubProbe](examples/CHPDHubProbe/CHPDHubProbe.ino) is a guided bench
test for a new hub, with the USB, PD and timeline logs for a PC.

## Things to know

- **A device behind a hub** is enumerated as on the port; the hub's ports
  are read one every 25 ms, never in a wait. A hub behind the hub is not
  used with `HUB`; with `HUBS` it is (two behind the first at most, four
  ports each, seven devices in all, the ports read every 15 ms), so a
  powered PD hub can feed a hub of its own. `HUBS` is its own code with
  a bigger device table, so a sketch that wants one level pays for no
  more.
- **Gamepads**: `host::pad()` has the buttons (bit n is button n + 1), the
  sticks scaled to -127..127, the triggers to 0..255 and the hat (8 when
  centred); `xinput` and `nswitch` say which button layout it is.
  `host::rawReport()` is the last report as sent. Xbox One and Series pads
  (GIP) are not read.
- In the simulator (`CHSIM`) the devices are made up: a keyboard typing,
  a mouse going round, an Xbox 360 pad, behind a hub, as far as the
  sketch's drivers read them.

Copyright: the transfer layer and the hub handling are adapted from WCH's
USBFS host driver and HOST_KM example ((c) Nanjing Qinheng
Microelectronics, for their microcontrollers; the notice is in the files
and the repository's NOTICE). The rest: Apache-2.0, the repository's
LICENSE.
