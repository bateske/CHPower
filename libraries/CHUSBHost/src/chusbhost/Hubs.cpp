// Chained hubs (HUBS): a hub on the port and up to two more behind it (a hub
// plugged into a hub), four ports each, the devices on them, seven at most,
// enumerated as on the port itself. The ports are read one every 15 ms
// (GET_PORT_STATUS), never in a wait. HUB (Hub.cpp) is the one-level hub,
// smaller, for the sketches that want no more.
#include "Core.h"

namespace host {
using namespace in;

static const uint8_t NB = 7, NH = 3;   // devices behind hubs (dev(1..7), at address 2 + their slot), hubs
static Dev seven[NB];

#ifndef CHSIM
// A hub in use: its device slot (0 the root), its ports (four at most), a
// device first seen on each port (for its settle time).
struct HubSt {
    uint8_t slot, nPorts;
    uint32_t seenAt[4];
};
static HubSt hubs[NH];
static uint8_t nHubs;
static uint32_t portAt;
static uint8_t hubNext, portNext;
static const char *hubNote;

static bool portStatus(uint8_t p, uint8_t *s) {
    uint16_t n = 0;
    return ctl(0xA3, 0, 0, p, 4, s, &n) == E_OK && n == 4;
}

// The slot of the device on a hub's port (0: none).
static uint8_t onPort(uint8_t hubSlot, uint8_t p) {
    for (uint8_t i = 1; i <= NB; i++) {
        const Dev &d = dev(i);
        if (d.addr && d.parent == hubSlot && d.port == p) return i;
    }
    return 0;
}

// A device gone: and if it was a hub, everything behind it, and the hub.
static void drop(uint8_t slot) {
    dev(slot) = Dev();
    for (uint8_t h = 0; h < nHubs; h++)
        if (hubs[h].slot == slot) {
            hubs[h] = hubs[--nHubs];
            break;
        }
    for (uint8_t i = 1; i <= NB; i++)
        if (dev(i).addr && dev(i).parent == slot) drop(i);
}

// GET_HUB_DESCRIPTOR of the device selected: its ports (four at most), then
// each port's power, and its power-good time (100 ms at least). 0 done, 1
// try again, 2 gone.
static uint8_t hubUp(uint8_t slot) {
    uint16_t n = 0;
    if (ctl(0xA0, 6, 0x2900, 0, 8, big, &n) != E_OK || n < 7) return 1;
    uint8_t np = big[2] > 4 ? 4 : big[2];
    if (!np || nHubs >= NH) return 1;
    uint16_t good = (uint16_t)(big[5] * 2);
    HubSt &h = hubs[nHubs++];
    h = HubSt{slot, np, {0, 0, 0, 0}};
    if (!slot) nPorts = np;
    for (uint8_t p = 1; p <= np; p++) ctl(0x23, 3, 8, p, 0, nullptr);   // SET_FEATURE PORT_POWER
    return pause(good > 100 ? good : 100) ? 0 : 2;
}

static void servicePort(uint8_t hi, uint8_t p) {
    HubSt &h = hubs[hi];
    uint8_t hubSlot = h.slot;
    uint8_t s[4];
    select(dev(hubSlot));
    if (!portStatus(p, s)) return;
    // Every change bit that is set is cleared (C_PORT_CONNECTION 16 ...
    // C_PORT_RESET 20), so none is left to look like a new event.
    for (uint8_t b = 0; b < 5; b++)
        if (s[2] >> b & 1) ctl(0x23, 1, (uint16_t)(16 + b), p, 0, nullptr);
    if (s[2] & 0x08) hubNote = "HUB PORT OVERCURRENT";
    uint8_t have = onPort(hubSlot, p);
    if (!(s[0] & 1)) {                                      // nothing on it
        if (have) {
            drop(have);
            recount();
        }
        h.seenAt[p - 1] = 0;
        return;
    }
    if (have) return;
    // A new device: 100 ms to settle (tATTDB), then a port reset.
    uint32_t now = millis();
    if (!h.seenAt[p - 1]) {
        h.seenAt[p - 1] = now ? now : 1;
        return;
    }
    if (now - h.seenAt[p - 1] < 100) return;
    h.seenAt[p - 1] = 0;
    uint8_t slot = 1;
    while (slot <= NB && dev(slot).addr) slot++;
    if (slot > NB) {
        hubNote = "TOO MANY DEVICES";
        return;
    }
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
    Dev &d = dev(slot);
    d = Dev();
    d.low = (s[1] & 2) != 0;
    uint16_t padLen = 0;
    uint8_t padIntf = 0;
    enumerating = true;
    deadline = millis() + 2500;
    bool ok = enumDevice(d, (uint8_t)(2 + slot), padLen, padIntf);
    enumerating = false;
    d.parent = hubSlot;
    d.port = p;
    if (ok && d.hub) {
        // A hub behind a hub: its ports too (HUBS: two more hubs at most).
        uint8_t r = d.low ? 1 : hubUp(slot);
        hubNote = r ? "HUB IN HUB: NOT USED" : nullptr;
        if (r) d = Dev();
    } else if (ok) {
        setupDev(d, padLen, padIntf);
        hubNote = nullptr;
    } else {
        d = Dev();
        hubNote = "HUB DEVICE FAILED";
    }
    recount();
}

// The hub on the port (the root device, selected): a fresh table of hubs.
static uint8_t start() {
    nHubs = 0;
    nPorts = 0;
    return hubUp(0);
}
static void ready() {
    hubNote = nullptr;
    hubNext = portNext = 0;
}
// One port of one hub each 15 ms, every hub's in turn.
static void service(uint32_t now) {
    if (now - portAt < 15 || !nHubs) return;
    portAt = now;
    if (hubNext >= nHubs) hubNext = portNext = 0;
    if (++portNext > hubs[hubNext].nPorts) {
        portNext = 1;
        hubNext = (uint8_t)((hubNext + 1) % nHubs);
    }
    servicePort(hubNext, portNext);
    stat = hubNote ? hubNote : (hasKb || hasMs || hasPad) ? "READY" : nHubs > 1 ? "HUBS READY" : "HUB READY";
}
#else
static uint8_t start() { return 0; }
static void ready() {}
static void service(uint32_t) {}
#endif

static const Hub HUBS_OPS = {start, ready, service};
void useHubs() {
    behind = seven;
    nDev = 1 + NB;
    hub = &HUBS_OPS;
}

}  // namespace host
