<a id="readme-top"></a>

<div align="center">

<a href="https://github.com/bateske/CHGame"><img src="docs/brand/banner.gif" width="100%" alt="CHPower: power for the CHGame Rev 1.5. The handheld pages from the PD TRIGGER app's box art to USB HOST's"></a>

<p>
<a href="https://github.com/bateske/CHGame"><img src="docs/brand/btn-chgame.svg" alt="CHGame"></a>&nbsp;
<a href="#-get-started"><img src="docs/brand/btn-start.svg" alt="Get started"></a>&nbsp;
<a href="docs/design.md"><img src="docs/brand/btn-design.svg" alt="The design"></a>&nbsp;
<a href="https://community.arduboy.com/c/color/56"><img src="docs/brand/btn-forum.svg" alt="Forum"></a>
</p>

[![Board](https://img.shields.io/badge/board-CHGame%20Rev%201.5-F9D84A?style=for-the-badge&labelColor=252628)](https://github.com/bateske/CHGame)
[![Arduino](https://img.shields.io/badge/Arduino-2%20libraries-BDE2C7?style=for-the-badge&logo=arduino&logoColor=white&labelColor=252628)](#-get-started)
[![USB-PD](https://img.shields.io/badge/USB--PD-3.0%20%2B%20PPS-C8B6F3?style=for-the-badge&labelColor=252628)](libraries/CHPower/README.md)
[![RISC-V](https://img.shields.io/badge/RISC--V-CH32X035-B7DACC?style=for-the-badge&logo=riscv&logoColor=white&labelColor=252628)](docs/design.md)

<br>

<img src="docs/brand/warning.svg" alt="Rev 1.5 only: the power section. For the CHGame Rev1 board with its power section. CN1 can carry up to 20 V: the library guards it.">

<br>

</div>

**CHPower** is the add-on for the [CHGame](https://github.com/bateske/CHGame)
handheld's Rev 1.5 board, the one with a power section: a USB-C charger
becomes a bench supply on its protected output, and its battery powers a
keyboard, a mouse or a gamepad as a USB host, or a powered USB-C hub powers
(and charges) it while it stays the host. Two Arduino libraries drive it,
every safety inside them, and each sketch is linked with only the parts it
calls. Two apps show them off, and a third tries out a new hub.

It lives apart from CHGame because the power section is the Rev 1.5's
alone: later boards may not carry it.

<details open>
<summary><b>Contents</b></summary>

1. [What it is](#-what-it-is)
2. [Get started](#-get-started): [install](#2-install-the-libraries), [a first sketch](#a-first-sketch)
3. [Only what you call](#-only-what-you-call)
4. [What the libraries guard](#-what-the-libraries-guard)
5. [The apps](#-the-apps)
6. [For developers](#-for-developers)
7. [Licences](#-licences)

</details>

## ⚡ What it is

The Rev 1.5 is a CHGame Rev1 (same chip, same pins, same games) with a
power section added on the USB-C port:

| | What the board has | What drives it |
|---|---|---|
| **USB-PD sink** | the CH32X035's USBPD peripheral, reached through the CH211's CC switches | `pd::`: PD 2.0 and 3.0, fixed supplies and PPS (any voltage in a range, 20 mV steps); data and power role swaps, for powered hubs |
| **Protected output CN1** | the CH211's high-side switch, an INA226 metering it and cutting it off, an eFuse | `pwr::`: on and off by a checked transaction, the limit, the meter |
| **Battery-powered USB host** | a boost from the battery on PA0, the USBFS peripheral as a host | `host::`: keyboards, mice, HID, Xbox 360 and Switch Pro pads, hubs (a hub behind a hub too), and powered PD hubs that run and charge the board |

A plain Rev1, or a Rev 1.5 running ordinary CHGame games, leaves all of it
off: the power section wakes up as a plain 5 V charger port.

### What is here

| | Piece | Where |
|---|---|---|
| 🔋 | **CHPower**, `#include <CHPower.h>`: the power section, the USB-PD sink, CN1 and the PD log | [`libraries/CHPower/`](libraries/CHPower/README.md) |
| 🔌 | **CHUSBHost**, `#include <CHUSBHost.h>`: the USB host, a driver for each kind of device, on CHPower | [`libraries/CHUSBHost/`](libraries/CHUSBHost/README.md) |
| 🎛 | **CHPDTrigger**, CHPower's example: a USB-PD trigger and bench supply | [`libraries/CHPower/examples/CHPDTrigger/`](libraries/CHPower/examples/CHPDTrigger/README.md) |
| ⌨️ | **CHUSBHost**, CHUSBHost's example: a view for each device, powered hubs, and a log | [`libraries/CHUSBHost/examples/CHUSBHost/`](libraries/CHUSBHost/examples/CHUSBHost/README.md) |
| 🔎 | **CHPDHubProbe**: a guided bench test for a powered USB-C hub, with its USB, PD and timeline logs | [`libraries/CHUSBHost/examples/CHPDHubProbe/`](libraries/CHUSBHost/examples/CHPDHubProbe/README.md) |
| 📐 | **The design**: the CH211's and the INA226's rules, the host entry sequence, what survives a reset | [`docs/design.md`](docs/design.md) |
| 🧰 | **Tools**: what each part costs, the pictures on this page | [`tools/`](tools/) |
| 📝 | **Changelog**: CHPower 0.1.0 and CHUSBHost 0.2.0, the first release | [`CHANGELOG.md`](CHANGELOG.md) |

<p align="right"><a href="#readme-top">back to top ↑</a></p>

## 🚀 Get started

### 1. Install the CHGame board package

The CHGame board package, **0.4.0 or later** (it has the *CHGame Rev1*
board, and brings the CHGame library these build on): add its Boards
Manager URL in the Arduino IDE (*File > Preferences*), then install
*CHGame* in the Boards Manager. CHGame's README has the steps:
[Get started](https://github.com/bateske/CHGame#-get-started).

```
https://github.com/bateske/CHGame/releases/latest/download/package_chgame_index.json
```

### 2. Install the libraries

Download this repository (*Code > Download ZIP*) and copy both folders in
its `libraries/` into your sketchbook's `libraries` folder (*File >
Preferences > Sketchbook location*), then restart the IDE:

```bash
git clone https://github.com/bateske/CHPower
cp -r CHPower/libraries/CHPower CHPower/libraries/CHUSBHost ~/Arduino/libraries/
```

### 3. Pick the board and open an app

*Tools > Board > CHGame > CHGame Rev1* (a Rev 1.5 is a Rev1), then *File >
Examples > CHPower > CHPDTrigger* or *File > Examples > CHUSBHost >
CHUSBHost*, and Upload. A build for Rev0 stops with an error: there, the
power section's pins are buttons.

### A first sketch

A charger asked for 9 V, then the output switched on with a 0.5 A cut-off:

```cpp
#include <CHGame.h>
#include <CHPower.h>

void setup() {
    chgame.boot();
    pwr::begin();               // first: the output inhibited, CC a plain sink
    pd::start();                // the PD sink: detection, then the charger's offer
}

void loop() {
    if (!chgame.nextFrame()) return;
    chgame.pollButtons();
    pd::service();              // every pass: the PD sink and every safety check
    if (chgame.justPressed(A_BUTTON)) pd::request(2);       // the offer's second supply: 9 V on most chargers
    if (chgame.justPressed(B_BUTTON) && pd::contractMv()) pwr::enableCn1(500);   // CN1 on, cut off at 0.5 A
}
```

A keyboard, read on battery power:

```cpp
#include <CHGame.h>
#include <CHUSBHost.h>

void setup() {
    chgame.boot();
    pwr::begin();
    host::begin(host::KEYBOARD);    // the drivers to link: a keyboard, nothing else
    host::enter();                  // waits for a device, then powers it
}

void loop() {
    if (!chgame.nextFrame()) return;
    host::service();
    host::Key k;
    while (host::nextKey(k)) { /* k.ch: 'a', 'B', '\n' ... */ }
}
```

<p align="right"><a href="#readme-top">back to top ↑</a></p>

## 🪶 Only what you call

The CHGame has 50 KB for a program and 18 KB of RAM. These libraries are
built so that a sketch is linked with what it calls and nothing more, and
none of it is a framework you have to live inside: call the parts you want,
in the order you want, from your own loop.

- **CHPower** is linked a piece at a time: `pwr::begin()` is the core, and
  the PD sink, CN1 and the PD log each come in with their first call. The
  pieces reach each other only through hooks a piece fills in when it is
  first used, so the core never pulls the others in.
- **CHUSBHost** has a driver for each kind of device, and links only the
  ones the sketch names in `host::begin()`; `host::ALL` is all of them.
  The PD code is linked for a host only when it names `host::PD_SWAP`
  (powered hubs) or calls `pd::` itself; otherwise the PD sink is paused
  through CHPower without being referenced.

Measured on minimal sketches (`python tools/sizes.py`, rev1, -Os with LTO),
over the CHGame library's frame loop alone:

| A sketch that calls | Flash | RAM |
|---|---:|---:|
| `pwr::begin()`: the power section's core and its safeties | +2.4 KB | +72 B |
| + `pd::start()`: the USB-PD sink | +7.6 KB | +372 B |
| + `pwr::enableCn1()`: the protected output | +8.7 KB | +412 B |
| + `plog::begin()`: the PD log | +9.5 KB | +2,348 B |
| `host::begin(host::MOUSE)` | +8.4 KB | +760 B |
| `host::begin(host::KEYBOARD)` | +9.3 KB | +808 B |
| `host::begin(host::GAMEPAD)` | +9.9 KB | +948 B |
| `host::begin(host::KEYBOARD \| host::MOUSE \| host::HUB)` | +10.6 KB | +1,044 B |
| `host::begin(host::PADS)`: HID, Xbox 360 and Switch Pro | +10.9 KB | +948 B |
| `host::begin(host::ALL)`: every driver, chained hubs, the probe log | +15.1 KB | +3,096 B |
| `... KEYBOARD \| MOUSE \| HUB \| PD_SWAP`, Try.SNK, the PD sink: powered hubs | +17.1 KB | +1,376 B |

<p align="right"><a href="#readme-top">back to top ↑</a></p>

## 🛡 What the libraries guard

The output can carry 20 V, and the host puts the battery's 5 V on the
port, so the safety is in the libraries, not left to each sketch:

- **The output goes by the charger's contract**, which the PD sink hands
  the power section: no contract, a limit over its budget, or host mode,
  and `enableCn1()` refuses. A contract that ends turns it off, one that
  shrinks lowers the limit. The charger is taken at its word on the
  voltage, and the output follows a new voltage as a bench supply's does.
- **It is watched every 10 ms**: the INA226's cut-off (in hardware),
  reverse current, the CH211 resetting itself or going quiet each turn it
  off.
- **5 V never meets another source's 5 V**: host power is refused while a
  charger or a PC is on the port, or the output is on, and goes off within
  milliseconds of an unplug. When a powered hub takes over the power, the
  board's 5 V is off and VBUS under 0.8 V before the hub's comes on.
- **Every exit to the CHGame menu powers down first** (START held, the
  library's `exitToMenu()`), through a hook in the CHGame library that
  CHPower fills in.

[CHPower's README](libraries/CHPower/README.md#what-it-guards-by-itself)
has the whole list, and what a reset leaves as it was.

<p align="right"><a href="#readme-top">back to top ↑</a></p>

## 🕹 The apps

| PD TRIGGER | USB HOST |
|---|---|
| <img src="libraries/CHPower/examples/CHPDTrigger/docs/gameplay.gif" width="256" alt="PD TRIGGER: the voltage raised to 11 V, the output switched on, the meter and the scope"> | <img src="libraries/CHUSBHost/examples/CHUSBHost/docs/gameplay.gif" width="256" alt="USB HOST: a keyboard typing, a mouse, a gamepad and the log"> |
| A USB-C charger as a bench supply: any voltage it offers, PPS 20 mV at a time, on the protected output with a meter, a scope, a current cut-off, a guided tour and the PD log. [Its README](libraries/CHPower/examples/CHPDTrigger/README.md) | A keyboard, a mouse or a gamepad (HID, Xbox 360, Switch Pro), on the port or through hubs, each with a view of its own, and a log of what each device said for a PC to read. A powered USB-C hub runs and charges it, plugged in powered or powered later. [Its README](libraries/CHUSBHost/examples/CHUSBHost/README.md) |

Both are drawn in the CHGame apps' "secret agent" style, and both have box
art for the CHGame's visual game menu: `chgame export` in an app's folder
makes it a `.chgame` cart for a Rev1 SD card.

**PD HUB PROBE** ([its README](libraries/CHUSBHost/examples/CHPDHubProbe/README.md))
is a plain bench tool beside them: it walks through a hub unpowered,
powered later, unplugged and plugged in powered, and logs what the hub
said at each step, for a hub that does not behave in USB HOST. Apple's
USB-C Digital AV Multiport Adapter is the one tested; what it does is in
[the design](docs/design.md#12-powered-hubs-added-2026-11-20).

<p align="right"><a href="#readme-top">back to top ↑</a></p>

## 🧰 For developers

The apps build, run in the simulator and record their GIFs with the CHGame
repository's tools, which take a sketch from here as they take their own
(with these libraries beside it). Clone CHGame beside this repository and
install its tools once:

```bash
git clone https://github.com/bateske/CHGame
pip install -e "./CHGame[sim]"
```

Then, in an app's folder (`libraries/CHPower/examples/CHPDTrigger`, say):

```bash
chgame build                                          # release build for rev1, with its size
chgame upload                                         # onto the board, over USB
chgame sim && chgame run tools/scripts/screens.txt out/screens   # its screens, in the simulator
chgame check                                          # everything that runs without a board
chgame gif                                            # docs/gameplay.gif, from tools/scripts/gameplay.txt
chgame boxart                                         # docs/cart.png, from tools/cart.py
chgame export                                         # build/<Name>.chgame, a cart for the SD menu
```

And from this repository's root:

```bash
python tools/sizes.py --markdown                      # the table above, measured again
python tools/brand/make.py                            # this page's banner, buttons, caution and docs/brand/social.png
```

`docs/brand/social.png` is the repository's link preview: GitHub takes it
only by hand, under *Settings > Social preview*.

<p align="right"><a href="#readme-top">back to top ↑</a></p>

## 📜 Licences

| Path | Licence |
|---|---|
| `libraries/CHPower/`, `libraries/CHUSBHost/` (but their examples), `tools/`, `docs/`, root files | Apache-2.0 (`LICENSE`, `NOTICE`) |
| `libraries/CHPower/examples/CHPDTrigger/`, `libraries/CHUSBHost/examples/CHUSBHost/`, `libraries/CHUSBHost/examples/CHPDHubProbe/` | MIT (each its own `LICENSE`) |

The USB-PD sink and the USB host's transfers are adapted from WCH's
CH32X035 examples, whose notice limits them to WCH's microcontrollers
(this is one): [NOTICE](NOTICE) names the files. The box art's titles are
set in a pixel font by others, named in each app's `tools/art/title.txt`;
this page's lettering is Bitrimus (CC0).

<p align="right"><a href="#readme-top">back to top ↑</a></p>
