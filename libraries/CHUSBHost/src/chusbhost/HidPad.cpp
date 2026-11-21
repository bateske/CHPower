// The HID gamepad driver (GAMEPAD): gamepads and joysticks in report
// protocol, where their controls sit found in their report descriptor.
#include "Core.h"

namespace host {
using namespace in;

struct Field {
    uint16_t bit;           // offset in the report (after the report ID)
    uint8_t size;           // 0: not present
    int32_t lo, hi;         // logical range
};
enum : uint8_t { F_X, F_Y, F_Z, F_RX, F_RY, F_RZ, F_HAT, F_GAS, F_BRAKE, F_N };
static Field fld[F_N];
static uint16_t btnBit;
static uint8_t btnN, padId;
static bool padIds;         // the descriptor uses report IDs

static uint32_t bits(const uint8_t *r, uint8_t len, uint16_t at, uint8_t n) {
    uint32_t v = 0;
    for (uint8_t i = 0; i < n; i++) {
        uint16_t b = (uint16_t)(at + i);
        if ((b >> 3) < len && (r[b >> 3] >> (b & 7) & 1)) v |= 1u << i;
    }
    return v;
}
static int32_t fieldVal(const Field &f, const uint8_t *r, uint8_t len) {
    uint32_t v = bits(r, len, f.bit, f.size);
    if (f.lo < 0 && f.size < 32 && (v >> (f.size - 1) & 1)) return (int32_t)(v | ~0u << f.size);   // signed
    return (int32_t)v;
}
static int8_t axis(const Field &f, const uint8_t *r, uint8_t len) {
    if (!f.size || f.hi <= f.lo) return 0;
    int32_t v = fieldVal(f, r, len);
    // (32-bit: a 64-bit division would cost 1 KB of flash.)
    int32_t span = f.hi - f.lo, off = v - f.lo;
    while (span > 0x7FFFFF) {
        span >>= 8;
        off >>= 8;
    }
    int32_t s = span ? off * 254 / span - 127 : 0;
    return (int8_t)(s < -127 ? -127 : s > 127 ? 127 : s);
}
static uint8_t trigger(const Field &f, const uint8_t *r, uint8_t len) {
    return f.size ? (uint8_t)(axis(f, r, len) + 127) : 0;
}

// The HID report descriptor: the Input fields of the joystick or gamepad
// application collection (all of them if there is none), by usage. True
// if it is a gamepad: one was found, or no keyboard was (a keyboard
// without a boot interface is the keyboard driver's).
static bool desc(Ep &, const uint8_t *d, uint16_t n) {
    Pad &pv = padState();
    memset(fld, 0, sizeof fld);
    btnN = 0;
    btnBit = 0;
    padIds = false;
    padId = 0;
    uint16_t page = 0, usages[8];
    uint8_t nUse = 0, rsize = 0, rcount = 0, id = 0, depth = 0;
    bool inPad = false, anyApp = false, kbFound = false, padFound = false;
    uint16_t umin = 0, umax = 0;
    int32_t lo = 0, hi = 0;
    uint16_t bit = 0;                       // within the current report ID's report
    for (uint16_t p = 0; p < n;) {
        uint8_t h = d[p];
        if (h == 0xFE) {                    // a long item: skipped
            if (p + 1 >= n) break;
            p = (uint16_t)(p + 3 + d[p + 1]);
            continue;
        }
        uint8_t sz = (uint8_t)(h & 3);
        if (sz == 3) sz = 4;
        uint32_t u = 0;
        for (uint8_t i = 0; i < sz && p + 1 + i < n; i++) u |= (uint32_t)d[p + 1 + i] << (8 * i);
        int32_t sv = sz == 1 ? (int8_t)u : sz == 2 ? (int16_t)u : (int32_t)u;
        uint8_t tag = (uint8_t)(h & 0xFC);
        p = (uint16_t)(p + 1 + sz);
        // Usages as page << 8 | id (the pages here all fit a byte).
        uint16_t us = (uint16_t)(sz == 4 ? (u >> 16) << 8 | (u & 0xFF) : page << 8 | (u & 0xFF));
        switch (tag) {
        case 0x04: page = (uint16_t)u; break;                           // Usage Page
        case 0x14: lo = sv; break;                                      // Logical Minimum
        case 0x24: hi = (lo >= 0 && sz < 4) ? (int32_t)u : sv; break;   // Logical Maximum
        case 0x74: rsize = (uint8_t)u; break;                           // Report Size
        case 0x94: rcount = (uint8_t)u; break;                          // Report Count
        case 0x84:                                                      // Report ID
            id = (uint8_t)u;
            padIds = true;
            bit = 0;
            break;
        case 0x08:                                                      // Usage
            if (nUse < 8) usages[nUse++] = us;
            break;
        case 0x18: umin = us; break;                                    // Usage Minimum
        case 0x28: umax = us; break;                                    // Usage Maximum
        case 0xA0:                                                      // Collection
            depth++;
            if (u == 1 && depth == 1) {                                 // Application
                uint16_t a = nUse ? usages[0] : 0;
                anyApp = true;
                inPad = a == 0x0104 || a == 0x0105;                     // Joystick, Game Pad
                padFound |= inPad;
                kbFound |= a == 0x0106;                                 // Keyboard
            }
            nUse = 0;
            umin = umax = 0;
            break;
        case 0xC0:                                                      // End Collection
            if (depth) depth--;
            break;
        case 0x80: {                                                    // Input
            bool take = inPad || !anyApp;
            if (take && !padId && padIds) padId = id;
            if (take && (!padIds || id == padId) && !(u & 1)) {        // (not a constant)
                for (uint8_t i = 0; i < rcount; i++) {
                    uint16_t w = 0;
                    if (nUse) w = usages[i < nUse ? i : nUse - 1];
                    else if (umin) w = (uint16_t)(umin + i <= umax ? umin + i : umax);
                    uint16_t at = (uint16_t)(bit + i * rsize);
                    if ((w >> 8) == 9) {                                // Button page
                        if (!btnN) btnBit = at;
                        if (btnN < 32 && rsize == 1) btnN++;
                    } else {
                        int8_t f = -1;
                        if (w >= 0x0130 && w <= 0x0135) f = (int8_t)(F_X + (w - 0x0130));
                        else if (w == 0x0139) f = F_HAT;
                        else if (w == 0x02C4) f = F_GAS;               // Simulation: Accelerator
                        else if (w == 0x02C5) f = F_BRAKE;             // Brake
                        if (f >= 0 && !fld[f].size && rsize && rsize <= 32) fld[f] = Field{at, rsize, lo, hi};
                    }
                }
            }
            bit = (uint16_t)(bit + rsize * rcount);
            nUse = 0;
            umin = umax = 0;
            break;
        }
        case 0x90:                                                      // Output and Feature
        case 0xB0:                                                      // end the local state
            nUse = 0;
            umin = umax = 0;
            break;
        default:
            break;
        }
    }
    // Which axes: left X/Y; right Z/Rz when there are both (most DirectInput
    // pads), else Rx/Ry; the triggers whatever is left.
    pv.nButtons = btnN;
    bool zr = fld[F_Z].size && fld[F_RZ].size;
    uint8_t a = (uint8_t)((fld[F_X].size ? AX_X : 0) | (fld[F_Y].size ? AX_Y : 0) | (fld[F_HAT].size ? AX_HAT : 0));
    if (zr || fld[F_RX].size) a |= AX_RX;
    if (zr || fld[F_RY].size) a |= AX_RY;
    if ((zr && fld[F_RX].size) || (!zr && fld[F_Z].size) || fld[F_BRAKE].size) a |= AX_LT;
    if ((zr && fld[F_RY].size) || (!zr && fld[F_RZ].size) || fld[F_GAS].size) a |= AX_RT;
    pv.axes = a;
    return padFound || !kbFound;
}

static void report(Ep &, const uint8_t *r, uint8_t len) {
    padRaw(r, len);
    if (padIds) {
        if (!len || r[0] != padId) return;
        r++;
        len--;
    }
    Pad &pv = padState();
    pv.buttons = bits(r, len, btnBit, btnN);
    pv.x = axis(fld[F_X], r, len);
    pv.y = axis(fld[F_Y], r, len);
    bool zr = fld[F_Z].size && fld[F_RZ].size;
    pv.rx = axis(zr ? fld[F_Z] : fld[F_RX], r, len);
    pv.ry = axis(zr ? fld[F_RZ] : fld[F_RY], r, len);
    pv.lt = fld[F_BRAKE].size ? trigger(fld[F_BRAKE], r, len) : trigger(zr ? fld[F_RX] : fld[F_Z], r, len);
    pv.rt = fld[F_GAS].size ? trigger(fld[F_GAS], r, len) : trigger(zr ? fld[F_RY] : fld[F_RZ], r, len);
    pv.hat = 8;
    const Field &h = fld[F_HAT];
    if (h.size) {
        int32_t v = fieldVal(h, r, len) - h.lo;
        int32_t span = h.hi - h.lo;
        if (v >= 0 && v <= span) pv.hat = (uint8_t)(span == 3 ? v * 2 : v & 7);
    }
}

static const Driver GAMEPAD_DRIVER = {nullptr, desc, report, nullptr, nullptr};
void useGamepad() {
    usePad();
    drv[K_PAD] = &GAMEPAD_DRIVER;
}

}  // namespace host
