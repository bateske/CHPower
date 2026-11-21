// Battery-powered USB host, its core (UsbHost.h): power, the port, the
// transfers, enumeration and the polling of each claimed endpoint. What the
// reports mean is the drivers' (Keyboard.cpp, Mouse.cpp, HidPad.cpp,
// XInput.cpp, SwitchPro.cpp), the hub's (Hub.cpp) and the log's (Log.cpp),
// each linked only when the sketch's begin() names it.
//
// The transfer layer and the hub handling are WCH's USBFS host driver and
// HOST_KM example (ch32x035_usbfs_host.c and app_km.c, V1.0.2, (c) 2021
// Nanjing Qinheng Microelectronics, "used for microcontroller manufactured
// by Nanjing Qinheng Microelectronics": this runs on their CH32X035),
// reworked: per-device address and speed, every hub change bit cleared,
// gamepads, and the hub's ports read in turn instead of a blocking poll.
#include <CHPower.h>
#include "Core.h"
#include <chpower/Hw.h>

#ifndef CHSIM
extern "C" {
extern volatile uint8_t USB_ENUM_OK;        // the core's CDC: nonzero while its Serial is live
void CDC_init(void);                        // the core's CDC: USB device again
}
#endif

namespace host {
namespace in {
const Driver *drv[K_N];
const Hub *hub;
const Probe *probe;
const PdSwap *swap;
void (*padClear)();
State st = OFF;
const char *stat = "";
uint8_t nPorts;
bool hasKb, hasMs, hasPad;
bool detached;
uint8_t xOut, outTog;
Dev root;
Dev *behind;
uint8_t nDev = 1;
uint8_t big[384];
}  // namespace in
using namespace in;

static char devText[32];
static uint16_t onMs, offMsV, nAttach, nFault;
static uint32_t t0, nReports, lastAt;
static uint8_t line, ccN;
static bool pdWas;          // the PD sink ran before enter(): leave() starts it again

static uint8_t padKind;     // the kind of the gamepad plugged in (for device())

State state() { return st; }
const char *status() { return stat; }
const char *device() { return devText; }
uint16_t vbusOnMs() { return onMs; }
uint16_t offMs() { return offMsV; }
uint16_t attaches() { return nAttach; }
uint16_t faults() { return nFault; }
uint8_t hubPorts() { return nPorts; }
uint32_t reports() { return nReports; }
uint32_t lastReportAt() { return lastAt; }
bool keyboard() { return hasKb; }
bool mouse() { return hasMs; }
bool gamepad() { return hasPad; }

void in::clearInput(bool fresh) {
    hasKb = hasMs = hasPad = false;
    padKind = K_NONE;
    for (uint8_t k = 1; k < K_N; k++)
        if (drv[k] && drv[k]->reset) drv[k]->reset(fresh);
    if (padClear) padClear();
}

static void describe() {
    char *q = devText;
    const char *parts[5] = {nPorts ? "HUB " : root.low ? "LOW SPEED " : "FULL SPEED ", hasKb ? "KEYBOARD " : "",
                            hasMs ? "MOUSE " : "",
                            hasPad ? (padKind == K_XPAD ? "XBOX PAD" : padKind == K_SPAD ? "SWITCH PAD" : "GAMEPAD") : "",
                            ""};
    for (const char *s : parts)
        while (*s && q < devText + sizeof devText - 1) *q++ = *s++;
    *q = 0;
}

void in::recount() {
    hasKb = hasMs = hasPad = false;
    padKind = K_NONE;
    for (uint8_t i = 0; i < nDev; i++) {
        Dev &d = dev(i);
        for (uint8_t j = 0; d.addr && j < d.nEp; j++) {
            uint8_t k = d.ep[j].kind;
            hasKb |= k == K_KBD;
            hasMs |= k == K_MOUSE;
            if (k >= K_PAD && !hasPad) {
                hasPad = true;
                padKind = k;
            }
        }
    }
    if (!hasPad && padClear) padClear();
    describe();
    if (swap) swap->recounted();           // (a power swap's devices coming back)
}

#ifndef CHSIM
// ---------------------------------------------------------------------------
// USBFS host registers (WCH's ch32x035_usb.h)
// ---------------------------------------------------------------------------
enum : uint8_t { UC_DMA_EN = 0x01, UC_CLR_ALL = 0x02, UC_RESET_SIE = 0x04, UC_INT_BUSY = 0x08,
                 UC_LOW_SPEED = 0x40, UC_HOST_MODE = 0x80 };
enum : uint8_t { UH_PORT_EN = 0x01, UH_BUS_RESET = 0x02, UH_LOW_SPEED = 0x04 };
enum : uint8_t { UH_SOF_EN = 0x40, UH_PRE_PID_EN = 0x80, UH_EP_TX_EN = 0x40, UH_EP_RX_EN = 0x08 };
enum : uint8_t { UIF_DETECT = 0x01, UIF_TRANSFER = 0x02, UMS_DEV_ATTACH = 0x01, UMS_DM_LEVEL = 0x02 };
enum : uint8_t { UIS_TOG_OK = 0x40, UIS_H_RES_MASK = 0x0F };
enum : uint8_t { R_TOG = 0x80, R_AUTO_TOG = 0x10, T_AUTO_TOG = 0x10 };
static const uint32_t AFIO_UDM_PUE = 0x03, AFIO_UDP_PUE = 0x0C, AFIO_PHY_V33 = 0x40, AFIO_IOEN = 0x80;
static const uint32_t USBPD_IN_HVT = 1u << 9, USBPD_PHY_V33 = 1u << 8;
enum : uint16_t { PA_CC_AI = 1 << 0, CC_PU_80 = 3 << 2, CC_CMP_MASK = 7 << 5, CC_CMP_22 = 2 << 5,
                  CC_CMP_123 = 7 << 5 };

alignas(4) static uint8_t rxb[64];
alignas(4) uint8_t in::txb[64];
static uint8_t ep0 = 8;     // the selected device's

// The CC line's level against a comparator: true if above.
bool in::above(uint8_t l, uint16_t cmp) {
    volatile uint16_t *p = l == 1 ? &USBPD->PORT_CC1 : &USBPD->PORT_CC2;
    uint16_t was = *p & CC_CMP_MASK;            // (PD_SWAP's receiver is on 0.66 V: put back)
    *p = (uint16_t)((*p & ~(CC_CMP_MASK | PA_CC_AI)) | cmp);
    waitUs(3);
    bool a = (*p & PA_CC_AI) != 0;
    *p = (uint16_t)((*p & ~(CC_CMP_MASK | PA_CC_AI)) | was);
    return a;
}
// A sink's Rd against our 80 uA: 0.25 to 0.61 V. Below 0.22 V is Ra or a
// short, above 1.23 V nothing.
static bool sinkOn(uint8_t l) { return above(l, CC_CMP_22) && !above(l, CC_CMP_123); }
// Why the host cut its 5 V, to the PD log (PD_SWAP's: no log without PD).
static void noteOff(uint8_t why, uint16_t ms = 0) {
    if (swap) swap->note(why, ms);
}

// Detach: the sink's Rd gone for 8 ms (tPDDebounce is 10-20 ms). Host power
// goes off right here, so even a blocking enumeration ends it at once.
static bool powered;                // PA0 high
static uint32_t openSince;
// Forced: 5 V on without a sink's Rd (a USB-C device without its CC
// resistors, as some are): no detach from CC then; D+/D- and a timeout end it.
static bool forced, forceReq;
static uint8_t goneN;               // looks in a row without the sink's Rd
// Powered by the port (enterPowered()): a PD hub's VBUS, CC the PD sink's,
// PA0 never driven. The end is the app's lost(), not CC.
static bool sinkHost, swapOwned;   // swapOwned: sinkHost by PD_SWAP's power swap (it owns the PD)
static bool (*lostFn)();
// Try.SNK (USB Type-C 4.5.2.2.10): on a sink's Rd, first be a sink a moment.
// A powered dual-role hub then sources us; anything else is powered as ever.
// trySnk: on; snkTried: done for this attach (cleared when the Rd goes).
static bool trySnk, snkTried;
void setTrySink(bool on) { trySnk = on; }
static bool ccGone() {
    if (sinkHost) {
        if (!lostFn || !lostFn()) return false;
        detached = true;
        return true;
    }
    uint32_t now = millis();
    if (swap && powered) {
        uint8_t ph = swap->phase();
        if (ph >= 1 && ph <= 4) {               // a power swap: CC and VBUS go, by design
            openSince = 0;
            return false;
        }
    }
    if (forced || sinkOn(line)) {
        openSince = 0;
        goneN = 0;
        return false;
    }
    if (!openSince) openSince = now ? now : 1;
    if (goneN < 255) goneN++;
    // tPDDebounce: 8 ms, or 20 ms (tPDDebounce's most) and five looks while
    // PD talks on this line (its signalling, or a hub's PD chip settling,
    // must not look like an unplug).
    bool pdOn = swap && powered;
    if (now - openSince < (pdOn ? 20u : 8u) || goneN < (pdOn ? 5 : 1)) return false;
    noteOff(1, (uint16_t)(now - openSince));
    goneN = 0;
    if (powered) {
        pwr::pa0Low();
        powered = false;
        offMsV = (uint16_t)(millis() - openSince);
    }
    detached = true;
    return true;
}
uint32_t in::deadline;              // an enumeration gives up then
bool in::enumerating;               // (the deadline applies only then: it once cut every report 2.5 s in)
bool in::pause(uint16_t ms) {
    while (ms--) {
        delay(1);
        if (swap && (powered || swapOwned)) swap->service();   // (a swap must not wait for an enumeration)
        if (ccGone()) return false;
    }
    return true;
}

static void rpOn() {
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO;
    RCC->AHBPCENR |= RCC_AHBPeriph_USBPD;
    AFIO->CTLR |= USBPD_IN_HVT | USBPD_PHY_V33;
    NVIC_DisableIRQ(USBPD_IRQn);
    USBPD->CONFIG = 0;
    pwr::ccSet(pwr::CC_HOST);                   // the CH211's Rd off, both paths to the MCU
    USBPD->PORT_CC1 = CC_PU_80 | CC_CMP_22;     // then our Rp (default USB power)
    USBPD->PORT_CC2 = CC_PU_80 | CC_CMP_22;
}
static void rpOff() {
    USBPD->PORT_CC1 = 0;
    USBPD->PORT_CC2 = 0;
    pwr::ccSet(pwr::CC_SINK);
}

// The serial port's USBFS, taken over (as CHSDtoUSB's usbmsc::begin()).
static void takeUsb() {
    NVIC_DisableIRQ(USBFS_IRQn);
    USB_ENUM_OK = 0;
    AFIO->CTLR &= ~AFIO_UDP_PUE;                // the PC sees the port leave
    USBFSH->BASE_CTRL = UC_RESET_SIE | UC_CLR_ALL;
    waitUs(20);
    USBFSH->BASE_CTRL = 0;
}
static void hostOn() {
    AFIO->CTLR = (AFIO->CTLR & ~(AFIO_UDP_PUE | AFIO_UDM_PUE)) | AFIO_PHY_V33 | AFIO_IOEN;
    // PC16/PC17 inputs with pull-downs (a host's 15k).
    GPIOC->CFGXR = (GPIOC->CFGXR & ~0xFFu) | 0x88u;
    GPIOC->BSXR = (1u << 16) | (1u << 17);
    USBFSH->BASE_CTRL = UC_HOST_MODE;
    for (uint16_t i = 0; i < 1000 && !(USBFSH->BASE_CTRL & UC_HOST_MODE); i++) { }
    USBFSH->HOST_CTRL = 0;
    USBFSH->DEV_ADDR = 0;
    USBFSH->HOST_EP_MOD = UH_EP_TX_EN | UH_EP_RX_EN;
    USBFSH->HOST_RX_DMA = (uint16_t)(uint32_t)rxb;
    USBFSH->HOST_TX_DMA = (uint16_t)(uint32_t)txb;
    USBFSH->HOST_RX_CTRL = 0;
    USBFSH->HOST_TX_CTRL = 0;
    USBFSH->BASE_CTRL = UC_HOST_MODE | UC_INT_BUSY | UC_DMA_EN;
    USBFSH->HOST_SETUP = UH_SOF_EN;
    USBFSH->INT_FG = 0xFF;
    USBFSH->INT_EN = 0;                         // polled
}
static void dropAll() {
    root = Dev();
    for (uint8_t i = 1; i < nDev; i++) behind[i - 1] = Dev();
    nPorts = 0;
    clearInput(false);
    devText[0] = 0;
}
static void hostOff() {
    USBFSH->BASE_CTRL = UC_RESET_SIE | UC_CLR_ALL;
    waitUs(10);
    USBFSH->BASE_CTRL = 0;
    dropAll();
}

// The device to talk to: its address and its speed. Low speed behind a
// (full-speed) hub: the SIE at low speed, the port at full speed (SOFs go
// on), and a PRE packet before each one (app_km.c's SetSelfSpeed).
void in::select(const Dev &d) {
    USBFSH->DEV_ADDR = (uint8_t)((USBFSH->DEV_ADDR & 0x80) | (d.addr & 0x7F));
    ep0 = d.ep0 ? d.ep0 : 8;
    if (!d.low) {
        USBFSH->BASE_CTRL &= ~UC_LOW_SPEED;
        USBFSH->HOST_CTRL &= ~UH_LOW_SPEED;
        USBFSH->HOST_SETUP &= ~UH_PRE_PID_EN;
    } else if (&d == &root) {
        USBFSH->BASE_CTRL |= UC_LOW_SPEED;
        USBFSH->HOST_CTRL |= UH_LOW_SPEED;
        USBFSH->HOST_SETUP |= UH_PRE_PID_EN;
    } else {
        USBFSH->BASE_CTRL |= UC_LOW_SPEED;
        USBFSH->HOST_CTRL &= ~UH_LOW_SPEED;
        USBFSH->HOST_SETUP |= UH_PRE_PID_EN;
    }
}

// USBFSH_Transact.
uint8_t in::transact(uint8_t pidEp, uint8_t tog, uint16_t timeout) {
    USBFSH->HOST_TX_CTRL = USBFSH->HOST_RX_CTRL = tog;
    for (uint8_t retry = 0; retry < 10; retry++) {
        if (ccGone() || (enumerating && (int32_t)(millis() - deadline) > 0)) return E_DISCON;
        USBFSH->HOST_EP_PID = pidEp;
        USBFSH->INT_FG = UIF_TRANSFER;
        uint16_t i = 1000;
        while (i && !(USBFSH->INT_FG & UIF_TRANSFER)) {
            waitUs(1);
            i--;
        }
        USBFSH->HOST_EP_PID = 0;
        if (!(USBFSH->INT_FG & UIF_TRANSFER)) return E_UNKNOWN;
        if (USBFSH->INT_ST & UIS_TOG_OK) return E_OK;
        uint8_t r = USBFSH->INT_ST & UIS_H_RES_MASK;
        if (r == PID_STALL) return (uint8_t)(r | E_XFER);
        if (r == PID_NAK) {
            if (!timeout) return (uint8_t)(r | E_XFER);
            if (timeout < 0xFFFF) timeout--;
            retry--;
        } else if ((pidEp >> 4) == PID_IN) {
            if (r != PID_DATA0 && r != PID_DATA1 && r) return (uint8_t)(r | E_XFER);
        } else if (r) {
            return (uint8_t)(r | E_XFER);
        }
        waitUs(20);
        if (USBFSH->INT_FG & UIF_DETECT) {
            waitUs(200);
            if (!(USBFSH->HOST_CTRL & UH_PORT_EN)) return E_DISCON;
            USBFSH->INT_FG = UIF_DETECT;
        }
    }
    return E_XFER;
}

// USBFSH_CtrlTransfer: the 8-byte setup packet in setup, data in or out.
static uint8_t control(const uint8_t *setup, uint8_t *data, uint16_t *got) {
    waitUs(200);
    if (got) *got = 0;
    memcpy(txb, setup, 8);
    USBFSH->HOST_TX_LEN = 8;
    uint8_t s = transact(PID_SETUP << 4, 0, 10000);
    if (s) return s;
    USBFSH->HOST_TX_CTRL = T_TOG | T_AUTO_TOG;
    USBFSH->HOST_RX_CTRL = R_TOG | R_AUTO_TOG;
    USBFSH->HOST_TX_LEN = 1;
    uint16_t rem = (uint16_t)(setup[6] | setup[7] << 8);
    if (rem && data) {
        if (setup[0] & 0x80) {
            while (rem) {
                waitUs(200);
                s = transact(PID_IN << 4, USBFSH->HOST_RX_CTRL, 10000);
                if (s) return s;
                uint16_t n = USBFSH->RX_LEN < rem ? USBFSH->RX_LEN : rem;
                memcpy(data, rxb, n);
                data += n;
                rem = (uint16_t)(rem - n);
                if (got) *got = (uint16_t)(*got + n);
                if (!USBFSH->RX_LEN || (USBFSH->RX_LEN & (ep0 - 1))) break;   // a short packet
            }
            USBFSH->HOST_TX_LEN = 0;            // the status stage is an OUT
        } else {
            while (rem) {
                waitUs(200);
                uint8_t n = (uint8_t)(rem >= ep0 ? ep0 : rem);
                USBFSH->HOST_TX_LEN = n;
                memcpy(txb, data, n);
                data += n;
                s = transact(PID_OUT << 4, USBFSH->HOST_TX_CTRL, 10000);
                if (s) return s;
                rem = (uint16_t)(rem - n);
                if (got) *got = (uint16_t)(*got + n);
            }
        }
    }
    waitUs(200);
    s = transact(USBFSH->HOST_TX_LEN ? PID_IN << 4 : PID_OUT << 4, R_TOG | T_TOG, 10000);
    return s;
}

uint8_t in::ctl(uint8_t type, uint8_t req, uint16_t val, uint16_t idx, uint16_t len, uint8_t *data,
                uint16_t *got) {
    uint8_t s[8] = {type, req, (uint8_t)val, (uint8_t)(val >> 8), (uint8_t)idx, (uint8_t)(idx >> 8),
                    (uint8_t)len, (uint8_t)(len >> 8)};
    uint16_t n = 0;
    if (!got) got = &n;
    uint8_t r = control(s, data, got);
    if (probe) probe->ctl(s, r, *got, data);
    return r;
}

static void resetPort() {
    USBFSH->DEV_ADDR = USBFSH->DEV_ADDR & 0x80;
    USBFSH->BASE_CTRL &= ~UC_LOW_SPEED;
    USBFSH->HOST_SETUP &= ~UH_PRE_PID_EN;
    USBFSH->HOST_CTRL = (uint8_t)((USBFSH->HOST_CTRL & ~UH_LOW_SPEED) | UH_BUS_RESET);
    delay(11);
    USBFSH->HOST_CTRL &= ~UH_BUS_RESET;
    delay(2);
    USBFSH->INT_FG = UIF_DETECT;
}
static bool enablePort(bool &lowSpeed) {
    if (!(USBFSH->MIS_ST & UMS_DEV_ATTACH)) return false;
    if (!(USBFSH->HOST_CTRL & UH_PORT_EN)) lowSpeed = (USBFSH->MIS_ST & UMS_DM_LEVEL) != 0;
    USBFSH->HOST_CTRL |= UH_PORT_EN;
    USBFSH->HOST_SETUP |= UH_SOF_EN;
    return true;
}

// ---------------------------------------------------------------------------
// Enumeration: one device, at address 0 (its port just reset), to addr
// ---------------------------------------------------------------------------
// Each endpoint's driver sets its device up (a K_PAD interface gets SET_IDLE
// here); then a K_PAD interface's report descriptor decides: a gamepad, or a
// keyboard that declares no boot interface (some Bluetooth ones, over their
// cable), or neither (not read).
void in::setupDev(Dev &d, uint16_t padLen, uint8_t padIntf) {
    for (uint8_t i = 0; i < d.nEp; i++) {
        Ep &e = d.ep[i];
        if (e.kind == K_PAD) ctl(0x21, 0x0A, 0, e.intf, 0, nullptr);    // SET_IDLE: only changes
        else if (drv[e.kind]->setup) drv[e.kind]->setup(d, e);
    }
    if (!padLen) return;
    if (padLen > sizeof big) padLen = sizeof big;
    uint16_t n = 0;
    if (padLen <= 1 || ctl(0x81, 6, 0x2200, padIntf, padLen, big, &n) != E_OK) n = 0;
    for (uint8_t i = 0; i < d.nEp; i++) {
        Ep &e = d.ep[i];
        if (e.kind != K_PAD) continue;
        if (drv[K_PAD] && drv[K_PAD]->desc(e, big, n)) continue;
        e.kind = drv[K_KBD] && drv[K_KBD]->desc(e, big, n) ? K_KBD : K_NONE;
    }
}

bool in::enumDevice(Dev &d, uint8_t addr, uint16_t &padLen, uint8_t &padIntf) {
    uint8_t dd[18];
    uint16_t nd, n;
    d.addr = 0;
    d.ep0 = 8;
    d.nEp = 0;
    select(d);
    if (ctl(0x80, 6, 0x0100, 0, 18, dd, &nd) != E_OK || nd < 8) return false;   // GET_DESCRIPTOR device
    if (probe) probe->dev(dd);
    d.ep0 = dd[7] ? dd[7] : 8;
    if (ctl(0x00, 5, addr, 0, 0, nullptr) != E_OK) return false;               // SET_ADDRESS
    d.addr = addr;
    select(d);
    if (!pause(10)) return false;
    if (ctl(0x80, 6, 0x0200, 0, 9, big, &n) != E_OK || n < 9) return false;   // GET_DESCRIPTOR configuration
    uint16_t total = (uint16_t)(big[2] | big[3] << 8);
    if (total > sizeof big) total = sizeof big;
    if (ctl(0x80, 6, 0x0200, 0, total, big, &n) != E_OK) return false;
    // A Switch Pro pad: its own reports, not its descriptor's.
    bool sw = dd[8] == 0x7E && dd[9] == 0x05 && dd[10] == 0x09 && dd[11] == 0x20 && drv[K_SPAD];
    // The interfaces: a hub, and each interrupt IN endpoint a driver takes;
    // one of each kind.
    d.hub = nd >= 5 && dd[4] == 9;
    uint8_t cls = 0, sub = 0, proto = 0, intf = 0;
    uint16_t hidLen = 0;
    for (uint16_t p = 0; p + 2 <= n && big[p] >= 2; p = (uint16_t)(p + big[p])) {
        const uint8_t *x = big + p;
        if (x[1] == 4 && p + 9 <= n) {
            intf = x[2];
            cls = x[5];
            sub = x[6];
            proto = x[7];
            hidLen = 0;
            if (cls == 9) d.hub = true;
            if (probe) probe->desc(x);
        } else if (x[1] == 0x21 && p + 9 <= n) {
            hidLen = (uint16_t)(x[7] | x[8] << 8);
        } else if (x[1] == 5) {
            if (probe && p + 7 <= n) probe->desc(x);
            if (!(x[2] & 0x80) && (x[3] & 3) == 3) {
                xOut = x[2] & 0x0F;
                outTog = 0;
            } else if (p + 7 <= n && (x[3] & 3) == 3 && d.nEp < 3) {
                uint8_t k = K_NONE;
                if (cls == 3 && proto == 1) k = K_KBD;
                else if (cls == 3 && proto == 2) k = K_MOUSE;
                else if (cls == 3) k = sw ? K_SPAD : K_PAD;
                else if (cls == 0xFF && sub == 0x5D && proto == 0x01) k = K_XPAD;
                // Taken only by a driver the sketch uses (K_PAD: a gamepad,
                // or a keyboard without a boot interface).
                if (k == K_PAD ? !drv[K_PAD] && !drv[K_KBD] : !drv[k]) k = K_NONE;
                bool dup = k == K_NONE || (k >= K_PAD && (hasPad || padLen));
                for (uint8_t i = 0; i < d.nEp; i++) dup |= d.ep[i].kind == k;
                if (!dup) {
                    Ep &e = d.ep[d.nEp++];
                    e = Ep{(uint8_t)(x[2] & 0x0F), 0, (uint8_t)(x[6] ? x[6] : 1), (uint8_t)(x[4] > 64 ? 64 : x[4]),
                           k, intf, 0, 0};
                    if (k == K_PAD) {
                        padLen = hidLen ? hidLen : 1;
                        padIntf = intf;
                    }
                }
            }
        }
    }
    return ctl(0x00, 9, big[5], 0, 0, nullptr) == E_OK;                       // SET_CONFIGURATION
}

// The device on the port itself.
static const char *enumRoot() {
    detached = false;
    enumerating = true;
    deadline = millis() + 3000;
    const char *why = "ENUMERATION FAILED";
    for (uint8_t tries = 1; tries <= 3; tries++) {
        if (!pause((uint16_t)(100 + (8u << tries)))) break;
        dropAll();
        resetPort();
        bool lowSpeed = false;
        uint8_t ok = 0;
        for (uint8_t i = 0; i < 100 && ok < 7; i++) {
            ok = enablePort(lowSpeed) ? (uint8_t)(ok + 1) : 0;
            if (!pause(1)) break;
        }
        if (detached) break;
        if (ok < 7) continue;
        Dev &d = root;
        d.low = lowSpeed;
        uint16_t padLen = 0;
        uint8_t padIntf = 0;
        if (!enumDevice(d, 2, padLen, padIntf)) {
            if ((int32_t)(millis() - deadline) > 0) {
                why = "NO ANSWER";
                break;
            }
            continue;
        }
        if (d.hub) {
            uint8_t r = hub ? hub->start() : 0;     // (no hub driver: a hub is not supported)
            if (r == 1) continue;
            if (r == 2) break;
        } else {
            setupDev(d, padLen, padIntf);
        }
        enumerating = false;
        recount();
        return nullptr;
    }
    enumerating = false;
    return detached ? "UNPLUGGED" : why;
}

// ---------------------------------------------------------------------------
// Polling the endpoints a driver took
// ---------------------------------------------------------------------------
static void pollEp(Dev &d, Ep &e) {
    uint32_t now = millis();
    uint8_t iv = e.interval < 4 ? 4 : e.interval;     // (no faster than every 4 ms)
    if (now - e.at < iv) return;
    e.at = now;
    select(d);
    uint8_t s = transact((uint8_t)(PID_IN << 4 | e.num), e.tog, 0);
    uint8_t len = USBFSH->RX_LEN > e.maxPkt ? e.maxPkt : (uint8_t)USBFSH->RX_LEN;
    if (probe) probe->poll((uint8_t)(d.addr - 2), e, s, rxb, (uint8_t)USBFSH->RX_LEN);
    if (s == E_OK) {
        e.tog ^= R_TOG;
        nReports++;
        lastAt = now;
        drv[e.kind]->report(e, rxb, len);
    } else if (s == (PID_STALL | E_XFER)) {
        ctl(0x02, 1, 0, (uint16_t)(e.num | 0x80), 0, nullptr);   // CLEAR_FEATURE ENDPOINT_HALT
        e.tog = 0;
    }
}

static void pollAll() {
    for (uint8_t i = 0; i < nDev; i++) {
        Dev &d = dev(i);
        if (!d.addr || d.hub) continue;
        for (uint8_t j = 0; j < d.nEp; j++)
            if (d.ep[j].kind) pollEp(d, d.ep[j]);
    }
    for (uint8_t k = 1; k < K_N; k++)
        if (drv[k] && drv[k]->after) drv[k]->after();
}

// Host power off: PA0 low first, then the USB host, the Rp, CC a sink.
static void sinkOff(const char *why) {
    if (swapOwned) swap->stop();
    swapOwned = false;
    hostOff();
    sinkHost = false;
    lostFn = nullptr;
    st = OFF;
    stat = why;
    CDC_init();                                 // the serial port again
}
static void powerOff(const char *why) {
    if (sinkHost) {
        sinkOff(why);
        return;
    }
    forced = false;
    pwr::pa0Low();
    powered = false;
    if (swap) swap->stop();
    hostOff();
    rpOff();
    pwr::pa0Analog();
    stat = why;
    st = SETTLING;
    t0 = millis();
}

bool forcedOn() { return forced; }
bool force() {
    if (st != WAIT_SINK || forced) return false;
    forceReq = true;
    return true;
}

static void arm() {
    forced = forceReq = false;
    if (st != TRY_SINK) snkTried = false;
    rpOn();
    ccN = 0;
    openSince = 0;
    st = WAIT_SINK;
    stat = "PLUG IN A DEVICE";
}

const char *enter() {
    if (st != OFF) return nullptr;
    if (!pwr::diag().ch211) return "NO CH211: HOST REFUSED";
    // 1. CN1 off (known off), the PD sink paused, PA0 low.
    if (!pwr::disableCn1()) return "CN1 STUCK ON: REFUSED";
    pdWas = pwr::pausePd();
    pwr::pa0Low();
    delay(5);
    // 2. PA0 analog, settled; VBUS must be absent.
    pwr::pa0Analog();
    uint32_t t = millis();
    for (;;) {
        delay(25);
        pwr::readRegs();
        uint16_t v = pwr::vbusMv();
        bool present = (pwr::reg(pwr::PIN_STAT) & pwr::ST_VBUS_RDY) || v > 4000;
        bool absent = !present && v < 800 && !(pwr::reg(pwr::SYS_STAT) & pwr::SYS_VBUS_EXIST);
        if (absent) break;
        if (present || millis() - t > 1000) {
            if (pdWas) pwr::resumePd();
            return "UNPLUG THE CHARGER";
        }
    }
    // 3. The port: the serial port goes, then CC as a source.
    takeUsb();
    dropAll();
    clearInput(true);
    nReports = 0;
    lastAt = 0;
    arm();
    return nullptr;
}

const char *enterPowered(bool (*lost)()) {
    if (st != OFF) return "HOST ALREADY ON";
    if (!pwr::diag().ch211) return "NO CH211: HOST REFUSED";
    if (pwr::pa0Driven()) return "HOST POWER ON: REFUSED";
    if (!pwr::disableCn1()) return "CN1 STUCK ON: REFUSED";
    takeUsb();
    dropAll();
    clearInput(true);
    nReports = 0;
    lastAt = 0;
    pdWas = false;
    forced = forceReq = false;
    powered = false;
    sinkHost = true;
    lostFn = lost;
    detached = false;
    nAttach++;
    hostOn();
    st = WAIT_DEVICE;
    t0 = millis();
    stat = "HUB POWERED: WAIT USB";
    return nullptr;
}
bool poweredByPort() { return sinkHost; }

void leave() {
    if (st == OFF) return;
    if (sinkHost) {
        sinkOff("");
        return;
    }
    if (pwr::pa0Driven()) pwr::pa0Low();
    powered = false;
    if (swap) swap->stop();
    hostOff();
    rpOff();
    pwr::pa0Analog();
    st = OFF;
    stat = "";
    CDC_init();                                 // the serial port again (the PC re-enumerates it)
    if (pdWas) pwr::resumePd();
}

static void serviceInner() {
    pwr::poll();                                // (every 10 ms at most)
    if (st == OFF) return;
    uint32_t now = millis();
    // PD_SWAP: our offer, and a hub's power role swap; once done, the host
    // runs on the hub's 5 V as enterPowered() would.
    if (swap && (powered || swapOwned)) {
        swap->service();
        uint8_t ph = swap->phase();
        if (powered && ph == 4) {
            powered = false;                    // (PA0 is low and analog: the PD sink watches VBUS)
            sinkHost = swapOwned = true;
            lostFn = swap->lost;
            stat = "HUB POWERS US NOW";
        } else if (powered && ph == 5) {
            nFault++;
            noteOff(4);
            powerOff("POWER SWAP FAILED");
            return;
        }
    }
    bool swapping = swap && powered && swap->phase() >= 1;
    static uint32_t ccAt;
    bool tick = now - ccAt >= 10;
    if (tick) ccAt = now;

    // Powered states: detach (the sink's Rd gone) or VBUS falling ends them.
    if (st == POWERING || st == WAIT_DEVICE || st == READY || st == UNSUPPORTED || st == FAILED) {
        if (tick && ccGone()) {
            powerOff(sinkHost ? "HUB POWER GONE" : "UNPLUGGED: POWER OFF");
            return;
        }
        if (st != POWERING && !swapping && !(pwr::reg(pwr::PIN_STAT) & pwr::ST_VBUS_RDY)) {
            if (sinkHost) {                     // the hub's 5 V gone: unplugged, not a fault of ours
                powerOff("HUB POWER GONE");
                return;
            }
            nFault++;
            noteOff(2);
            powerOff("5 V FELL: OVERLOAD?");
            return;
        }
        if (pwr::reg(pwr::SYS_STAT) & pwr::SYS_OT_RST) {
            nFault++;
            noteOff(3);
            powerOff("CH211 OVER-TEMPERATURE");
            return;
        }
    }
    // A device unplugged from an OTG adapter (whose Rd stays): D+/D- go
    // quiet. Host power stays on for the next one.
    if ((st == READY || st == UNSUPPORTED || st == FAILED) && !(USBFSH->MIS_ST & UMS_DEV_ATTACH)) {
        if (forced) {                           // forced 5 V: off with the device
            powerOff("UNPLUGGED: POWER OFF");
            return;
        }
        dropAll();
        USBFSH->HOST_CTRL &= ~UH_PORT_EN;
        st = WAIT_DEVICE;
        t0 = now;
        stat = swap && swap->riding() ? "SWITCHING POWER" : "UNPLUGGED";
        return;
    }

    switch (st) {
    case WAIT_SINK:
        if (!tick) break;
        if (pwr::reg(pwr::PIN_STAT) & pwr::ST_VBUS_RDY) {
            stat = "CHARGER IN: UNPLUG IT";
            ccN = 0;
            forceReq = false;                   // (never onto another source's 5 V)
            break;
        }
        if (forceReq) {
            forceReq = false;
            forced = true;
            // What CC showed: 0 under 0.22 V, 1 to 0.66, 2 to 1.23, 3 over.
            if (probe) {
                uint8_t lv[2];
                for (uint8_t c = 1; c <= 2; c++)
                    lv[c - 1] = (uint8_t)(above(c, CC_CMP_22) + above(c, 5 << 5) + above(c, CC_CMP_123));
                probe->forced(lv[0], lv[1]);
            }
        } else {
            uint8_t l = sinkOn(1) ? 1 : sinkOn(2) ? 2 : 0;
            if (!l || (ccN && l != line)) {
                ccN = 0;
                line = l;
                if (!l) snkTried = false;
                if (!l) stat = "PLUG IN A DEVICE";
                break;
            }
            line = l;
            if (++ccN < 15) break;              // tCCDebounce, 150 ms
            if (trySnk && !snkTried) {
                // Try.SNK: our Rp off, the CH211's Rd on, for up to 450 ms
                // (a powered dual-role hub sources us within about 250).
                snkTried = true;
                ccN = 0;
                rpOff();
                st = TRY_SINK;
                t0 = now;
                stat = "TRY SINK";
                break;
            }
        }
        // 4. A sink: host power on, after a fresh look at VBUS: the CH211
        //    heard now, PA0's reading under 0.8 V, VBUS neither ready nor present.
        ccN = 0;
        if (!pwr::readRegs() || !pwr::vbusValid() || pwr::vbusMv() >= 800 ||
            (pwr::reg(pwr::PIN_STAT) & pwr::ST_VBUS_RDY) || (pwr::reg(pwr::SYS_STAT) & pwr::SYS_VBUS_EXIST)) {
            stat = "VBUS SEEN: NO POWER";
            break;
        }
        // (pa0High() checks once more: CN1 off, no VBUS from elsewhere.)
        if (!pwr::pa0High()) {
            stat = "VBUS SEEN: NO POWER";
            forced = false;
            break;
        }
        nAttach++;
        openSince = 0;
        detached = false;
        powered = true;
        t0 = now;
        st = POWERING;
        stat = "POWERING";
        break;
    case POWERING:
        pwr::readRegs();
        if (pwr::reg(pwr::PIN_STAT) & pwr::ST_VBUS_RDY) {
            onMs = (uint16_t)(millis() - t0);
            hostOn();
            if (swap && !forced && line) swap->start(line);   // our 5 V offered over PD
            st = WAIT_DEVICE;
            t0 = millis();
            stat = "5 V ON";
        } else if (now - t0 > 275) {
            nFault++;
            powerOff("NO 5 V IN 275 MS");
        }
        break;
    case WAIT_DEVICE:
        if (USBFSH->MIS_ST & UMS_DEV_ATTACH) {
            stat = "ENUMERATING...";
            const char *e = enumRoot();
            if (detached) {
                powerOff("UNPLUGGED: POWER OFF");
                break;
            }
            if (e) {
                st = FAILED;
                stat = e;
            } else if (nPorts) {
                st = READY;
                stat = "HUB READY";
                hub->ready();
            } else if (hasKb || hasMs || hasPad) {
                st = READY;
                stat = "READY";
            } else {
                st = UNSUPPORTED;
                stat = root.hub ? "A HUB: NOT SUPPORTED" : "NOT A DEVICE THIS READS";
            }
        } else if (forced && now - t0 > 5000) {
            powerOff("NO DEVICE: 5 V OFF");
        } else if (now - t0 > 3000) {
            stat = "POWERED, NO USB SEEN";
        }
        break;
    case READY:
        if (nPorts) {
            hub->service(now);
            if (detached) {
                powerOff("UNPLUGGED: POWER OFF");
                break;
            }
        }
        pollAll();
        break;
    case TRY_SINK:
        if (!tick) break;
        pwr::readRegs();
        if (pwr::reg(pwr::PIN_STAT) & pwr::ST_VBUS_RDY) {
            // The partner sources us: host mode ends; the app takes the PD sink.
            st = OFF;
            stat = "PORT POWERS US";
            CDC_init();
            break;
        }
        if (now - t0 > 450) {
            arm();                              // no: our Rp again, and power it
            snkTried = true;
            stat = "NO SOURCE: POWERING";
        }
        break;
    case SETTLING:
        // Host power off: look for the next sink only once VBUS has gone
        // (the handoff's step 5); still there after 1 s, say so and wait.
        if (!tick) break;
        if (pwr::vbusValid() && pwr::vbusMv() < 800 && !(pwr::reg(pwr::SYS_STAT) & pwr::SYS_VBUS_EXIST)) arm();
        else if (now - t0 > 1000) stat = "5 V STILL ON: WAIT";
        break;
    default:
        break;
    }
}

void service() {
    serviceInner();
    if (probe) probe->status(stat);
}

#else  // CHSIM: whatever the sketch's drivers read, behind a hub, made up

static uint32_t simT;
static uint8_t typedN;
const char *enter() {
    pdWas = pwr::pausePd();
    st = READY;
    clearInput(true);
    hasKb = drv[K_KBD] != nullptr;
    hasMs = drv[K_MOUSE] != nullptr;
    hasPad = drv[K_XPAD] != nullptr;    // an Xbox 360 pad, as on the bench
    padKind = hasPad ? K_XPAD : K_NONE;
    if (hasPad) {
        Pad &p = padState();
        p.nButtons = 11;
        p.axes = 0x7F;
        p.xinput = true;
    }
    nPorts = hub ? 4 : 0;
    onMs = 3;
    describe();
    stat = "READY";
    simT = millis();
    typedN = 0;
    return nullptr;
}
void leave() {
    st = OFF;
    if (pdWas) pwr::resumePd();
}
bool force() { return false; }
bool forcedOn() { return false; }
void setTrySink(bool) {}
const char *enterPowered(bool (*)()) { return enter(); }
bool poweredByPort() { return false; }

// A triangle wave, -127..127, period p ms.
static int8_t tri(uint32_t t, uint32_t p) {
    int32_t v = (int32_t)((t % p) * 508 / p);
    return (int8_t)(v < 254 ? v - 127 : 381 - v);
}
// "Hello, agent." typed once, the mouse going round, the pad's controls in turn.
void service() {
    if (st != READY) return;
    uint32_t t = millis() - simT;
    static Ep simEp;
    static const char MSG[] = "Hello, agent.\n";
    if (hasKb && typedN < sizeof MSG - 1 && t > 300u + typedN * 80u) {
        char c = MSG[typedN++];
        uint8_t r[8] = {0};
        if (c >= 'a' && c <= 'z') {
            r[2] = (uint8_t)(4 + c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            r[0] = 2;
            r[2] = (uint8_t)(4 + c - 'A');
        } else {
            r[2] = c == ' ' ? 0x2C : c == ',' ? 0x36 : c == '.' ? 0x37 : 0x28;
        }
        drv[K_KBD]->report(simEp, r, 8);
        uint8_t up[8] = {0};
        drv[K_KBD]->report(simEp, up, 8);
    }
    if (hasMs) {
        uint8_t r[3] = {(uint8_t)((t / 700) % 4 == 1 ? 1 : (t / 700) % 4 == 3 ? 2 : 0),
                        (uint8_t)(int8_t)(tri(t, 2000) / 32), (uint8_t)(int8_t)(tri(t + 500, 2000) / 32)};
        drv[K_MOUSE]->report(simEp, r, 3);
    }
    if (hasPad) {
        Pad &p = padState();
        p.buttons = 1u << ((t / 300) % 11);
        p.x = tri(t, 1600);
        p.y = tri(t + 400, 1600);
        p.rx = tri(t, 2400);
        p.ry = tri(t + 600, 2400);
        p.lt = (uint8_t)(tri(t, 1000) + 127);
        p.rt = (uint8_t)(127 - tri(t, 1000));
        p.hat = (uint8_t)((t / 500) % 9);
    }
    nReports++;
    lastAt = millis();
}

#endif

}  // namespace host
