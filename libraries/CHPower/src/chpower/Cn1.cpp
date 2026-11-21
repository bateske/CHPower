// CN1, the protected output (Power.h): its enable transaction, its limit,
// its telemetry and the checks that need the INA226's readings. Linked only
// into a sketch that calls one of these; its first call hooks the checks
// into poll() (CN1 cannot be on before: begin() inhibits it).
#include "Power.h"
#include "Internal.h"
#include "Hw.h"

namespace pwr {
using namespace in;

static Tele tl;
static uint16_t limit;
static int32_t peak, tripCur;
static uint32_t tripAtMs;
static int32_t highestMv;               // CN1's highest voltage this run (what C25 may hold)

static void cn1Check(uint32_t now);
static void onTrip() {
    tripAtMs = millis();
    tripCur = tl.ma;
}
static void use() {
    cn1Poll = cn1Check;
    cn1Trip = onTrip;
}

const Tele &tele() {
    use();
    return tl;
}
uint16_t limitMa() { return limit; }
int32_t peakMa() { return peak; }
int32_t tripMa() { return tripCur; }
uint32_t tripMs() { return tripAtMs; }

static uint16_t clampLimit(uint16_t lim) {
    uint16_t top = budgetMa();
    if (top > 2400 || !conMa) top = 2400;           // (no contract: 2.4 A; enabling refuses anyway)
    if (lim > top) lim = top;
    if (lim < 100) lim = 100;
    return lim;
}

#ifndef CHSIM
static void readTele() {
    uint16_t s, i, p, b;
    // 0x01, 0x04, 0x03, then 0x02 last: the pointer stays on the bus voltage.
    if (!inaRead(0x01, s) || !inaRead(0x04, i) || !inaRead(0x03, p) || !inaRead(0x02, b)) return;
    tl.shuntUv = (int32_t)(int16_t)s * 5 / 2;
    tl.ma = (int32_t)(int16_t)i / 4;
    tl.mw = (int32_t)p * 25 / 4;
    tl.busMv = (int32_t)b * 5 / 4;
    if (tl.busMv > highestMv) highestMv = tl.busMv;
}

static void checkContract() {
    conNew = false;
    if (outState != OUT_ON) return;
    uint16_t b = budgetMa();
    if (!conMa || b < 100) {
        disableCn1();
        whyText = conMa ? "BUDGET UNDER 0.1 A: CN1 OFF" : "CONTRACT ENDED: CN1 OFF";
    } else if (limit > b) {
        setLimit(b);
    }
}

// From poll(), every 10 ms, after the CH211's checks.
static void cn1Check(uint32_t now) {
    readTele();
    if (conNew) checkContract();
    if (outState != OUT_ON) return;
    if (tl.ma > peak) peak = tl.ma;
    // The enable node fell: the INA226's latch (or U14, or the eFuse) took
    // CN1 off.
    if (!(regs[PIN_STAT] & ST_HVLI)) trip("CN1 TRIPPED");
    else if (tl.ma < -50) trip("REVERSE CURRENT INTO CN1");
}

// The initialise / rearm transaction (docs/design.md section 5).
const char *enableCn1(uint16_t lim) {
    use();
    uint16_t contractMv = conMv, contractMa = conMa;
    if (!dg.ch211 || !dg.ina) return whyText = "NO CH211/INA226: BLOCKED";
    if (outState == OUT_BLOCKED) return whyText = "BLOCKED UNTIL RESTART";
    if (outState == OUT_ON) return nullptr;
    if (driven) return whyText = "HOST MODE: CN1 STAYS OFF";
    if (!contractMa) return whyText = "NO CONTRACT: PD OR 1.5A/3A";
    uint16_t allow = allowMa(contractMv, contractMa);
    if (lim > allow) return whyText = "LIMIT OVER THE BUDGET";
    if (lim < 100) return whyText = "LIMIT UNDER 100 MA";

    // 1. Inhibited, and the enable node low.
    if (!inhibit() || !readRegs()) return whyText = "CH211 DID NOT ANSWER";
    if (regs[PIN_STAT] & ST_HVLI) {
        outState = OUT_BLOCKED;
        return whyText = "HVLI HIGH WHILE INHIBITED";
    }
    // 2. The INA226: the alert read once (clears a latched trip), then the
    //    configuration, the calibration, the limit and the alert, read back.
    uint16_t m = 0, v0 = 0, v5 = 0, v7 = 0, v6 = 0, id = 0;
    uint16_t lc = (uint16_t)(lim * 4u);             // floor(I x 0.010 / 2.5 uV)
    if (!inaRead(0x06, m) || !inaWrite(0x00, 0x4007) || !inaWrite(0x05, 0x0800) ||
        !inaWrite(0x07, lc) || !inaWrite(0x06, 0x8003) || !inaRead(0x00, v0) || !inaRead(0x05, v5) ||
        !inaRead(0x07, v7) || !inaRead(0x06, v6) || !inaRead(0xFE, id))
        return whyText = "INA226 DID NOT ANSWER";
    if (v0 != 0x4007 || v5 != 0x0800 || v7 != lc || (v6 & 0xFC03) != 0x8003 || id != 0x5449)
        return whyText = "INA226 DID NOT READ BACK";
    // 3. Nothing drives CN1 already.
    delay(2);
    readTele();                                     // (ends on 0x02)
    int32_t vb = vbusMv();
    int32_t most = (vb > highestMv ? vb : highestMv) + 500;
    if (tl.ma < -50 || tl.ma > 50 || tl.busMv > most) return whyText = "CN1 ALREADY DRIVEN: REFUSED";
    // 4. (the contract and the budget: above) 5. Permit, and the node rises.
    enabling = true;
    if (!permit()) {
        inhibitOk();
        enabling = false;
        return whyText = "CH211 PERMIT DID NOT READ BACK";
    }
    uint32_t t0 = millis();
    bool up = false;
    while (millis() - t0 < 60) {
        if (readRegs() && (regs[PIN_STAT] & ST_HVLI)) {
            up = true;
            break;
        }
    }
    if (!up) {
        inhibitOk();
        enabling = false;
        return whyText = "HVLI STAYED LOW: A CLAMP HOLDS";
    }
    limit = lim;
    peak = 0;
    tripCur = 0;
    outState = OUT_ON;
    enabling = false;
    whyText = "";
    return nullptr;
}

bool setLimit(uint16_t want) {
    use();
    uint16_t lim = clampLimit(want);
    limit = lim;
    if (outState != OUT_ON) return lim == want;
    uint16_t lc = (uint16_t)(lim * 4u), v7 = 0;
    return inaWrite(0x07, lc) && inaRead(0x07, v7) && v7 == lc && lim == want;
}

#else  // CHSIM: a load that wanders between 0.2 and 0.6 A

static uint32_t simT;
static int32_t simMv = 5000;
static void cn1Check(uint32_t) {
    simT++;
    int32_t w = (int32_t)(simT % 240);
    tl.busMv = outState == OUT_ON ? simMv + 20 : 0;
    tl.ma = outState == OUT_ON ? 200 + (w < 120 ? w : 240 - w) * 10 / 3 + (int32_t)(simT % 7) : 0;
    tl.mw = tl.busMv * tl.ma / 1000;
    tl.shuntUv = tl.ma * 10;
    if (outState == OUT_ON && tl.ma > peak) peak = tl.ma;
    regs[PIN_STAT] = (uint8_t)(0x13 | (outState == OUT_ON ? ST_HVLI : 0));
}
const char *enableCn1(uint16_t lim) {
    use();
    if (!conMa) return whyText = "NO CONTRACT: PD OR 1.5A/3A";
    if (lim > budgetMa()) return whyText = "LIMIT OVER THE BUDGET";
    limit = lim;
    simMv = conMv;
    outState = OUT_ON;
    peak = 0;
    return nullptr;
}
bool setLimit(uint16_t lim) {
    use();
    limit = clampLimit(lim);
    return limit == lim;
}
#endif

}  // namespace pwr
