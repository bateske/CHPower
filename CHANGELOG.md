# Changelog

Each library has its own version, in its `library.properties`.

## CHUSBHost 0.2.0, CHPower 0.1.0 (2026-11-21): the first release

### CHPower 0.1.0

- `pwr::`: the CH211 and the INA226 on their own I2C, PA0 (VBUS sense and
  host power), the protected output CN1 with its cut-off limit and its
  meter. CN1 follows the charger's contract, and is checked every 10 ms.
- `pd::`: the USB-PD sink. Supports PD 2.0 and 3.0, fixed and PPS
  supplies, Try.SNK, and data and power role swaps for powered hubs.
- `plog::`: the PD log, with 160 events in RAM and the last 16 kept for
  the next run.
- Each piece is linked on its first call: the core, the PD sink, CN1 and
  the log.
- Example: CHPDTrigger, a USB-PD trigger and bench supply.

### CHUSBHost 0.2.0

- `host::`: a battery-powered USB host. Drivers for boot keyboards, boot
  mice, HID gamepads, Xbox 360 pads, Switch Pro pads, hubs (`HUB`, and
  `HUBS` for a hub behind a hub), powered USB-C PD hubs (`PD_SWAP`) and a
  probe log (`LOG`). A sketch links only the drivers it names in
  `host::begin()`.
- New since 0.1.0: `host::capsLock()`. A keyboard's error reports
  (rollover, POST fail, undefined) now update only the modifiers, so keys
  still held are not pressed a second time when the error clears.
- Examples: CHUSBHost, a view for each device and a log; CHPDHubProbe, a
  guided bench test for a powered hub.
