# Rev 1.5 power: the design

> **Read this as the design, not the manual.** It was written on 2026-11-17,
> before any of the code, in the CHGame repository (as `docs/rev1.5-power.md`),
> and it is kept for its hardware rules: the CH211's and the INA226's, the
> host entry sequence, what survives a reset. What was built differs in its
> names and its split: two libraries, CHPower (`pwr::`, `pd::`, `plog::`)
> and CHUSBHost (`host::`), with two apps, CHPDTrigger and CHUSBHost, in
> this repository rather than the board package. The libraries' READMEs
> describe what they do now.

Rev 1.5 (design "Rev1B") is a Rev 1 with a power section: a USB-C PD sink, a
battery-powered USB host and a protected output on CN1. It is the same device
as Rev 1 (`rev1`, target `CGR1`, the IDE's **CHGame Rev1**): only PA0, PC14 and
PC15 differ, and a rev1 program runs on it unchanged
([hardware-revisions.md](https://github.com/bateske/CHGame/blob/main/docs/hardware-revisions.md)). This page designs the
software for the power section. None of it is implemented: the next session
builds it with the board on the bench.

Sources: the Rev 1 / Rev 1.5 board package handoff of 2026-11-17, kept with
the board's design files ("handoff §n"); the CH211 manual v1.2; the CH32X035
reference manual v1.9 ("RM") and datasheet v2.3 ("DS"); the INA226 and
TPS61023 datasheets; the USB Type-C and PD specifications (timer names); WCH's
CH32X035 EVT examples.

## 1. Scope

In scope: a library, CHPower, that owns PA0, the CH211, the INA226, the USBPD
peripheral and (in host mode) USBFS; a hook that makes every software reset
leave the power section as power-on left it; four Rev 1.5 apps; the bench
checks. Out of scope: role swaps and dual-role attach (since added for
powered hubs: section 12), VCONN, PPS and EPR, a
source above 5 V (the boost is a fixed ~5 V supply, handoff §6); battery
gauging or charge control (none is wired, handoff §9); the bootloaders, games,
casino card and `.chgame` format; detecting Rev 1.5 at run time.

**Already safe:** Rev 1 firmware (bootloaders, games, apps) on a Rev 1.5.

| Part | Rev 1 firmware | Result on Rev 1.5 |
|---|---|---|
| PA0 (HOST_EN, VBUS sense) | never drives it: an input after every reset | R11 (1 kΩ) holds HOST_EN low: boost off, input buck on |
| PC14/PC15 (CC1/CC2 to the CH211) | floating inputs | `CC_CTRL` resets to 0x44: Rd on both CC lines, both paths open. A plain 5 V Type-C sink, as Rev 1 |
| CN2 (PC18/PC19) | no I2C traffic (CHBoardTest only reads idle levels) | `OD_CTRL` resets to 0xAA (CN1 inhibited) and the INA226's mask to 0 (ALERT released, Q7 clamps the eFuse): CN1 off by two paths |

That holds while the CH211 and INA226 are in their power-on state: until the
first CHPower program runs, and after it only if that program left through the
reset hook (section 8), or after a full power-down. The CH211 is also fed from
VBUS and keeps its registers while USB is plugged in (handoff §8).

## 2. The library: CHPower

`platform/board/arduino/CHGame/libraries/CHPower` (working name), beside CHGfx,
CHGame and CHSd, so the board package delivers it.

- **Including it declares Rev 1.5 hardware.** No run-time detection: an ACK at
  0x35 or 0x40 proves nothing (handoff §2). `CHPower.h` has
  `#if !defined(ARDUINO_CHGAME_REV1) && !defined(CHSIM)` / `#error`.
- `library.properties`: `architectures=ch32v`, `includes=CHPower.h`,
  `dot_a_linkage=true`. An archive member is linked only for a symbol still
  undefined, and `USBPD_IRQHandler` and `chgame_beforeReset` are weak already:
  they go in the files of `power::setUsbRole()` and `power::begin()`.
- `host/` models the CH211, the INA226 (a read of 0x06 or a write of 0x00
  clears its latch) and PA0 for the simulator and `tests/run_tests.py`.
  Outside the library: sections 8 and 9; `tools/chsim/chsim.py` (CHPower's
  `src/` and `host/`, as for CHSd); the rev1 variant's `PIN_VBUS_SENSE`
  comment; `docs/api/Doxyfile` (`lib_chpower`); `platform/README.md`; the
  CHANGELOG.

**`begin()`**, first in `setup()`, writes to a device only after a read-only
check of it. A failed step stops it and makes every enable, and every role but
`Sink`, return `NoDevice` until the next `begin()`: a missing device is a
diagnostic. A CHPower program on a Rev 1 is unsupported, but it drives nothing
on PA0 and enables nothing. The steps:
1. PA0 an analog input (a reset leaves it a floating input; R11 holds HOST_EN
   low either way). SDA low: up to 9 clocks, then STOP. SCL low: SCL push-pull for step 2's
   transfers (section 3).
2. **CH211, reads only.** A fast read at 0x34 returns all eight registers with
   no pointer write (CH211 manual §5.7). The reserved bits (`OD_CTRL` 4 and 0,
   `HVCP_CTRL` 6-5, `HVIO_KEY` 7-6 and 3, `SYS_STAT` 2) must read 0; the rest
   is what `begin()` inherited. Reading `SYS_STAT` acknowledges the VBUS
   power-up event, which releases SCL if other firmware left `INT_PIN` = 1. If
   SCL is still low, one push-pull write of `PIN_CFG` with `INT_PIN` = 0; then
   SCL must idle high.
3. **CH211 writes** (section 4): `PIN_CFG` (`INT_PIN` = 0, `SDA_PU` = 1) first,
   since the inhibit's read-back needs a sound bus; `OD_CTRL` inhibit;
   `HVIO_KEY` ← 0x00; `CC_CTRL` ← 0x44.
4. **INA226, reads only:** 0xFE = 0x5449, 0xFF = 0x2260. Pointing at 0xFE
   soft-resets some other 0x40 parts (HTU21D, Si7021), hence after the CH211.
5. **INA226 writes:** read 0x06 once, whose alert flag says whether a trip was
   inherited (the read clears the latch, CN1 being inhibited); then RST
   (0x00 ← 0x8000); the pointer is left on 0x02.

Steps 2 and 4 set the RAM flags `verified.ch211` and `verified.ina`, which the
reset hook checks before it writes anything (section 8).

```cpp
namespace power {
enum class Status : uint8_t { Ok, Busy, Refused, NoDevice, BusError, Tripped };
enum class Role : uint8_t { Sink, PdSink, Host };
enum class Output : uint8_t { Off, On, Tripped, Blocked };
struct Vbus { uint16_t mv; bool valid; uint8_t why; };     // why: Ok, Driven, Settling
struct Contract { uint16_t mv, ma; uint8_t from; };        // None, TypeC, Pd
struct Pdo { uint16_t mv, ma; };
struct Telemetry { uint16_t mv; int32_t ma; uint32_t mw; bool enabled; };   // CN1
struct Diag;  // devices found, CH211 registers, what begin() inherited, last refusal, bus errors
Status begin();                  void poll();      // poll(): every pass through loop()
bool busy();                                       // a PD exchange is in flight (section 6)
Vbus readVbus();                 Contract contract();
Status setUsbRole(Role r);       Role usbRole();
uint8_t sourcePdos(Pdo *out, uint8_t max);         // what the source offered (PdSink)
Status requestVoltage(uint16_t mv);                // the highest fixed PDO <= mv
Status enablePdOutput(uint16_t limitMa);           // CN1 on, tripping at limitMa
Status rearmPdOutput(uint16_t limitMa);            // after a trip, on the user's request
Status disablePdOutput();        Output output();  Telemetry readPdOutputTelemetry();
const Diag &diagnostics();
}
```

Everything is synchronous, main-loop only, allocates nothing and never retries
on its own. `poll()` runs at least every 5 ms in `PdSink` (section 6), every
50 ms otherwise.

**Roles**, one at a time, every change through `Sink` (handoff §6):
- `Sink` (after `begin()`): `CC_CTRL` 0x44, USBPD off, PA0 analog, USB device;
  5 V like a Rev 1. VBUS valid.
- `PdSink`: the CC path connected through the CH211, USBPD runs the sink
  policy, USB device. VBUS valid; CN1 may be enabled.
- `Host`: Rd removed, the MCU's Rp on, PA0 HIGH, USBFS a host. VBUS invalid
  (`Driven`); CN1 off and refused.

**CN1** goes `Off` to `On` through the initialise transaction (section 5), `On`
to `Tripped` when the enable node falls, and back only through
`rearmPdOutput()`; `Blocked` (a missing device, a failed check) lasts until
`begin()`. **VBUS** is ADC1 channel 0 read at register level (`analogRead()`
reconfigures the pin), 8 samples averaged, about 81 mV of VBUS per count
(handoff §7). It is valid only with PA0 analog, outside `Host`, after settling.

| Resource | Owner | Rule for other code |
|---|---|---|
| PA0 | CHPower | No `pinMode`, `digitalWrite` or `analogRead` on `PIN_VBUS_SENSE` (an `analogRead` turns host power off). `poll()` checks PA0's configuration and falls back to `Sink`, with a diagnostic, if it changed. |
| CH211, INA226 (0x34, 0x35, 0x40) | CHPower | Nothing else addresses them: no generic INA226/INA219 library, no CH211 example code. |
| PC14/PC15, USBPD and its interrupt | CHPower | none |
| USBFS | the core's CDC device | host mode borrows it and gives it back (section 7) |
| The bus on PC18/PC19 | shared | CHPower owns it for each of its transfers (section 3) |

## 3. The I2C bus on CN2

The CH211 (0x35, and 0x34 for its fast read), the INA226 (0x40) and any CN2
accessory share PC19 (SCL) and PC18 (SDA), pulled up by 4.7 kΩ (handoff §5).

- **`INT_PIN` = 0 and polling** (handoff §8's deliberate alternative): ordinary
  open-drain I2C, clock stretching included, validated on the bench before
  anything is claimed for accessories. `poll()` uses the fast read: `PIN_STAT`
  every 20 ms while CN1 is `On`, all eight registers every 50 ms.
- **Its own master, not Wire:** GPIO open-drain at 100 kHz, a loop-count
  timeout on every SCL rise, PC18/PC19 freed from the debug port by
  `AFIO_PCFR1.SW_CFG` = 100 (masked, RM §8.3.2.1) until the next reset, and
  `GPIOC_CFGXR` written masked (PC16/PC17 are USB's). The I2C peripheral fails
  on lots whose fifth-to-last digit is 0 (DS §1.4.16), Wire's `millis()`
  timeouts stand still in the hook, and recovery needs GPIO control of SCL.
- **`INT_PIN` = 1 left by other firmware** makes the CH211 pull SCL weakly low
  while an interrupt is pending; its 0.18-0.5 mA (CH211 manual §6.2) leaves SCL
  undefined against 4.7 kΩ. `begin()` clears it with SCL push-pull (manual
  §5.8) for those transfers only: push-pull would overdrive an accessory that
  stretches the clock.
- **SDA through Q4** (AO3400A: gate on 3.3 V, source on SDA, drain on the
  CH211's `SDA_PD`). `SDA_PD` has no pull-up on the board: released, Q4's body
  diode charges it to about 3.3 V less a diode drop, above the CH211's 2.1 V
  VIH but held only by leakage. `begin()` sets `PIN_CFG.SDA_PU` (7-15 kΩ),
  which makes Q4 a normal level shifter; the level is checked on a scope.
- **One owner at a time.** Main-loop transfers run to the end under an owner
  flag. Interrupt code does no I2C: the PD interrupt sets `inhibitPending` for
  `poll()` (section 6); the reset hook masks interrupts and may break a
  transfer, since a reset follows.
- **Wire beside it**, for CN2 accessories: main loop only, never at 0x34, 0x35
  or 0x40. While I2C1 is on, a CHPower transfer clears its `PE` and saves the
  pins' configuration, and restores both after (handoff §12 asks for a test).

## 4. CH211

Registers used (CH211 manual §5.6; names as in WCH's `CH211_I2C.h`):

| Register | Address | Use |
|---|---|---|
| `PIN_STAT` | 0x00 | Bit 2 `HVLI` (the eFuse's enable node through HVIO and R36; 1 = enabled) and bit 4 `VBUS_RDY`. Never bit 3 `HVHI` for this 3.3 V signal. |
| `PIN_CFG` | 0x01 | `INT_PIN` (bits 1:0) = 0, `SDA_PU` (bit 2) = 1, the rest as found. Never `INT_PIN` = 2: HVIO is the protection's enable node. |
| `CC_CTRL` | 0x02 | Resets to 0x44. Values below. |
| `OD_CTRL` | 0x03 | CN1 inhibit `(old & ~0x0E) \| 0x0A`, permit `(old & ~0x0E) \| 0x08`, bits 7-4 kept. Never `OD1_GE` = 1 (it couples the channel to the live CC path). |
| `HVIO_KEY` | 0x05 | 0x00, its reset value (handoff §8): HVIO an input, since its pull-up, pull-down and output would act on the enable node. Written by `begin()` with CN1 inhibited and read back; a mismatch is a diagnostic. |
| `SYS_STAT` | 0x07 | Read in `begin()` (acknowledges the VBUS power-up event) and by `poll()`: `VBUS_EXIST` and `OT_RST`. |

`HVCP_CTRL` (HVCP is unconnected) and `SYS_CFG` (`LDO33_OFF` stays 0) are not
touched. `CC_CTRL` has no reserved bits and is written whole:

| State | `CC_CTRL` | Meaning |
|---|---|---|
| `Sink`, and after every reset path | 0x44 | Rd on CC1 and CC2, both paths open (the reset value) |
| `PdSink`, attach and orientation | 0x77 | Rd kept, both paths connected (`CCx_OE` = `CCx_GE` = 1) |
| `PdSink`, on CC1 / CC2 | 0x47 / 0x74 | Rd kept on both, only the active path connected |
| `Host` | 0x33 | Rd off on both, both paths connected; the MCU adds Rp |

A channel's `OE` and `GE` change together, in one write: `OE` = 1 with `GE` = 0
clamps CC low (handoff §8), and `GE` = 1 with `OE` = 0 still passes the MCU's
level through the switch's diodes (CH211 manual §5.1). `CCx_VCE` is never set.

**Over-temperature.** `OT_RST` = 1 means the CH211 has reset `CC_CTRL`,
`OD_CTRL` and `HVCP_CTRL` itself (§5.6): CN1 inhibited, CC paths open, Rd on.
That cuts the PD path in `PdSink` and drops the MCU's Rp in `Host` while PA0
still runs the boost. `poll()` treats it, or a `CC_CTRL` that differs from the
role's value, as a fault: `Sink` through section 7's step 5, CN1 `Tripped`.

**Discipline.** One register per write, read-modify-write: read, change only
the owned bits, skip the write if nothing changes (as WCH's
`CH211_I2C_SetBit`), write, read back, compare. A mismatch is `BusError`; after
one on `OD_CTRL` nothing is permitted until `begin()`. No multi-byte writes.

## 5. INA226

| Register | Value | Notes (handoff §8) |
|---|---|---|
| Configuration 0x00 | 0x4007 | continuous shunt and bus, averaging 1, 140 µs each |
| Calibration 0x05 | 0x0800 | 250 µA per current count, 6.25 mW per power count |
| Alert limit 0x07 | `floor(I_trip × 0.010 / 2.5 µV)` = `floor(I_trip × 4000)` | 2.4 A gives 9600 (0x2580) |
| Mask/Enable 0x06 | 0x8003 | shunt over-limit, ALERT inverted, latched |

- **While CN1 is permitted: no read of 0x06, no write of 0x00**, which can
  clear the latch; both happen only in `begin()` and the transaction below,
  inhibited. Telemetry is 0x01 (shunt, signed 2.5 µV), 0x02 (bus, 1.25 mV),
  0x03 (power) and 0x04 (current, signed 250 µA), one register per transfer.
  The pointer is left on 0x02, so a bus scanner's pointer-less read never
  reads 0x06.
- **Reverse current.** The eFuse blocks nothing (handoff §8): while permitted,
  `poll()` reads 0x04 and inhibits below -50 mA.

**The budget** of `enablePdOutput(limitMa)`:

```text
I_console(V) = P_console / (eta_buck * V)        console, charger, buck losses
I_allow      = (I_contract - I_console(V)) / (1 + tol) - 0.05 A
I_trip       = min(limit, 2.4 A); refused if limit > I_allow
```

`I_contract` is the accepted fixed PDO's current, or the Type-C advertisement
(1.5 A or 3.0 A) of a source without PD, capped at 3 A; default USB power
gives no budget. `P_console`, `eta_buck` and `tol` are measured on the bench;
until then 2.0 W, 0.85 and 10 %, which allow 2.57 A at 20 V/3 A (so the 2.4 A
cap) and 2.25 A at 5 V/3 A. A limit above `I_allow` is refused, not lowered.

**Initialise or rearm.** A failed step leaves CN1 inhibited, records why and
stops; nothing retries (handoff §8).
1. CH211 inhibit, read back; `HVLI` must read 0.
2. Read 0x06 once: its alert flag (kept for `diagnostics()`) says whether a
   trip was latched, and the read clears the latch. Write 0x00 = 0x4007,
   0x05 = 0x0800, 0x07 = the limit, 0x06 = 0x8003. Read back 0x00, 0x05, 0x07,
   0x06 (`(v & 0xFC03) == 0x8003`), 0xFE and 0xFF; leave the pointer on 0x02.
3. After 1 ms, 0x04 must read near 0. 0x02 (`VUSB_OUT`) may hold what C25 kept,
   since nothing discharges an unloaded CN1 after a disable, a trip or an
   inhibit: allowed up to the higher of VBUS and CN1's highest voltage in this
   run (none after `begin()`), plus 0.5 V. Above that, or a current below
   -50 mA, something drives CN1: refused.
4. Check the contract and the budget.
5. Permit (masked `OD_CTRL`), read back. `HVLI` must read 1 within the eFuse's
   ramp (about 20 ms at 20 V with C27; measured), or CHPower inhibits again and
   reports a hardware clamp (INA alert, U14, the eFuse's OVP).

While `On`, `poll()` reads `HVLI` every 20 ms. When it falls, CN1 has tripped:
CHPower inhibits through the CH211 too, leaves the INA226 latched and reports
`Tripped`. Only `rearmPdOutput()`, on the user's request, runs the steps again.

## 6. PD sink

The base is WCH's `USBPD/USBPD_SNK` (`PD_Process.c`), not `USBPD/USBPD_CH211`,
which links WCH's binary-only `libCH32_USBPD.a`. Carried over: the PHY (RM
§8.3.2.4: `USBPD_IN_HVT | USBPD_PHY_V33` for 3.3 V; `BMC_CLK_CNT` for 48 MHz;
the DMA buffers, `PD_Rx_Mode()`, `PD_Phy_SendPack()`), GoodCRC, Hard Reset
detection and the messages (fixed PDOs only; VCONN_Swap rejected). Changed:
- **Rd stays in the CH211.** The example sets `CC_PD` in `USBPD->PORT_CCx`;
  CHPower clears it, so each line has one Rd. The MCU's comparators read CC
  through the connected path: 0.22 V for a source, 0.66 V and 1.23 V for its
  Type-C advertisement (RM §21.2.15).
- **Orientation:** `CC_CTRL` 0x77 while detecting, then 0x47 or 0x74 with
  `USBPD_CONFIG.CC_SEL` on the active line. **Detach** is VBUS falling
  (`VBUS_RDY` and PA0), as the example's comments advise.
- **Nothing blocks:** its `Delay_Ms()` calls and its `while(1)` on a bad PDO
  index go; timers use `millis()`. **Not_Supported** (PD 3.0) answers the rest.
- **The interrupt** sends GoodCRC (due within tTransmit, 195 µs) and sets
  flags, no I2C. It runs from SRAM (`RAMFUNC`: about 2 cycles per instruction
  against 5 from flash). With the core's two-level nesting (`INTSYSCR` = 0x3)
  only bit 7 of a priority preempts (RM §7.5.2.37), and CHGfx's DMA (0, from
  `dmaInit()`) and USBFS (reset value 0), like USART1 (2) when used, are at
  the top level already: either they move to 0x80 (a CHGfx and a core
  change), or the bench measures their longest run against the 195 µs.
  `PD_RST_EN` stays 0 (RM §3.2.2).
- **Replies with a deadline** (the Request, within tReceiverResponse, 15 ms,
  of Source_Capabilities) go from `poll()`, hence its 5 ms rule.
- **Flash saves.** `save::` masks interrupts for each page (`Save.cpp`) and
  knows nothing of CHPower, so it is the app's rule: save only while
  `power::busy()` is false (Source_Capabilities to PS_RDY). A message lost to
  a page is retried by the source, at worst ending in a Soft_Reset.

**Contract policy:**
- `setUsbRole(PdSink)` connects CC, waits for Source_Capabilities and requests
  PDO 1 (5 V) at min(its current, 3 A). A source that sends none (the example
  waits 1 s, five times) leaves a Type-C contract at 5 V. `requestVoltage(mv)`
  takes the highest fixed PDO at or below `mv`, never above 20 V or 3 A.
- **CN1 first.** Before a request that changes the voltage or lowers the
  current, CHPower inhibits CN1; after PS_RDY it checks VBUS on PA0 (within
  5 %), recomputes the budget and leaves CN1 off for the app to enable. A
  Source_Capabilities, Hard Reset or detach while CN1 is on sets
  `inhibitPending`, and `poll()` inhibits within 5 ms: a Hard Reset leaves at
  least tPSHardReset (25 ms) before VBUS moves, and a new voltage needs
  CHPower's own Request.
- **The console does not notice**: it runs from the input buck (5-20 V in),
  except through a Hard Reset, when VBUS drops to 0 V for up to about a
  second; with a battery the console runs through it, without one it resets.
- **Inherited contracts.** A contract above 5 V outlives an MCU reset; Rev 1
  firmware takes it (the buck), and the hook and `begin()` inhibit CN1.
  `setUsbRole(PdSink)` with VBUS above 5.5 V sends Soft_Reset, so the source
  offers its capabilities again without dropping VBUS.

## 7. Host mode

The handoff's five steps (§7, in italics), and how CHPower does each:
1. *Inhibit CN1, stop the previous role, PA0 LOW.* `disablePdOutput()`; USBPD
   off; PA0 an output, LOW.
2. *PA0 analog, settle, sample; confirm VBUS absent, else refuse.* Absent means
   VBUS (from PA0) under 0.8 V, `PIN_STAT.VBUS_RDY` = 0 and
   `SYS_STAT.VBUS_EXIST` = 0. VBUS between 0.8 V and 4.0 V may be discharging:
   wait at most 1 s, then `Refused`. 0.8 V of VBUS is about 8 mV at the pin,
   ten counts, so the ADC's offset is measured on the bench.
3. *Rd off, the MCU's Rp and CC paths on, attach and orientation debounced,
   VCONN off.* `PORT_CCx` with `CC_PD` clear, `CC_CTRL` 0x33, then Rp
   `CC_PU_80` (default USB power until the boost is measured). A sink reads
   0.22-1.23 V; below is Ra or a short, above is open. Debounce 150 ms
   (tCCDebounce).
4. *Only after a valid sink: PA0 HIGH, the host controller on, readiness
   checked.* `VBUS_RDY` must read 1 within 275 ms (tVBUSON), or a fault exit.
5. *Detach, fault or exit: PA0 LOW first, sink-safe CC/Rd, CN1 inhibited; PA0
   analog and settled before sensing.* PA0 LOW; Rp off; `CC_CTRL` 0x44; PA0
   analog, readings `Settling` until VBUS is under 0.8 V or 1 s has passed.
   Detach (CC open), `VBUS_RDY` falling (overload: the boost limits at its
   3.7 A valley and folds back to about 350 mA into a short, TPS61023
   datasheet §7.3), `OT_RST` and `setUsbRole(Sink)` all take this path.

**USBFS as a host.** The core's CDC device owns USBFS: `main.cpp` starts it
before `setup()`, and its strong `USBFS_IRQHandler` is in the vector table. To
start, no core change: the app takes the peripheral over as CHSDtoUSB's
`usbmsc::begin()` does (`USBFS_IRQn` off, the core's Serial marked gone, the
D+ pull-up dropped, the SIE reset), runs a polled host ported from
`USB/USBFS/HOST_KM` with PC16/PC17 set up for a host (RM §8.3.2.4, §18.2.3),
and on leaving calls the core's `CDC_init()`, which enumerates the CDC device
again. Meanwhile there is no serial port, so no upload touch and no debug
protocol; START held 3 s still reaches the menu, where uploads work. Later, two
core changes would make it a library feature: a documented USBFS
release/restore pair instead of reaching into `USB_ENUM_OK`, and a handler
pointer in `USBFS_IRQHandler`. The INA226 does not see host current (handoff
§7). `PdSink` and `Host` start mutually exclusive.

## 8. Retained state across resets

| Event | MCU, PA0 | CH211 (CN1 permit, CC, `INT_PIN`) | INA226 (config, latch) |
|---|---|---|---|
| Power switch off and on, no USB | reset; PA0 an input | power-on state | power-on state |
| Power switch off and on, USB plugged in | reset | **kept** (fed from VBUS) | power-on state: ALERT released, CN1 held off |
| MCU-only reset (START held, upload touch, debug `B`) | reset; host power off | **kept** | **kept** |
| Crash (the fault handler spins) | not reset; PA0 as it was | kept | kept |

So an MCU-only reset can leave CN1 permitted (the INA226 still guards it), Rd
removed (a charger sees no sink until the switch is cycled with USB out), the
CC paths connected to the MCU's PD pins (question 3), and, after other
firmware, `INT_PIN` = 1 pulling SCL. **The bootloader cannot fix this:** an
inhibit needs an I2C master and two transfers in every rev1 bootloader,
100-200 B, and the rev1 Graphic Menu has tens of bytes to spare (the figure is
in [SIZES.md](https://github.com/bateske/CHGame/blob/main/platform/bootloader/SIZES.md)); and on a Rev 1, 0x35 may be a
CN2 accessory it would write to blind.

**The hook.** `void chgame_beforeReset(void)`, declared in the core's
`chgame_boot.h` with an empty weak default in `chgame_boot.c`, runs before
every software reset. CHPower's:
1. Mask interrupts. Return unless `verified.ch211` is set (section 2).
2. End any CHPower transfer in progress (release SDA, clock it high, STOP).
3. In `Host`: PA0 LOW, host power off first (handoff §7, step 5).
4. Inhibit CN1 (`OD_CTRL`, masked); if `verified.ina`, INA226 RST (ALERT
   released, Q7 holds CN1 off): two off paths that survive the reset.
5. USBPD: `PORT_CC1/2` = 0, its interrupt off.
6. `CC_CTRL` ← 0x44: Rd on both lines, paths open, VCONN off.

The reset then returns PA0 to an input, held low by R11. The hook needs no
interrupts, heap or `millis()`; every transfer has a loop-count timeout, so
with no device answering it ends in under 10 ms.

| Reset path | Where | Change |
|---|---|---|
| START held 3 s; `CHGame::exitToMenu()` | `libraries/CHGame/src/chgame/Input.cpp`: `CHGame::pollButtons()` → `chgame_exitToMenu()` → `NVIC_SystemReset()`, interrupts on | mask interrupts, call the hook, then reset |
| 1200-baud upload touch | `cores/arduino/ch32/usb/wch_usbcdc_cdc.c` sets `CDC_bootRequest`; `wch_usbcdc_handler.c`: `USBFS_IRQHandler()` → `cores/arduino/ch32/chgame_boot.c`: `chgame_enter_bootloader()` | its `__disable_irq()` moves first, then the hook |
| Debug `B` | `libraries/CHGame/src/chgame/Debug.cpp`: `dbg::execute()` → `chgame_enter_bootloader()` | covered by the line above |
| An app's own USB stack | `examples/Apps/CHSDtoUSB/UsbMsc.cpp`: `usbmsc::poll()` → `chgame_enter_bootloader()` | covered; a CHPower app never calls `NVIC_SystemReset()` itself |
| Crash, release build | `chgame_boot.c`: `chgame_fault()`, from `HardFault_Handler` (`system/CH32X035/USER/ch32x035_it.c`) and `while1_handler` (`ch32x035_misc.c`); spins until power-off | no hook. Its rev1 branch clears PA1 only (`GPIOA->BCR = 1u << 1`); adding bit 0 drives PA0 low if it is an output: host power off after a crash, at no cost |
| Crash, debug build | `chgame_fault()` → `Debug.cpp`: `chgame_fault_park()`, interrupts on; without a park, a press of A → `NVIC_SystemReset()` | the park's `B` and upload touch run the hook in a crashed program, accepted since every transfer is bounded; the A restart gets a hook call before its reset |
| Bootloader resets (self-update, RUN after an upload) | `platform/bootloader` | none: no program has run since the hook |

The core changes take effect once the core is released or staged (CLAUDE.md
rule 6). `chgame_boot.h` defines `CHGAME_HAS_BEFORE_RESET`, and `CHPower.h`
refuses to build without it, except under `CHSIM`. **Crashes and hangs** rely
on hardware: the INA226's latch still trips CN1, U14 clamps the enable on a
3.3 V brownout, and switching off removes host power (R11 holds HOST_EN low).
A hung MCU keeps whatever was on (handoff §8).

## 9. Apps

In `examples/Apps/`, built for `rev1` only (`BOARDS = ("rev1",)` in their
`tools/game.py`, honoured by `chgame check`, `export` and `stage.py`), off the
casino card unless the owner decides otherwise. Working names; sim scripts run
against the `host/` models, the checks on the bench.
- **CHPowerScope (diagnostics):** VBUS and its validity, the CH211's
  registers, `HVLI`, what `begin()` inherited, the INA226's readings, and CN1
  with a chosen limit (0.5 to 2.4 A): enable, disable, rearm. *Checks:* VBUS
  within 0.2 V of a meter at each PD voltage; bus voltage within 1 %, current
  within 2 % + 10 mA; CN1 agrees with `HVLI` and the meter; a load ramp trips
  within 10 % of the limit and stays off until rearm; a minute of telemetry on
  a logic analyser shows no access to 0x06 and no write to 0x00.
- **CHPDSink (PD trigger):** the source's PDOs; requests 5, 9, 12, 15 or 20 V;
  the contract and VBUS. *Checks:* every offered voltage in both orientations,
  VBUS within 5 %; a source without PD stays at 5 V with no error; replugging
  recovers; with CN1 on, CN1 is at 0 V before VBUS moves (scope).
- **CHPowerBank (battery bank host):** powers a phone or USB device from the
  battery. *Checks:* refused with a charger plugged in or VBUS uncertain; a
  phone (C to C) and an OTG adapter get 5 V within tVBUSON, both orientations;
  unplugging drops PA0 and restores `CC_CTRL` 0x44 within 100 ms; overload and
  short end host mode with a report; CN1 stays at 0 V.
- **CHUsbKeys (host HID test):** a USB keyboard's keys on screen. *Checks:* a
  low-speed and a full-speed keyboard enumerate and type; replugging works;
  START held 3 s reaches the menu, and an upload works from there.

## 10. Bench plan

Instruments: a USB-C PD source (fixed 5-20 V PDOs at 3 A), a 5 V source
without PD, a USB-C power meter that shows the contract, an electronic load
(20 V, 3 A, ramp), a multimeter, an oscilloscope, a logic analyser on CN2, a
phone, an OTG adapter, a USB keyboard. The handoff's §12 rows, then this
design's:
- [ ] **Output defaults.** Power-on with rev1 firmware and each app: no
  host-power pulse on VUSB, no CN1 enable (scope `PD_EN` and CN1).
- [ ] **Expansion/debug.** SCL and SDA idle high with `INT_PIN` = 0; `SDA_PD`
  before and after `SDA_PU` (above 2.1 V); `begin()` recovers SCL after a test
  build set `INT_PIN` = 1; Wire with a CN2 accessory; a debugger after a reset.
- [ ] **VBUS sense.** The divider's factor at 5-20 V against a meter; the ADC's
  offset at 0 V (vSafe0V is ten counts); invalid while PA0 is driven.
- [ ] **Sink and host.** Both orientations, attach and detach, host entry
  refused with a live source or an uncertain reading, source shutdown, Rd
  restored; host load and current limit measured apart from CN1.
- [ ] **Protected CN1.** Inhibit, permit and `HVLI` against the meter;
  calibrated telemetry; trips at several limits and contracts; latched until
  rearm; no latch-clearing reads; how long C25 holds `VUSB_OUT` unloaded.
- [ ] **Resets and supplies.** Cold start, MCU-only reset, USB in and out, the
  switch with and without USB, battery only, a CH211 reset (VBUS and `VSYS_SW`
  off), an INA226 reset, a 3.3 V brownout; any time an enabled output persists.
- [ ] **CN1 and VBUS across resets.** CN1 on at 9 V and 0.5 A, then host mode
  with a phone: power-on, START held 3 s, an upload (1200-baud touch), debug
  `B`, a deliberate crash. CN1 and host VBUS reach 0 V before the reset; record
  how long it takes and what `begin()` reports after.
- [ ] **PD timing.** The longest CHGfx DMA and USB interrupt (a pin toggled on
  entry and exit) against 195 µs; GoodCRC during a flush with Serial busy; a
  flash save during a contract.
- [ ] **The MCU's `CC_PD`.** The RM (§21.2.15) calls bit 1 of `PORT_CCx`
  reserved, reset value 1; WCH's example uses it as a pull-down. With
  `CC_CTRL` 0x77 and the CH211's Rd off, measure CC1 to GND, set and clear.
- [ ] **`P_console`:** the worst input power (backlight, audio, SD, a flat
  battery charging) at 5, 9 and 20 V, and the buck's efficiency.
- [ ] **The I2C peripheral:** the MCU's lot number; does Wire work on it.
- [ ] **Charging while switched off after a PD session**, the CC path still
  connected to an unpowered MCU: the CC voltage and the contract.

## 11. Open questions for the owner

1. **Names:** CHPower and `power::`, and the four apps.
2. **Licence:** the PD and host code come from WCH's examples, whose notice
   limits use to WCH chips. Acceptable in an Apache-2.0 or MIT library?
3. **Rd after resets.** If `CC_PD` is a real Rd, on at reset, a crash with the
   CC path connected leaves two Rd in parallel (about 2.5 kΩ), which a source
   may take for Ra. Clear `PORT_CCx` in every rev1 bootloader, or accept it?
4. **`SDA_PD`:** rely on `SDA_PU`, or add a pull-up on a later board?
5. **Host current:** default USB power only (80 µA Rp), or 1.5 A once the
   boost and the battery are measured?
6. **A watchdog** while CN1 or host power is on? A reset turns host power off,
   but CN1 stays permitted (still guarded) until a CHPower program starts.
7. **PD in games:** may a game use PD, calling `poll()` every 5 ms while it
   waits for the next frame?
8. **PD interrupt priority:** move CHGfx's DMA and the core's USBFS to
   preemption level 1 for every program, or rely on their measured lengths?
9. **CC while switched off:** keep the CC path connected for the whole PD
   session (the sink can always answer), or open it once the contract is
   settled (safe with the MCU off, but the sink cannot answer)?
10. **The casino card:** does any of the four apps belong on a rev1 card?
    Answered 2026-11-19: the two built, CHPDTrigger and CHUSBHost, are on
    the rev1 card's APPS folder (PD TRIGGER, USB HOST), not on rev0's.

## 12. Powered hubs (added 2026-11-20)

Built and measured on the bench with Apple's USB-C Digital AV Multiport
Adapter (05AC:100F inside), with [CHPDHubProbe](../libraries/CHUSBHost/examples/CHPDHubProbe/README.md)'s
logs. The goal: a hub with its own charger powers the board, and charges
it, while the board stays the USB host.

**What the adapter is.** Unpowered, a sink (Rd) and a 2-port USB 2.0 hub.
With its charger in, a dual-role port: it sinks when it sees our Rp and
sources when it sees our Rd. As a source it offers one PDO, 5 V 0.1 A
(dual-role power, USB communications, dual-role data, unconstrained), asks
for VCONN_Swap and then DR_Swap (it is a UFP data-wise). As a sink with
its charger plugged in later, it asks for PR_Swap, but only of a source
that talks PD. Its hub chip runs on the cable's 5 V.

**What the board does** (all opt-in; CHPDTrigger is unchanged):

| Case | Mechanism | Where |
|---|---|---|
| Plugged in powered | **Try.SNK**: on a sink's Rd, the board is a sink (`CC_CTRL` 0x44) for up to 450 ms before it powers the port; a powered dual-role hub sources it (the adapter within 210 ms) | `host::setTrySink()`, state `TRY_SINK` |
| The host role | the PD sink accepts the hub's **DR_Swap** (`pd::allowDataSwap()`), the header's data-role bit then DFP; the host runs on the hub's 5 V without driving PA0 or touching CC | `pd::dfp()`, `host::enterPowered()` |
| Powered later | while host power is on, a **PD source** on the line in use: 5 V 0.5 A, dual-role power and data, PD 2.0 (no collision avoidance on the 80 uA Rp). A **PR_Swap** is accepted: PA0 low and analog, VBUS let fall under 0.8 V (about 400 ms with the hub's load), Rp off and the CH211's Rd on (`CC_CTRL` 0x47/0x74), PS_RDY as a sink, then the hub's PS_RDY and offer | `pd::startSource()`, `host::PD_SWAP` |
| Chained hubs | up to three hubs (the root and two behind it), four ports each, seven devices; a low-speed device behind them gets PRE packets as behind one | `host::HUBS` (`Hubs.cpp`; `HUB` stays one level) |

**Rules kept.** 5 V never meets another source's: PA0 stays refused while
VBUS is present, and the swap waits for vSafe0V before Rd. A detach while
PD talks on the line needs 20 ms (tPDDebounce's most) and five looks
without the sink's Rd: the hub's PD chip settling once read as an unplug
in under 8 ms. Every time the host cuts its 5 V, the PD log says why and
both lines' levels (`K_HOST`).

**What is not done.** The hub's devices restart in a power swap (its hub
chip loses VBUS; the host and the app ride through it, `reconnecting()`).
The adapter's 0.1 A offer is taken as it is; whether it offers more after
a Discover Identity, or with a bigger charger, is not known. A PC that
asked for DR_Swap would be accepted (none has been seen to). HDMI through
the adapter is DisplayPort Alt Mode, out of reach of a full-speed MCU.

**What it costs a sketch that wants none of it.** Each piece is opt-in and
linked only when named: Try.SNK and `enterPowered()` with their calls,
the PD source, the swap, its ride-through and the power-off notes with
`PD_SWAP`, chained hubs with `HUBS`. A desktop-style sketch with a
keyboard, a mouse and a hub (CHGfx, the CHGame library's drawing and
audio, CHSd's read path) measured 23,112 B of image and 12,436 B of
static RAM after them, against 23,076 B and 12,416 B before.

