// The Switch Pro pad driver (SWITCH_PRO): 057E:2009, a Switch Pro pad or a
// pad in Switch mode. Without its handshake it leaves the bus after 2 s;
// with it, full reports (0x30): buttons in bytes 3-5, the sticks as 12-bit
// pairs in 6-11 (Y up).
#include "Core.h"

namespace host {
using namespace in;

static int8_t swAxis(uint16_t v, bool flip) {
    int16_t s = (int16_t)(((int16_t)v - 2048) / 14);
    if (flip) s = (int16_t)-s;
    return (int8_t)(s < -127 ? -127 : s > 127 ? 127 : s);
}
static void report(Ep &, const uint8_t *r, uint8_t len) {
    padRaw(r, len);
    if (len < 12 || r[0] != 0x30) return;      // (0x81: its answers to the handshake)
    Pad &pv = padState();
    uint8_t R = r[3], S = r[4], L = r[5];
    // As drawn: B A Y X, L R, ZL ZR, - +, LS RS.
    pv.buttons = (uint32_t)((R >> 2 & 1) | (R >> 3 & 1) << 1 | (R & 1) << 2 | (R >> 1 & 1) << 3 |
                            (L >> 6 & 1) << 4 | (R >> 6 & 1) << 5 | (L >> 7 & 1) << 6 | (R >> 7 & 1) << 7 |
                            (S & 3) << 8 | (S >> 3 & 1) << 10 | (S >> 2 & 1) << 11);
    static const uint8_t HAT[16] = {8, 4, 0, 8, 2, 3, 1, 8, 6, 5, 7, 8, 8, 8, 8, 8};   // down up right left
    pv.hat = HAT[L & 15];
    pv.lt = L & 0x80 ? 255 : 0;
    pv.rt = R & 0x80 ? 255 : 0;
    pv.x = swAxis((uint16_t)(r[6] | (r[7] & 15) << 8), false);
    pv.y = swAxis((uint16_t)(r[7] >> 4 | r[8] << 4), true);
    pv.rx = swAxis((uint16_t)(r[9] | (r[10] & 15) << 8), false);
    pv.ry = swAxis((uint16_t)(r[10] >> 4 | r[11] << 4), true);
}

#ifndef CHSIM
// The handshake (what Linux's hid-nintendo sends) on its interrupt OUT
// endpoint: hello, 3 Mbit, hello, USB only and no timeout; then full
// reports (0x30) and player 1's light.
static void setup(Dev &, Ep &) {
    static const uint8_t SEQ[] = {2, 0x80, 0x02, 2, 0x80, 0x03, 2, 0x80, 0x02, 2, 0x80, 0x04,
                                  12, 0x01, 0, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x03, 0x30,
                                  12, 0x01, 1, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x30, 0x01};
    for (uint8_t i = 0; i < sizeof SEQ && xOut; i = (uint8_t)(i + 1 + SEQ[i])) {
        memcpy(txb, SEQ + i + 1, SEQ[i]);
        USBFSH->HOST_TX_LEN = SEQ[i];
        if (transact((uint8_t)(PID_OUT << 4 | xOut), outTog, 200) == E_OK) outTog ^= T_TOG;
        pause(40);
    }
    Pad &pv = padState();
    pv.nButtons = 12;
    pv.axes = 0x7F;
    pv.nswitch = true;
}
#else
static void setup(Dev &, Ep &) {}
#endif

static const Driver SWITCH_DRIVER = {setup, nullptr, report, nullptr, nullptr};
void useSwitchPro() {
    usePad();
    drv[K_SPAD] = &SWITCH_DRIVER;
}

}  // namespace host
