// The Xbox 360 pad driver (XINPUT): vendor class 0xFF/0x5D/0x01, not HID,
// a fixed 20-byte report (type 0, length 20). Xbox One and Series pads
// (GIP) are another protocol, not read.
#include "Core.h"

namespace host {
using namespace in;

// Its Y axes point up; HID's point down, as here.
static int8_t xAxis(const uint8_t *p, bool flip) {
    int16_t s = (int16_t)((int16_t)(p[0] | p[1] << 8) >> 8);
    if (flip) s = (int16_t)~s;              // (-128..127 to 127..-128, no overflow)
    return (int8_t)(s < -127 ? -127 : s);
}
static void report(Ep &, const uint8_t *r, uint8_t len) {
    padRaw(r, len);
    if (len < 14 || r[0] != 0x00) return;  // (type 1 is the ring's LED status)
    Pad &pv = padState();
    uint16_t b = (uint16_t)(r[2] | r[3] << 8);
    // A B X Y, LB RB, BACK START, LS RS, GUIDE as buttons 1-11.
    static const uint8_t MAP[11] = {12, 13, 14, 15, 8, 9, 5, 4, 6, 7, 10};
    uint32_t out = 0;
    for (uint8_t i = 0; i < 11; i++)
        if (b >> MAP[i] & 1) out |= 1u << i;
    pv.buttons = out;
    static const uint8_t HAT[16] = {8, 0, 4, 8, 6, 7, 5, 6, 2, 1, 3, 2, 8, 0, 4, 8};   // up down left right
    pv.hat = HAT[b & 15];
    pv.lt = r[4];
    pv.rt = r[5];
    pv.x = xAxis(r + 6, false);
    pv.y = xAxis(r + 8, true);
    pv.rx = xAxis(r + 10, false);
    pv.ry = xAxis(r + 12, true);
}

#ifndef CHSIM
// Some pads that pass for a Microsoft one wait for this read before they
// send anything (Linux's xpad sends it too).
static void setup(Dev &, Ep &) {
    uint8_t b[20];
    ctl(0xC1, 0x01, 0x0100, 0, 20, b);
    Pad &pv = padState();
    pv.nButtons = 11;
    pv.axes = 0x7F;
    pv.xinput = true;
}
#else
static void setup(Dev &, Ep &) {}
#endif

static const Driver XINPUT_DRIVER = {setup, nullptr, report, nullptr, nullptr};
void useXInput() {
    usePad();
    drv[K_XPAD] = &XINPUT_DRIVER;
}

}  // namespace host
