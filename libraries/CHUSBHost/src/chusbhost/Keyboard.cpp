// The keyboard driver (KEYBOARD): boot keyboards, and a keyboard that
// declares no boot interface (its Keyboard collection found in its report
// descriptor, its reports taken as the boot layout after their report ID,
// which such keyboards use). Caps Lock is kept here, and its LED lit.
#include "Core.h"

namespace host {
using namespace in;

static uint8_t modsV, held[6], capsLock;
static bool capsChanged;
static Key keyQ[16];
static uint8_t keyHead, keyCount;

uint8_t mods() { return modsV; }
const uint8_t *keysDown() { return held; }
bool nextKey(Key &k) {
    if (!keyCount) return false;
    k = keyQ[keyHead];
    keyHead = (uint8_t)((keyHead + 1) & 15);
    keyCount--;
    return true;
}

// A usage's character on a US layout (0: none).
static char usChar(uint8_t u, bool shift) {
    static const char NUM[] = "1234567890", NUMS[] = "!@#$%^&*()";
    static const char PUN[] = "-=[]\\#;'`,./", PUNS[] = "_+{}|~:\"~<>?";
    static const char PAD[] = "/*-+\n1234567890.";
    if (u >= 0x04 && u <= 0x1D) {
        bool up = shift != (capsLock != 0);
        return (char)((up ? 'A' : 'a') + u - 0x04);
    }
    if (u >= 0x1E && u <= 0x27) return shift ? NUMS[u - 0x1E] : NUM[u - 0x1E];
    if (u == 0x28) return '\n';
    if (u == 0x2A) return '\b';
    if (u == 0x2B) return '\t';
    if (u == 0x2C) return ' ';
    if (u >= 0x2D && u <= 0x38) return shift ? PUNS[u - 0x2D] : PUN[u - 0x2D];
    if (u >= 0x54 && u <= 0x63) return PAD[u - 0x54];
    return 0;
}

// A boot keyboard report: modifiers, reserved, six usages (after the
// report ID, if it has one). Each usage not held in the report before is a
// press.
static void report(Ep &e, const uint8_t *r, uint8_t len) {
    if (e.rid) {
        if (!len || r[0] != e.rid) return;
        r++;
        len--;
    }
    if (len < 8) return;
    modsV = r[0];
    bool shift = (r[0] & 0x22) != 0;
    for (uint8_t i = 2; i < 8; i++) {
        uint8_t u = r[i];
        if (u < 4) continue;                    // none, or the keyboard's rollover error
        bool was = false;
        for (uint8_t j = 0; j < 6; j++) was |= held[j] == u;
        if (was) continue;
        if (u == 0x39) {                        // Caps Lock: ours to keep (and its LED)
            capsLock ^= 1;
            capsChanged = true;
        }
        if (keyCount < 16) {
            Key &k = keyQ[(keyHead + keyCount) & 15];
            k.usage = u;
            k.mods = r[0];
            k.ch = usChar(u, shift);
            keyCount++;
        }
    }
    memcpy(held, r + 2, 6);
}

static void reset(bool fresh) {
    modsV = 0;
    memset(held, 0, sizeof held);
    keyHead = keyCount = 0;
    if (fresh) capsLock = 0;
}

// A K_PAD interface's descriptor (the gamepad driver did not take it): a
// keyboard if it has a Keyboard application collection; its report ID is
// the one in force at that collection's first Input.
static bool desc(Ep &e, const uint8_t *d, uint16_t n) {
    uint16_t page = 0, use = 0;
    uint8_t id = 0, depth = 0;
    bool inKb = false;
    for (uint16_t p = 0; p < n;) {
        uint8_t h = d[p];
        if (h == 0xFE) {                        // a long item: skipped
            if (p + 1 >= n) break;
            p = (uint16_t)(p + 3 + d[p + 1]);
            continue;
        }
        uint8_t sz = (uint8_t)(h & 3);
        if (sz == 3) sz = 4;
        uint32_t u = 0;
        for (uint8_t i = 0; i < sz && p + 1 + i < n; i++) u |= (uint32_t)d[p + 1 + i] << (8 * i);
        p = (uint16_t)(p + 1 + sz);
        switch (h & 0xFC) {
        case 0x04: page = (uint16_t)u; break;                                       // Usage Page
        case 0x08: use = (uint16_t)(sz == 4 ? (u >> 16) << 8 | (u & 0xFF) : page << 8 | (u & 0xFF)); break;
        case 0x84: id = (uint8_t)u; break;                                          // Report ID
        case 0xA0:                                                                  // Collection
            if (++depth == 1 && u == 1) inKb = use == 0x0106;
            use = 0;
            break;
        case 0xC0:                                                                  // End Collection
            if (depth) depth--;
            break;
        case 0x80:                                                                  // Input
            if (inKb) {
                e.rid = id;
                return true;
            }
            use = 0;
            break;
        case 0x90:                                                                  // Output, Feature
        case 0xB0:
            use = 0;
            break;
        default:
            break;
        }
    }
    return false;
}

#ifndef CHSIM
static void setup(Dev &, Ep &e) {
    ctl(0x21, 0x0B, 0, e.intf, 0, nullptr);     // SET_PROTOCOL: boot
    ctl(0x21, 0x0A, 0, e.intf, 0, nullptr);     // SET_IDLE: only changes
}
// Caps Lock's LED on the keyboards (SET_REPORT, output report).
static void after() {
    if (!capsChanged) return;
    capsChanged = false;
    for (uint8_t i = 0; i < nDev; i++) {
        Dev &d = dev(i);
        for (uint8_t j = 0; d.addr && j < d.nEp; j++)
            if (d.ep[j].kind == K_KBD) {
                uint8_t led = capsLock ? 2 : 0;
                select(d);
                ctl(0x21, 9, 0x0200, d.ep[j].intf, 1, &led);
            }
    }
}
#else
static void setup(Dev &, Ep &) {}
static void after() {}
#endif

static const Driver KEYBOARD_DRIVER = {setup, desc, report, reset, after};
void useKeyboard() { drv[K_KBD] = &KEYBOARD_DRIVER; }

}  // namespace host
