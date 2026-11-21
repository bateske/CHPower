# CHPDTrigger: PD Trigger

Turn a USB-C charger into a bench supply: the Rev 1.5 asks it for any
voltage it offers (and any voltage in a PPS range, 20 mV at a time), then
meters, graphs and cuts off its protected output CN1. A multimeter, a scope
and a bench supply on a secret agent's watch, in warm amber.

![CHPDTrigger gameplay](docs/gameplay.gif)

## Controls

| Where | Button | Action |
|---|---|---|
| Trigger | UP / DOWN | Turn the bracketed knob (hold for big steps) |
| | LEFT / RIGHT | The other knob: SET or LIMIT |
| | A | The output on, off, or on again after a cut-off |
| | SELECT | The menu |
| Menu, tour, lists | UP / DOWN | Pick, scroll |
| | A | Open, ask for a voltage |
| | B | Back |
| PD log | A / SELECT | Freeze / clear |
| | LEFT / RIGHT | This run / the last run |
| Anywhere | START (hold 3 s) | The output off, then the SD game menu |

## What it does

Plug a USB-C PD charger into the port. The top line says what it speaks
(PD 2.0, PD 3.0, PD 3.0 PPS, or Type-C 5 V without PD).

- **The meter**: the output's volts (big), amps and watts while it is on;
  the charger's volts (INPUT) while it is off. The numbers hold still while
  a reading only wanders (averaged, the last digit moving only on a whole
  step) and jump at once when it really changes; the scope keeps every
  sample. The thin bar under the title is the current against the limit.
- **The scope**: twelve seconds of volts (amber) and amps (cyan); the
  ground darkens while the output is on, and a red line marks a cut-off.
- **SET**: the voltage. UP/DOWN steps 0.1 V through a PPS range and stops
  at each fixed voltage; without PPS it goes from one fixed voltage to the
  next. Half a second after the knob stops, the charger is asked (the value
  blinks until it agrees). The voltage set last is asked again when a
  charger is plugged in.
- **LIMIT**: the output's cut-off current, 0.1-2.4 A in 0.05 A steps, never
  over what the charger can spare after the console (MAX).
- **A** switches the output (CN1: pin 2 +, pin 1 ground). It stays on while
  you turn the voltage, fixed and PPS alike, as a bench supply's output
  follows its knob. Pressed while the voltage is still changing, it
  switches on once the charger has settled.

The settings are kept in flash. **SELECT** opens the menu:

- **CHARGER OFFER**: every voltage the charger offers (fixed, and PPS
  ranges); A asks for one.
- **GUIDED TOUR**: first power, setting a voltage (and PPS), powering a
  project and its cut-off. Each step watches for what you do and says what
  it saw.
- **DIAGNOSTICS**: the CH211's eight registers (now and at start-up), the
  INA226's alert at start-up, VBUS and its flags, the PD state and its
  counts, the output, the I2C bus.
- **PD LOG**: what the PD sink and the CH211 did, newest at the bottom,
  each with its time: every PD message in (cyan) and out (amber; red when
  the charger never acknowledged it), the offer, the state, the CC line,
  hard resets, VBUS moving. The **last run** is the last 16 events before
  this start, kept in flash at each hard reset, unplug or unanswered
  request.

## How to use it

It needs a **Rev 1.5** board: the CHGame Rev1 board with the power
section. On a plain Rev 1 it says NO POWER KIT and switches nothing; for a
Rev0 it does not build. Install the CHGame board package (0.4.0 or later)
and this repository's libraries ([Get started](../../../../README.md#-get-started)),
pick *Tools > Board > CHGame Rev1* and open *File > Examples > CHPower >
CHPDTrigger*, or run `chgame upload` in this folder with the CHGame tools.
`chgame export` makes it a `.chgame` cart for the SD game menu (PD
TRIGGER).

With a PC on the port, any byte sent gets the PD log (the last run's, then
this run's) as text:

```bash
python libraries/CHPower/examples/CHPDTrigger/tools/pdlog.py --out out/pdlog.txt
```

- **Leave by holding START**, not by uploading while the output is on. An
  upload resets the chip without asking the sketch first, and the CH211
  keeps CN1 switched on across a reset (still under the INA226's cut-off)
  until the next sketch on CHPower starts.
- The output's budget assumes the console takes 2 W through an 85% buck,
  with 10% tolerance and 50 mA kept back: about 2.25 A at 5 V / 3 A. It is
  a placeholder until the bench measures the console.

## Developer notes

- **The power section is the CHPower library** ([its README](../../README.md)):
  `pwr::` (the CH211, the INA226 and CN1), `pd::` (the PD sink) and
  `plog::` (its log). This app uses all three, so it links all three; its
  sister app, [CHUSBHost](../../../CHUSBHost/examples/CHUSBHost/README.md),
  links none of the PD sink.
- **The safety is the library's, not the app's.** CN1 goes by the contract
  the PD sink hands the power section; the limit is clamped to that
  contract's budget; a contract that ends or shrinks turns CN1 off or the
  limit down; VBUS off the contract trips it; and every exit through the
  CHGame library powers down first (CHPower's `chgame_beforeExit()`).
  [PdUi.cpp](PdUi.cpp) only asks.
- **The PD interrupt answers the charger** (GoodCRC, then the reply) in
  microseconds; `pd::service()` from `loop()` does the I2C and the timing.
  A flash write stops the CPU, so the settings and the last run are saved
  only while `pd::quiet()` ([Saved.cpp](Saved.cpp)).
- **The guided tour** ([Guide.cpp](Guide.cpp)) is a list of steps, each
  watching the hardware for what it asks (a charger, a voltage, a load, a
  cut-off) instead of a PASS/FAIL test.
- **The secret agent chrome** ([Agent.h](Agent.h)) is the apps' shared
  file (CHSDtoUSB's instrument panel), with this app's own palette in
  [CHPDTrigger.ino](CHPDTrigger.ino).
- In the simulator (`chgame sim`, then `chgame run tools/scripts/screens.txt
  out/screens`) the charger is made up: PD 3.0, 5/9/15/20 V and PPS
  3.3-21 V, with a load on CN1.

## Credits

By bateske, MIT ([LICENSE](LICENSE)). Built on the CHGame library, CHGfx
and CHPower, whose PD sink comes from WCH's USBPD_SNK example (its notice
is in the library's files and the repository's NOTICE).
