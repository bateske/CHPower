// PD_SWAP: the PD source side while the host powers the port, so a hub
// that gets its own charger later can take over the power (a PR_Swap) and
// leave the board the USB host, charging. Linked only when begin() names it:
// it is the one piece of the host that links CHPower's PD code.
#include "Core.h"
#include <CHPower.h>

namespace host {
using namespace in;

// The ride-through: a bus-powered hub restarts when our 5 V goes (the swap
// needs VBUS at 0 V before the hub's own 5 V), so its devices leave and come
// back. Until rideUntil, or until as many devices are back as there were
// (rideWant), reconnecting() says so.
static uint32_t rideUntil;
static uint8_t rideWant;
static uint8_t devCount() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < nDev; i++)
        if (dev(i).addr) n++;
    return n;
}
static bool sRiding() {
    if (rideUntil && (int32_t)(millis() - rideUntil) >= 0) rideUntil = 0;
    return rideUntil != 0;
}
bool reconnecting() { return swap && sRiding(); }
static void sRecounted() {
    if (rideUntil && devCount() >= rideWant) rideUntil = 0;     // all back
}

static void sStart(uint8_t line) {
    pd::allowPowerSwap(true);
    pd::startSource(line);
}
static void sStop() {
    rideUntil = 0;
    pd::stop();
}
static void sService() {
    pd::service();
    uint8_t ph = pd::swapPhase();
    if (ph >= 1 && ph <= 3 && !rideUntil) {    // a swap begins: its devices will be back
        rideWant = devCount();
        rideUntil = (millis() + 8000) | 1;
    }
}

#ifndef CHSIM
// Both lines' levels for the PD log: 0 under 0.22 V, 1 to 0.66, 2 to 1.23, 3 over.
static void sNote(uint8_t why, uint16_t ms) {
    uint8_t v = 0;
    for (uint8_t c = 1; c <= 2; c++)
        v = (uint8_t)(v << 4 | (above(c, 2 << 5) + above(c, 5 << 5) + above(c, 7 << 5)));
    plog::add(plog::K_HOST, why, v, ms);
}
#else
static void sNote(uint8_t, uint16_t) {}
#endif
static uint8_t sPhase() { return pd::swapPhase(); }
// After the swap, the host lasts as long as the hub's 5 V: the PD sink's
// detach (VBUS falling) ends it.
static bool sLost() {
    pd::State s = pd::state();
    return s == pd::OFF || s == pd::DETECT;
}

static const PdSwap SWAP = {sStart, sStop, sService, sPhase, sLost, sNote, sRecounted, sRiding};
void usePdSwap() { swap = &SWAP; }

}  // namespace host
