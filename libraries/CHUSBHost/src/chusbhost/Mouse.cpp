// The mouse driver (MOUSE): boot mice.
#include "Core.h"

namespace host {
using namespace in;

static uint8_t btn;
static int16_t mdx, mdy;
static int8_t mwheel;

uint8_t buttons() { return btn; }
void takeMouse(int16_t &dx, int16_t &dy, int8_t &wheel) {
    dx = mdx;
    dy = mdy;
    wheel = mwheel;
    mdx = mdy = 0;
    mwheel = 0;
}

// A boot mouse report: buttons, x, y, then the wheel.
static void report(Ep &, const uint8_t *r, uint8_t len) {
    if (len < 3) return;
    btn = r[0] & 0x1F;
    mdx = (int16_t)(mdx + (int8_t)r[1]);
    mdy = (int16_t)(mdy + (int8_t)r[2]);
    if (len >= 4) mwheel = (int8_t)(mwheel + (int8_t)r[3]);
}

static void reset(bool) {
    btn = 0;
    mdx = mdy = 0;
    mwheel = 0;
}

#ifndef CHSIM
static void setup(Dev &, Ep &e) {
    ctl(0x21, 0x0B, 0, e.intf, 0, nullptr);     // SET_PROTOCOL: boot
    ctl(0x21, 0x0A, 0, e.intf, 0, nullptr);     // SET_IDLE: only changes
}
#else
static void setup(Dev &, Ep &) {}
#endif

static const Driver MOUSE_DRIVER = {setup, nullptr, report, reset, nullptr};
void useMouse() { drv[K_MOUSE] = &MOUSE_DRIVER; }

}  // namespace host
