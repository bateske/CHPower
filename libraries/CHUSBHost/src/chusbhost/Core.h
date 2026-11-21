// What the host's core (Host.cpp) and its drivers share (not for sketches).
//
// The core enumerates, polls each claimed endpoint and hands its reports to
// the driver of its kind, through the table drv[]; a driver is in the table
// (and linked at all) only once the sketch's begin() names it. So is the
// hub (hub), the probe log (probe) and the gamepads' shared state (padClear).
#pragma once
#include <Arduino.h>
#include <string.h>
#include "UsbHost.h"

namespace host {

// An endpoint's kind: which driver reads it. K_PAD is any other HID
// interface: its report descriptor decides (a gamepad, or a keyboard that
// declares no boot interface).
enum Kind : uint8_t { K_NONE, K_KBD, K_MOUSE, K_PAD, K_XPAD, K_SPAD, K_N };

struct Ep {
    uint8_t num, tog, interval, maxPkt, kind, intf, rid;
    uint32_t at;
};
// [0] the device on the port itself (a hub or a device); with the hub
// driver, [1..7] behind it or behind a hub behind it, at address 2 + their
// slot, each with its hub's slot and port.
struct Dev {
    uint8_t addr;           // 0: no device
    bool low, hub;
    uint8_t ep0, nEp;
    Ep ep[3];
    uint8_t parent, port;   // behind a hub: the hub's slot (0 the root) and its port
};

struct Driver {
    void (*setup)(Dev &d, Ep &e);       // after SET_CONFIGURATION, the device selected
    // A K_PAD interface's report descriptor (n may be 0): true if this
    // driver takes it (it may set e.rid).
    bool (*desc)(Ep &e, const uint8_t *d, uint16_t n);
    void (*report)(Ep &e, const uint8_t *r, uint8_t len);
    void (*reset)(bool fresh);          // its state to rest (fresh: enter(), Caps Lock too)
    void (*after)();                    // after each round of polls
};
struct Hub {
    // The root device is a hub: its ports powered. 0 done, 1 try again, 2 gone.
    uint8_t (*start)();
    void (*ready)();                    // the hub enumerated: its ports from the first
    void (*service)(uint32_t now);      // from READY, every pass
};
// PD_SWAP (PdSwap.cpp): the PD source side while we power the port, and the
// power role swap a powered hub asks for.
struct PdSwap {
    void (*start)(uint8_t line);        // host power on: our offer out on that CC line
    void (*stop)();
    void (*service)();
    uint8_t (*phase)();                 // pd::swapPhase(): 1-3 under way, 4 done, 5 failed
    bool (*lost)();                     // after the swap: the hub's 5 V gone
    // Why the host cut its 5 V (1 CC gone, 2 VBUS fell, 3 over-temperature,
    // 4 swap failed), for the PD log with CC's levels; ms: how long CC was gone.
    void (*note)(uint8_t why, uint16_t ms);
    void (*recounted)();                // the devices changed: the ride-through ends when all are back
    bool (*riding)();                   // reconnecting()
};
struct Probe {
    void (*ctl)(const uint8_t *setup, uint8_t result, uint16_t got, const uint8_t *data);
    void (*dev)(const uint8_t *dd);     // a device descriptor
    void (*desc)(const uint8_t *x);     // an interface (type 4) or endpoint (5) descriptor
    void (*poll)(uint8_t dev, const Ep &e, uint8_t result, const uint8_t *r, uint8_t len);
    void (*status)(const char *s);      // a new status line
    void (*forced)(uint8_t cc1, uint8_t cc2);   // CC's levels when 5 V was forced
};

namespace in {
extern const Driver *drv[K_N];
extern const Hub *hub;
extern const Probe *probe;
extern const PdSwap *swap;
extern void (*padClear)();              // the gamepads' state to rest (Pad.cpp)

extern State st;
extern const char *stat;
extern uint8_t nPorts;
extern bool hasKb, hasMs, hasPad;
extern bool detached;                   // seen during enumeration
extern uint8_t xOut, outTog;            // a device's interrupt OUT endpoint (a pad's lights, a handshake), its toggle
extern Dev root;
extern Dev *behind;                     // the hubs' seven (Hub.cpp), or nullptr
extern uint8_t nDev;                    // 1, or 8 with the hub driver
inline Dev &dev(uint8_t i) { return i ? behind[i - 1] : root; }
extern uint8_t big[384];                // a configuration, then a report descriptor (or a hub's)

#ifndef CHSIM
extern uint8_t txb[64];
enum : uint8_t { PID_OUT = 0x01, PID_DATA0 = 0x03, PID_IN = 0x09, PID_NAK = 0x0A, PID_DATA1 = 0x0B,
                 PID_SETUP = 0x0D, PID_STALL = 0x0E };
enum : uint8_t { E_OK = 0, E_DISCON = 0x16, E_BUF = 0x17, E_XFER = 0x20, E_UNKNOWN = 0xFE };
enum : uint8_t { T_TOG = 0x40 };
void select(const Dev &d);
uint8_t transact(uint8_t pidEp, uint8_t tog, uint16_t timeout);
uint8_t ctl(uint8_t type, uint8_t req, uint16_t val, uint16_t idx, uint16_t len, uint8_t *data,
            uint16_t *got = nullptr);
bool pause(uint16_t ms);
bool above(uint8_t line, uint16_t cmp);     // CC's level on a line against a comparator
// One device, at address 0 (its port just reset), to addr; then set up:
// each endpoint's driver, and a K_PAD interface's descriptor read.
bool enumDevice(Dev &d, uint8_t addr, uint16_t &padLen, uint8_t &padIntf);
void setupDev(Dev &d, uint16_t padLen, uint8_t padIntf);
extern bool enumerating;
extern uint32_t deadline;
#endif
void recount();                         // what the devices still there provide, and device()
void clearInput(bool fresh);
}  // namespace in

// The gamepads' shared state (Pad.cpp).
Pad &padState();
void padRaw(const uint8_t *r, uint8_t len);
void usePad();

}  // namespace host
