# CHPower

The CHGame Rev 1.5 board's power section, for sketches on the CHGame
library: `#include <CHPower.h>`.

| | |
|---|---|
| `pwr::` (chpower/Power.h) | the CH211 and the INA226 on their own I2C (CN2's PC18/PC19), PA0 (VBUS sense, and host power), the protected output CN1: on, off, its cut-off limit, its meter |
| `pd::` (chpower/Pd.h) | the USB-PD sink: PD 2.0 and 3.0, fixed and PPS supplies (a PPS request repeats itself every 8 s), the answers sent from the PD interrupt; from WCH's USBPD_SNK example |
| `plog::` (chpower/PdLog.h) | what the PD sink and the CH211 did, with times: 160 events in RAM, the last 16 handed to the app to keep for the run after |

Rev 1.5 only (a rev1 board): `pwr::begin()` finds no CH211 on any other
board, and every power feature is then refused; a build for Rev0 stops
with an error. The battery-powered USB host is its own library on this one,
[CHUSBHost](../CHUSBHost/README.md). The design, the rules for the CH211
and the INA226 (the inhibit, the rearm transaction) and what survives a
reset are [docs/design.md](../../docs/design.md).

The example is [CHPDTrigger](examples/CHPDTrigger/README.md), a USB-PD
trigger and bench supply.

## A sketch

```cpp
#include <CHGame.h>
#include <CHPower.h>

void setup() {
    chgame.boot();
    pwr::begin();               // first: CN1 inhibited, CC a plain sink
    pd::start();                // the PD sink: detection, then the source's offer
}

void loop() {
    if (!chgame.nextFrame()) return;
    chgame.pollButtons();
    pd::service();              // every pass: the PD sink and every check below
    if (chgame.justPressed(A_BUTTON)) pd::request(2);      // the offer's second supply (9 V, say)
    if (chgame.justPressed(B_BUTTON) && pd::contractMv()) pwr::enableCn1(500);   // CN1 on, cut off at 0.5 A
}
```

## Only what a sketch calls

The library is linked a piece at a time. `pwr::begin()` is the core; each
other piece comes in with its first call and costs nothing until then:

| A sketch that calls | Links | Flash | RAM |
|---|---|---:|---:|
| `pwr::begin()`, `poll()` | the core: the I2C, the CH211, PA0 and VBUS, the safeties below | 2.4 KB | 72 B |
| + `pd::start()` | the PD sink, its interrupt, its role swaps and source side | + 5.2 KB | + 300 B |
| + `pwr::enableCn1()`, `setLimit()` or `tele()` | CN1: the INA226's transaction, its telemetry, its checks in `poll()` | + 1.1 KB | + 40 B |
| + `plog::begin()` | the PD log | + 0.8 KB | + 1.9 KB |

Measured by the repository's `tools/sizes.py` (rev1, -Os with LTO). The
pieces reach each other only through hooks that a piece fills in when it
is first used: the core never calls the PD sink, CN1 or the log by name, so
the linker leaves out what the sketch does not use. A USB host
([CHUSBHost](../CHUSBHost/README.md)) pauses the PD sink through
`pwr::pausePd()`, which does not link it either.

## What it guards by itself

An app calls `pwr::begin()`, then `pd::service()` (or CHUSBHost's
`host::service()`) from `loop()`. Every check below runs inside the
library, whatever the app does or forgets:

- **CN1 goes by the library's own contract**, which the PD sink hands it
  (`pwr::onContract()`): `enableCn1(limit)` is refused without one, in
  host mode, or with the limit over its budget (`pwr::budgetMa()`, at most
  2.4 A).
- **The limit is clamped** to 100 mA .. the budget, by `setLimit()` too.
- **A contract that ends while CN1 is on turns it off**; one that shrinks
  lowers the limit to its budget (or turns CN1 off under 100 mA).
- **The cut-off trips by itself**: the INA226's latch at the limit, in
  hardware; reverse current into CN1; the enable node falling.
- **Host power is gated**: `pa0High()` refuses while CN1 is on or the
  CH211 sees VBUS from anywhere (5 V never meets another source's 5 V).
- **PD does not start** without the CH211, or while host power is driven.

What it leaves to the charger and the sketch: the charger's contract is
taken at its word (VBUS is not compared with it: PA0's reading through its
divider is a few percent out, and a certified charger delivers what it
agreed), and CN1 stays on while the voltage changes, as a bench supply's
output follows its knob. A sketch that wants the output off before every
change of supply calls `pd::setOutputOffOnChange(true)`.
- **The checks poll themselves**: `pd::service()` and `host::service()`
  call `pwr::poll()` (every 10 ms at most), so no app has to.
- **Every exit through the CHGame library** (START held 3 s,
  `exitToMenu()`, the debug protocol's B) calls `pwr::safeDown()` first:
  this library defines the CHGame library's weak `chgame_beforeExit()`.

Not through the library: an upload's 1200-baud touch resets the board
directly, and the 1k pull-down on PA0 takes host power off as the reset
lets go of the pin; a crash stops in the core's fault handler, which on
Rev1 drives PA0 low itself (board package 0.4.0). In both, CN1 stays as
the CH211 had it (the CH211 keeps its registers across a reset), guarded
by the INA226's latch, until a sketch on this library starts:
`pwr::begin()` inhibits it first.

## Roles, for powered hubs

By default the PD sink is the USB device (UFP) and rejects every swap.
CHUSBHost's powered-hub support opts in:

- `pd::allowDataSwap(true)` accepts a partner's DR_Swap (the sink becomes
  the USB host, `pd::dfp()`); `pd::requestDataSwap()` asks for one.
- `pd::startSource(line)` runs the PD side as a 5 V 0.5 A dual-role
  source while host power is on, and with `pd::allowPowerSwap(true)`
  accepts a PR_Swap, ending as a sink (`pd::swapPhase()` 4).
- The log records each swap, the source's raw PDOs (their flags) and an
  app's step markers.

## Things to know

- **Sketches run in user mode**: anything that turns interrupts off goes
  through CSR 0x800 (gintenr), as the PD log does; `mstatus` traps.
- **The PD interrupt answers the source** (GoodCRC, then the reply a
  message needs) at once; `pd::service()` from `loop()` does the rest (the
  I2C, VBUS, detection, timeouts). A flash write stops the CPU, the
  interrupt with it: write only while `pd::quiet()`.
- **The log records from `plog::begin()`**; before it, and in a sketch that
  never calls it, the PD sink and the power section log nothing.
  `pd::event()` (the last thing that happened, for a screen) works without
  it.
- In the simulator (`CHSIM`) the power section is made up: a PD 3.0
  charger with PPS, and a load on CN1.

Copyright: the PD code is adapted from WCH's USBPD_SNK example ((c) Nanjing
Qinheng Microelectronics, for their microcontrollers; the notice is in the
files and the repository's NOTICE). The rest: Apache-2.0, the repository's
LICENSE.
