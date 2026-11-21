// The hub (HUB): one hub on the port, up to four of its ports, each device
// on them enumerated as on the port itself (a hub behind it is not used:
// HUBS, Hubs.cpp, chains them). Its ports are read one every 25 ms
// (GET_PORT_STATUS), never in a wait.
#include "Core.h"

namespace host {
using namespace in;

static Dev four[4];

#ifndef CHSIM
static uint32_t portAt, seenAt[5];
static uint8_t portNext;
static const char *hubNote;

static bool portStatus(uint8_t p, uint8_t *s) {
    uint16_t n = 0;
    return ctl(0xA3, 0, 0, p, 4, s, &n) == E_OK && n == 4;
}

static void servicePort(uint8_t p) {
    Dev &d = dev(p);
    uint8_t s[4];
    select(root);
    if (!portStatus(p, s)) return;
    // Every change bit that is set is cleared (C_PORT_CONNECTION 16 ...
    // C_PORT_RESET 20), so none is left to look like a new event.
    for (uint8_t b = 0; b < 5; b++)
        if (s[2] >> b & 1) ctl(0x23, 1, (uint16_t)(16 + b), p, 0, nullptr);
    if (s[2] & 0x08) hubNote = "HUB PORT OVERCURRENT";
    if (!(s[0] & 1)) {                                      // nothing on it
        if (d.addr) {
            d = Dev();
            recount();
        }
        seenAt[p] = 0;
        return;
    }
    if (d.addr) return;
    // A new device: 100 ms to settle (tATTDB), then a port reset.
    uint32_t now = millis();
    if (!seenAt[p]) {
        seenAt[p] = now | 1;
        return;
    }
    if (now - seenAt[p] < 100) return;
    seenAt[p] = 0;
    ctl(0x23, 3, 4, p, 0, nullptr);                         // SET_FEATURE PORT_RESET
    bool done = false;
    for (uint8_t i = 0; i < 50 && !done; i++) {
        if (!pause(2)) return;
        if (portStatus(p, s)) done = (s[2] & 0x10) != 0;
    }
    ctl(0x23, 1, 20, p, 0, nullptr);                        // CLEAR_FEATURE C_PORT_RESET
    if (!done || !(s[0] & 2)) {
        hubNote = "HUB PORT RESET FAILED";
        return;
    }
    if (!pause(10)) return;
    d.low = (s[1] & 2) != 0;
    uint16_t padLen = 0;
    uint8_t padIntf = 0;
    enumerating = true;
    deadline = millis() + 2500;
    bool ok = enumDevice(d, (uint8_t)(2 + p), padLen, padIntf);
    enumerating = false;
    if (ok && d.hub) {
        d = Dev();                                          // a hub behind a hub: HUBS for that
        hubNote = "HUB IN HUB: NOT USED";
    } else if (ok) {
        setupDev(d, padLen, padIntf);
        hubNote = nullptr;
    } else {
        d = Dev();
        hubNote = "HUB DEVICE FAILED";
    }
    recount();
}

// GET_HUB_DESCRIPTOR: its ports (four at most here), then each port's
// power, and its power-good time (100 ms at least).
static uint8_t start() {
    uint16_t n = 0;
    if (ctl(0xA0, 6, 0x2900, 0, 8, big, &n) != E_OK || n < 7) return 1;
    nPorts = big[2] > 4 ? 4 : big[2];
    if (!nPorts) return 1;
    for (uint8_t p = 1; p <= nPorts; p++) ctl(0x23, 3, 8, p, 0, nullptr);   // SET_FEATURE PORT_POWER
    uint16_t good = (uint16_t)(big[5] * 2);
    return pause(good > 100 ? good : 100) ? 0 : 2;
}
static void ready() {
    hubNote = nullptr;
    portNext = 0;
}
static void service(uint32_t now) {
    if (now - portAt < 25) return;
    portAt = now;
    portNext = (uint8_t)(portNext % nPorts + 1);
    servicePort(portNext);
    stat = hubNote ? hubNote : (hasKb || hasMs || hasPad) ? "READY" : "HUB READY";
}
#else
static uint8_t start() { return 0; }
static void ready() {}
static void service(uint32_t) {}
#endif

static const Hub HUB_OPS = {start, ready, service};
void useHub() {
    behind = four;
    nDev = 5;
    hub = &HUB_OPS;
}

}  // namespace host
