// The Rev 1.5 power section's core (Power.h): the I2C, the CH211, PA0, the
// contract and the checks that need no INA226 reading. CN1's enable, limit
// and telemetry are Cn1.cpp's, linked only into a sketch that uses CN1.
#include "Power.h"
#include "PdLog.h"
#include "Internal.h"
#include "Hw.h"

namespace plog {
void (*hook)(uint8_t kind, uint8_t a, uint16_t b, uint16_t c);
void add(uint8_t kind, uint8_t a, uint16_t b, uint16_t c) {
    if (hook) hook(kind, a, b, c);
}
}

namespace pwr {
namespace in {
Diag dg;
const char *whyText = "";
uint8_t regs[8];
Out outState = OUT_OFF;
bool driven;
bool offFailed;
volatile bool enabling;
volatile uint16_t conMv, conMa;
volatile bool conNew;
void (*cn1Poll)(uint32_t now);
void (*cn1Trip)();
bool (*pdHalt)();
void (*pdResume)();
}  // namespace in
using namespace in;

static uint8_t ccWant = CC_SINK;
#ifndef CHSIM
static uint32_t analogAt;               // PA0 made analog then
static uint32_t polledAt;
#endif

const Diag &diag() { return dg; }
const char *why() { return whyText; }
void setWhy(const char *s) { whyText = s; }
uint8_t reg(uint8_t r) { return regs[r & 7]; }
Out out() { return outState; }
uint8_t ccWanted() { return ccWant; }
bool pa0Driven() { return driven; }

// The budget (docs/design.md section 5): the console's share of the
// contract taken off (2.0 W through an 85% buck, until measured), 10%
// tolerance and 50 mA kept back; never above 2.4 A.
uint16_t allowMa(uint16_t mv, uint16_t contractMa) {
    if (!mv || !contractMa) return 0;
    if (contractMa > 3000) contractMa = 3000;
    int32_t console = 2000000L / ((int32_t)mv * 85 / 100);
    int32_t a = ((int32_t)contractMa - console) * 10 / 11 - 50;
    if (a < 0) a = 0;
    if (a > 2400) a = 2400;
    return (uint16_t)a;
}
uint16_t budgetMa() { return allowMa(conMv, conMa); }

bool pausePd() { return pdHalt && pdHalt(); }
void resumePd() {
    if (pdResume) pdResume();
}

#ifndef CHSIM
// ---------------------------------------------------------------------------
// I2C on CN2: SDA PC18, SCL PC19 (GPIOC CFGXR nibbles 2 and 3)
// ---------------------------------------------------------------------------
static const uint32_t SDA = 1u << 18, SCL = 1u << 19;
static const uint32_t SDA_SH = 8, SCL_SH = 12;     // nibble shifts in CFGXR
static bool sclPP;                                  // SCL driven both ways (INT_PIN recovery)
static bool err;

static inline void cfg(uint32_t sh, uint32_t v) {
    GPIOC->CFGXR = (GPIOC->CFGXR & ~(0xFu << sh)) | (v << sh);
}
static inline void hp() { waitUs(5); }              // half a 100 kHz bit
static inline void sdaLow() { cfg(SDA_SH, 0x1); }   // output, latch 0
static inline void sdaRel() { cfg(SDA_SH, 0x4); }   // floating input: the pull-up
static inline bool sdaIn() { return (GPIOC->INDR & SDA) != 0; }
static inline bool sclIn() { return (GPIOC->INDR & SCL) != 0; }
static inline void sclLow() {
    if (sclPP) GPIOC->BSXR = 1u << 19;              // BR19
    else cfg(SCL_SH, 0x1);
}
static void sclRel() {
    if (sclPP) {
        GPIOC->BSXR = 1u << 3;                      // BS19: driven high
        return;
    }
    cfg(SCL_SH, 0x4);
    for (uint16_t i = 0; i < 2000 && !sclIn(); i++) waitUs(1);   // clock stretching, bounded
    if (!sclIn()) err = true;
}

static void busInit() {
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC;
    // PC18/PC19 are SWDIO/SWCLK after reset: SW_CFG = 100 makes them GPIO
    // until the next reset (RM 8.3.2.1). Masked: PCFR1 holds the remaps.
    AFIO->PCFR1 = (AFIO->PCFR1 & ~(7u << 24)) | (4u << 24);
    GPIOC->BSXR = (1u << 18) | (1u << 19);          // both latches 0
    sdaRel();
    cfg(SCL_SH, 0x4);
}

static void start() {
    sdaRel();
    hp();                                           // (a repeated START: the slave lets go of its ACK)
    sclRel();
    hp();
    sdaLow();
    hp();
    sclLow();
    hp();
}
static void stop() {
    sdaLow();
    hp();
    sclRel();
    hp();
    sdaRel();
    hp();
}
// Both stop at once after a clock-stretch timeout (err): each edge would
// otherwise wait its 2 ms again.
static bool wbyte(uint8_t b) {
    for (uint8_t i = 0; i < 8; i++, b <<= 1) {
        if (err) return false;
        if (b & 0x80) sdaRel();
        else sdaLow();
        hp();
        sclRel();
        hp();
        sclLow();
    }
    sdaRel();
    hp();
    sclRel();
    bool ack = !sdaIn();
    hp();
    sclLow();
    return ack;
}
static uint8_t rbyte(bool ack) {
    uint8_t v = 0;
    sdaRel();
    for (uint8_t i = 0; i < 8; i++) {
        if (err) return 0xFF;
        hp();
        sclRel();
        v = (uint8_t)(v << 1 | (sdaIn() ? 1 : 0));
        hp();
        sclLow();
    }
    if (ack) sdaLow();
    hp();
    sclRel();
    hp();
    sclLow();
    sdaRel();
    return v;
}
// SDA held low by a slave cut off mid-byte: clock it out, then a STOP.
static void busClear() {
    sdaRel();
    for (uint8_t i = 0; i < 9 && !sdaIn(); i++) {
        sclLow();
        hp();
        sclRel();
        hp();
    }
    stop();
}

// One whole transfer; on any failure the bus is cleared and false returned.
static bool done(bool ok) {
    if (err || !ok) {
        err = false;
        if (!sdaIn()) busClear();                   // (the caller has sent its STOP)
        dg.busErrors++;
        err = false;
        return false;
    }
    return true;
}

// CH211: write at 0x35 (0x6A), read at 0x35 with a pointer, fast read at 0x34.
static bool chWrite(uint8_t r, uint8_t v) {
    start();
    bool ok = wbyte(0x6A) && wbyte(r) && wbyte(v);
    stop();
    return done(ok);
}
static bool chRead(uint8_t r, uint8_t &v) {
    start();
    bool ok = wbyte(0x6A) && wbyte(r);
    if (ok) {
        start();                                    // repeated START
        ok = wbyte(0x6B);
        if (ok) v = rbyte(false);
    }
    stop();
    return done(ok);
}
static bool chReadAll(uint8_t *v) {
    start();
    bool ok = wbyte(0x69);
    if (ok)
        for (uint8_t i = 0; i < 8; i++) v[i] = rbyte(i < 7);
    stop();
    return done(ok);
}
// INA226 at 0x40 (0x80 / 0x81), big-endian 16-bit registers.
bool in::inaWrite(uint8_t r, uint16_t v) {
    start();
    bool ok = wbyte(0x80) && wbyte(r) && wbyte((uint8_t)(v >> 8)) && wbyte((uint8_t)v);
    stop();
    return done(ok);
}
bool in::inaRead(uint8_t r, uint16_t &v) {
    start();
    bool ok = wbyte(0x80) && wbyte(r);
    if (ok) {
        start();
        ok = wbyte(0x81);
        if (ok) {
            uint8_t hi = rbyte(true);
            v = (uint16_t)(hi << 8 | rbyte(false));
        }
    }
    stop();
    return done(ok);
}

// Read-modify-write of the owned bits, skipped if unchanged, read back.
static bool chSet(uint8_t r, uint8_t mask, uint8_t val) {
    uint8_t old, now;
    if (!chRead(r, old)) return false;
    uint8_t nv = (uint8_t)((old & ~mask) | (val & mask));
    if (nv != old && !chWrite(r, nv)) return false;
    if (!chRead(r, now) || (now & mask) != (val & mask)) {
        dg.busErrors++;
        return false;
    }
    regs[r] = now;
    return true;
}
bool in::inhibit() { return chSet(OD_CTRL, 0x0E, 0x0A); }
bool in::permit() { return chSet(OD_CTRL, 0x0E, 0x08); }

// The inhibit, read back, and the enable node (HVLI) seen low within 5 ms.
// A failure is never taken as off: offFailed makes poll() try again every
// frame, and nothing that needs CN1 off (a new voltage, host mode) goes on.
bool in::inhibitOk() {
    if (inhibit()) {
        for (uint8_t i = 0; i < 6; i++) {
            uint8_t ps;
            if (chRead(PIN_STAT, ps) && !(ps & ST_HVLI)) {
                regs[PIN_STAT] = ps;
                offFailed = false;
                return true;
            }
            delay(1);
        }
    }
    offFailed = true;
    whyText = "CN1 INHIBIT FAILED: RETRYING";
    return false;
}

bool readRegs() { return dg.ch211 && chReadAll(regs); }

// ---------------------------------------------------------------------------
// PA0
// ---------------------------------------------------------------------------
void pa0Low() {
    GPIOA->BCR = 1u;                                // latch 0 before it drives
    GPIOA->CFGLR = (GPIOA->CFGLR & ~0xFu) | 0x1u;   // push-pull output
    driven = true;
}
bool pa0High() {
    if (!driven) pa0Low();
    if (outState == OUT_ON || !dg.ch211 || !readRegs() || (regs[PIN_STAT] & ST_VBUS_RDY) ||
        (regs[SYS_STAT] & SYS_VBUS_EXIST))
        return false;
    GPIOA->BSHR = 1u;
    return true;
}
void pa0Analog() {
    GPIOA->BCR = 1u;
    GPIOA->CFGLR = GPIOA->CFGLR & ~0xFu;            // analog input, no pulls
    driven = false;
    analogAt = millis();
}
bool vbusValid() { return !driven && millis() - analogAt >= 20; }
uint16_t vbusMv() {
    if (!vbusValid()) return 0;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 8; i++) sum += analogRead(PIN_VBUS_SENSE);
    if (driven) return 0;                           // (never: analogRead keeps it analog)
    return (uint16_t)(sum * (3300u * 101u / 8u) / 4095u);
}

// ---------------------------------------------------------------------------
bool begin() {
    pa0Analog();
    analogReadResolution(12);
    busInit();
    dg = Diag();
    whyText = "";
    // An inherited INT_PIN = 1 holds SCL weakly low: drive SCL both ways for
    // the transfers that clear it (CH211 manual 5.8).
    if (!sclIn()) {
        dg.sclHeld = true;
        sclPP = true;
        cfg(SCL_SH, 0x1);
        GPIOC->BSXR = 1u << 3;
    }
    if (!sdaIn()) busClear();

    // 1. The CH211, reads only. Its reserved bits must read 0.
    if (!chReadAll(dg.boot)) {
        whyText = "NO CH211 AT 0x34/0x35";
        outState = OUT_BLOCKED;
        return false;
    }
    if ((dg.boot[OD_CTRL] & 0x11) || (dg.boot[HVCP_CTRL] & 0x60) || (dg.boot[HVIO_KEY] & 0xC8) ||
        (dg.boot[SYS_STAT] & 0x04)) {
        whyText = "CH211 RESERVED BITS SET";
        outState = OUT_BLOCKED;
        return false;
    }
    // 2. Its writes: PIN_CFG first (INT_PIN 0, SDA pull-up on), then CN1
    //    inhibited, HVIO an input, CC a plain sink.
    if (!chSet(PIN_CFG, 0x07, 0x04)) {
        whyText = "CH211 PIN_CFG WRITE FAILED";
        outState = OUT_BLOCKED;
        return false;
    }
    if (sclPP) {
        sclPP = false;
        cfg(SCL_SH, 0x4);
        GPIOC->BSXR = 1u << 19;
        waitUs(50);
        if (!sclIn()) {
            whyText = "SCL STILL LOW AFTER INT_PIN=0";
            outState = OUT_BLOCKED;
            return false;
        }
    }
    ccWant = CC_SINK;
    if (!inhibit() || !chSet(HVIO_KEY, 0x37, 0x00) || !chSet(CC_CTRL, 0xFF, CC_SINK)) {
        whyText = "CH211 SET-UP DID NOT READ BACK";
        outState = OUT_BLOCKED;
        return false;
    }
    dg.ch211 = true;

    // 3. The INA226: its ids, then its inherited alert, then a reset and
    //    the calibration (CN1 is inhibited, so the latch may be cleared).
    uint16_t m = 0, d = 0;
    if (!inaRead(0xFE, m) || !inaRead(0xFF, d) || m != 0x5449 || d != 0x2260) {
        whyText = "NO INA226 AT 0x40";
        outState = OUT_BLOCKED;
        return false;
    }
    inaRead(0x06, dg.inaBootMask);
    uint16_t c = 0, k = 0;
    if (!inaWrite(0x00, 0x8000) || !inaWrite(0x00, 0x4007) || !inaWrite(0x05, 0x0800) ||
        !inaRead(0x00, c) || !inaRead(0x05, k) || c != 0x4007 || k != 0x0800) {
        whyText = "INA226 SET-UP DID NOT READ BACK";
        outState = OUT_BLOCKED;
        return false;
    }
    uint16_t b;
    inaRead(0x02, b);                               // the pointer left on the bus voltage
    dg.ina = true;
    readRegs();
    return true;
}

bool ccSet(uint8_t v) {
    ccWant = v;
    return dg.ch211 && chSet(CC_CTRL, 0xFF, v);
}

void in::trip(const char *why) {
    outState = OUT_TRIPPED;
    if (cn1Trip) cn1Trip();
    inhibitOk();
    whyText = why;
}

void onContract(uint16_t mv, uint16_t ma) {
    conMv = mv;
    conMa = ma;
    conNew = true;
}

void poll() {
    if (!dg.ch211) return;
    uint32_t now = millis();
    if (now - polledAt < 10) return;
    polledAt = now;
    static uint8_t readFails;
    uint8_t r[8];
    if (chReadAll(r)) {
        readFails = 0;
        // The PD log: VBUS ready / present, VSYS present, CN1's enable node
        // and over-temperature, when one of them changes.
        static uint8_t lastSys = 0xFF, lastPin = 0xFF;
        uint8_t sys = r[SYS_STAT] & 0xF3, pin = r[PIN_STAT] & ST_HVLI;
        if (sys != lastSys || pin != lastPin) {
            plog::add(plog::K_STAT, sys, pin);
            lastSys = sys;
            lastPin = pin;
        }
        for (uint8_t i = 0; i < 8; i++) regs[i] = r[i];
    } else if (++readFails >= 3 && outState == OUT_ON) {
        // CN1 on and the CH211 not heard for three frames: nothing is
        // watching the enable node any more.
        trip("I2C LOST WHILE CN1 WAS ON");
        return;
    }
    if (offFailed) inhibitOk();
    if (regs[CC_CTRL] != ccWant || (regs[SYS_STAT] & SYS_OT_RST)) {
        // The CH211 reset its CC and OD registers (over-temperature), or
        // something changed them: CN1 is off; the owner of CC decides.
        dg.ccFaults++;
        if (outState == OUT_ON) trip("CH211 RESET ITS OUTPUTS");
    }
    // CN1's own checks, once the sketch uses CN1 (it is never on before).
    if (cn1Poll && dg.ina) cn1Poll(now);
}

bool disableCn1() {
    if (!dg.ch211) return true;                     // never enabled without it
    bool ok = inhibitOk();
    if (ok && outState == OUT_ON) outState = OUT_OFF;   // (TRIPPED stays: rearm is the user's)
    return ok;
}
bool cn1OffFailed() { return offFailed; }
bool cn1SurelyOff() { return !dg.ch211 || (outState != OUT_ON && !offFailed && !enabling); }

}  // namespace pwr
// The CHGame library's exit to the menu calls this first (a weak hook there).
extern "C" void chgame_beforeExit(void) { pwr::safeDown(); }
namespace pwr {

void safeDown() {
    noInterrupts();
    if (driven) pa0Low();                           // host power off first
    USBPD->PORT_CC1 = 0;
    USBPD->PORT_CC2 = 0;
    NVIC_DisableIRQ(USBPD_IRQn);
    if (dg.ch211) {
        inhibit();
        if (dg.ina) inaWrite(0x00, 0x8000);         // ALERT released: Q7 holds CN1 off too
        chSet(CC_CTRL, 0xFF, CC_SINK);
    }
    pa0Analog();
    interrupts();
}

#else  // CHSIM: made-up readings, to look at the screens

void pa0Low() { driven = true; }
bool pa0High() {
    driven = true;
    return true;
}
void pa0Analog() { driven = false; }
bool vbusValid() { return !driven; }
uint16_t vbusMv() { return driven ? 0 : 5080; }
bool readRegs() { return true; }
bool begin() {
    dg = Diag();
    dg.ch211 = dg.ina = true;
    regs[PIN_STAT] = 0x13;
    regs[PIN_CFG] = 0x04;
    regs[CC_CTRL] = CC_SINK;
    regs[OD_CTRL] = 0xAA;
    regs[HVCP_CTRL] = 0x02;
    regs[SYS_CFG] = 0x08;
    regs[SYS_STAT] = 0x13;
    for (uint8_t i = 0; i < 8; i++) dg.boot[i] = regs[i];
    return true;
}
bool ccSet(uint8_t v) {
    ccWant = regs[CC_CTRL] = v;
    return true;
}
void in::trip(const char *why) {
    outState = OUT_TRIPPED;
    if (cn1Trip) cn1Trip();
    whyText = why;
}
void onContract(uint16_t mv, uint16_t ma) {
    conMv = mv;
    conMa = ma;
}
void poll() {
    if (cn1Poll) cn1Poll(0);
}
bool disableCn1() {
    if (outState == OUT_ON) outState = OUT_OFF;
    return true;
}
bool cn1OffFailed() { return false; }
bool cn1SurelyOff() { return outState != OUT_ON; }
void safeDown() { outState = OUT_OFF; }
#endif

}  // namespace pwr
