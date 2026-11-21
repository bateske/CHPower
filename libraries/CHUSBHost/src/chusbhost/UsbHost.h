// Battery-powered USB host: keyboards, mice and gamepads, on the port
// directly or behind a hub (two hubs deep at most).
//
// What a sketch pays for. The core (power, the port, the transfers,
// enumeration) is in every sketch that calls enter(). Each kind of device
// is a driver of its own, linked only when the sketch names it in begin():
//
//     host::begin(host::KEYBOARD);                  // a keyboard, no more
//     host::begin(host::KEYBOARD | host::MOUSE | host::HUB);
//     host::begin(host::ALL);                       // everything below
//
// begin() is inline: with the kinds written as constants, as above, the
// drivers not named are never referenced and never linked (their code and
// their RAM). A device no driver takes enumerates and is shown as not
// supported. LOG adds the probe log (probeLine() and the rest, 1.6 KB of
// RAM): what the host did, for finding out why a device is not read.
//
// Entry follows the board handoff's five steps (docs/design.md section 7):
// CN1 off and the PD sink paused (pwr::pausePd(): it is not linked for
// that); PA0 low, then analog, and VBUS must be absent (a charger plugged
// in refuses); CC: the CH211's Rd off and both paths to the MCU, whose
// 80 uA Rp (default USB power) then looks for a sink's Rd, debounced
// 150 ms; only then PA0 high (the boost), and VBUS must be ready within
// 275 ms. Detach, a fault or leave() drives PA0 low first.
//
// USBFS is the core's serial port until then: enter() takes it over as
// CHSDtoUSB does, and leave() gives it back (CDC_init()), so there is no
// serial port, upload or debug protocol while in host mode.
//
// The USB side is WCH's HOST_KM example's driver (ch32x035_usbfs_host.c,
// V1.0.2, and its hub handling in app_km.c), reworked: one hub level (up to
// four ports), keyboards and mice in boot protocol, gamepads in report
// protocol with their report descriptor parsed (buttons, X/Y, Z/Rz, Rx/Ry,
// hat switch), Xbox 360 pads (XInput: not HID, a fixed report) and Switch
// Pro pads (their handshake, then their full report).
#pragma once
#include <stdint.h>

namespace host {

// The drivers, for begin().
enum : uint16_t {
    KEYBOARD = 0x01,    // boot keyboards, and those that declare no boot interface
    MOUSE = 0x02,       // boot mice
    GAMEPAD = 0x04,     // HID gamepads and joysticks (their report descriptor parsed)
    XINPUT = 0x08,      // Xbox 360 pads (and the many that pass for one)
    SWITCH_PRO = 0x10,  // Switch Pro pads, and pads in Switch mode (057E:2009)
    HUB = 0x20,         // one hub on the port, four of its ports (without it a hub is not supported)
    LOG = 0x40,         // the probe log
    PD_SWAP = 0x80,     // PD as the source while powering, and a hub's power role swap (not in ALL)
    HUBS = 0x120,       // HUB, and up to two hubs behind it: four ports each, seven devices (more RAM)
    PADS = GAMEPAD | XINPUT | SWITCH_PRO,
    ALL = 0x17F,        // every driver, HUBS for the hubs (not PD_SWAP)
};
void useKeyboard();
void useMouse();
void useGamepad();
void useXInput();
void useSwitchPro();
void useHub();
void useHubs();
void useLog();
void usePdSwap();
// Before enter(): the drivers to link and use. Calling it again adds more.
__attribute__((always_inline)) inline void begin(uint16_t what) {
    if (what & KEYBOARD) useKeyboard();
    if (what & MOUSE) useMouse();
    if (what & GAMEPAD) useGamepad();
    if (what & XINPUT) useXInput();
    if (what & SWITCH_PRO) useSwitchPro();
    if (what & 0x100) useHubs();
    else if (what & HUB) useHub();
    if (what & LOG) useLog();
    if (what & PD_SWAP) usePdSwap();
}

enum State : uint8_t {
    OFF,
    WAIT_SINK,      // Rp on: plug in an OTG adapter, a hub or a C-to-C device
    POWERING,       // a sink: PA0 high, waiting for VBUS
    WAIT_DEVICE,    // VBUS on: waiting for D+/D- to show a device
    READY,          // enumerated: at least one device a driver takes (or a hub waiting for one)
    UNSUPPORTED,    // enumerated, but nothing this sketch's drivers take
    FAILED,         // enumeration failed
    SETTLING,       // power off after a detach: waiting for VBUS to fall
    TRY_SINK,       // setTrySink(): a sink seen, we are a sink a moment (a powered hub sources us)
};
// Try.SNK on attach: a sink's Rd seen, the board is a sink for up to 450 ms
// before it powers the port. A powered dual-role hub (Apple's USB-C Digital
// AV Multiport Adapter, with its charger in) then becomes the source: host
// mode ends with status() "PORT POWERS US", state() OFF and the serial port
// back, for the app's PD sink and enterPowered(). Anything else is powered
// as ever, 0.45 s later. Off by default.
void setTrySink(bool on);

// nullptr when host mode is on; otherwise why not. Needs pwr::begin() to
// have found the power section.
const char *enter();
void leave();               // host power off, the serial port back, the PD sink again if it ran
// Host mode on a port someone else powers: a powered PD hub that has given
// us the host role (pd::dfp()). The hub's VBUS runs (and charges) the board;
// CC stays the PD sink's and PA0 is never driven, so there is no 5 V of
// ours. lost() (the app's: the PD contract or the role gone) ends it,
// checked every 10 ms and during every transfer's wait; the host then goes
// OFF with status() saying why, and the serial port comes back.
const char *enterPowered(bool (*lost)());
bool poweredByPort();       // in host mode through enterPowered()
// A hub's power swap (PD_SWAP) under way, or its devices not all back from
// it yet (8 s at most): a bus-powered hub restarts when our 5 V hands over to
// its own, so its devices leave and enumerate again. A sketch can hold its
// views (no unplug sounds) meanwhile.
bool reconnecting();
// 5 V on now, without waiting for a device's Rd on CC (for a USB-C device
// without its CC resistors): only while waiting for a device, and only
// after the same check as ever that VBUS is absent (no 5 V onto another
// source's). Off when the device leaves D+/D-, or 5 s without one. False
// when it cannot be now.
bool force();
bool forcedOn();            // 5 V on by force()
void service();             // from loop(), every pass (it polls the power section's checks too)
State state();
const char *status();       // a line for the screen
const char *device();       // what is plugged in: "HUB KEYBOARD MOUSE", "LOW SPEED GAMEPAD" ...
uint16_t vbusOnMs();        // PA0 high to VBUS ready, the last time
uint16_t offMs();           // detach seen to PA0 low, the last time
uint16_t attaches();
uint16_t faults();          // host power cut by a fault (VBUS fell, over-temperature, no VBUS)
uint8_t hubPorts();         // a hub's ports (0: no hub)
uint32_t reports();         // reports received since enter()
uint32_t lastReportAt();    // millis() of the last one (0: none yet)
bool keyboard();            // one is plugged in (and KEYBOARD is used)
bool mouse();
bool gamepad();

// Keyboards (KEYBOARD): key presses as events (a press of a key not held
// before).
struct Key {
    uint8_t usage;          // the HID usage (0x04 'a' ... ; 0x28 Enter, 0x2A Backspace ...)
    uint8_t mods;           // the modifier byte when pressed (bit 1/5 shift, 0/4 ctrl, 2/6 alt, 3/7 GUI)
    char ch;                // its character on a US layout, 0 for none ('\n' Enter, '\b' Backspace)
};
bool nextKey(Key &k);       // the next press, oldest first; false when none is waiting
uint8_t mods();             // the modifier byte now
const uint8_t *keysDown();  // the six usages held now (0 = none)

// Mice (MOUSE): movement summed since the last takeMouse().
void takeMouse(int16_t &dx, int16_t &dy, int8_t &wheel);
uint8_t buttons();          // bit 0 left, 1 right, 2 middle (3, 4: back, forward)

// Gamepads (GAMEPAD, XINPUT, SWITCH_PRO): the controls, read from its
// reports. Axes are scaled to -127..127 (0 at rest), triggers to 0..255.
enum : uint8_t { AX_X = 1, AX_Y = 2, AX_RX = 4, AX_RY = 8, AX_HAT = 16, AX_LT = 32, AX_RT = 64 };
struct Pad {
    uint32_t buttons;       // bit n: button n + 1
    uint8_t nButtons;       // how many the descriptor declares (11 for XInput)
    uint8_t axes;           // AX_ bits: which controls it has
    int8_t x, y, rx, ry;    // left stick, right stick (Rx/Ry, or Z/Rz)
    uint8_t lt, rt;         // triggers
    uint8_t hat;            // 0 up, 1 up-right ... 7 up-left; 8 centred
    bool xinput;            // an Xbox 360 pad: buttons 1-11 are A B X Y LB RB BACK START LS RS GUIDE
    bool nswitch;           // a Switch Pro pad: 1-12 are B A Y X L R ZL ZR - + LS RS
};
const Pad &pad();
const uint8_t *rawReport(uint8_t &len);   // the gamepad's last report as sent (64 bytes at most)

// The probe log (LOG; in the simulator, made up): what the host did, as
// text lines (oldest first), and each endpoint's polls.
uint8_t probeCount();
const char *probeLine(uint8_t i);
struct EpStat {
    uint8_t dev, ep, kind;
    uint32_t ok, nak, err;
    uint8_t last;           // the last error's code
    uint8_t data[10], len;  // the last report's first bytes
};
uint8_t probeEps();
const EpStat &probeEp(uint8_t i);

}  // namespace host
