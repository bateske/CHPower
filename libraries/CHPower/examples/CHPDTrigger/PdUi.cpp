// PD mode (PdUi.h).
#pragma GCC optimize("Os")
#include "PdUi.h"
#include <CHPower.h>
#include "Ui.h"
#include "Saved.h"

using namespace ui;
using agent::tiny;
using agent::tinyR;

// ===========================================================================
// The trigger's state, kept up on every screen
// ===========================================================================
namespace trig {

static uint16_t dial = 5000, limit = 1000;
static uint32_t applyAt;                // 0: nothing waiting
static uint8_t lastOut, lastPd;
static uint32_t nTrip;
static uint32_t onAfter;                // A pressed while the voltage was changing: on once it settles
static int32_t tripCur;

// The scope: 120 samples, one every 100 ms.
static const uint8_t GN = 120;
static uint8_t gv[GN], gi[GN], gf[GN];  // 0.1 V, 10 mA, flags: 1 on, 2 a trip
static uint8_t gHead;
static uint32_t gAt;

uint16_t dialMv() { return dial; }
uint16_t limitMa() { return pwr::limitMa(); }    // (the library clamps it to the budget)
uint32_t trips() { return nTrip; }
int32_t tripMa() { return tripCur; }

uint8_t ppsPos() {
    if (pd::specRev() < 3) return 0;
    for (uint8_t i = 0; i < pd::pdoCount(); i++)
        if (pd::pdo(i).kind == pd::PDO_PPS) return (uint8_t)(i + 1);
    return 0;
}
static uint8_t fixedPos(uint16_t mv) {
    for (uint8_t i = 0; i < pd::pdoCount(); i++)
        if (pd::pdo(i).kind == pd::PDO_FIXED && pd::pdo(i).mv == mv) return (uint8_t)(i + 1);
    return 0;
}

void begin() {
    dial = saved::voltsMv() ? saved::voltsMv() : 5000;
    limit = saved::limitMa() >= 100 && saved::limitMa() <= 2400 ? saved::limitMa() : 1000;
    pwr::setLimit(limit);
}

void setDial(uint16_t mv, bool now) {
    dial = mv;
    applyAt = millis() + (now ? 0 : 500);
    if (!applyAt) applyAt = 1;
    saved::voltsMv() = mv;
    saved::touch();
}
void setLimit(uint16_t ma) {
    pwr::setLimit(ma);
    limit = ma;
    saved::limitMa() = ma;
    saved::touch();
}
bool settled() { return !applyAt && pd::contractMv() == dial; }

static void apply() {
    applyAt = 0;
    if (pd::state() != pd::READY) return;
    // The output stays on through a new voltage, as a bench supply's does
    // (the library's default); were it to go off, say so.
    bool wasOn = pwr::out() == pwr::OUT_ON;
    uint8_t pos = fixedPos(dial), pp = ppsPos();
    bool ok;
    if (pos) {
        if (pd::active() == pos) return;
        ok = pd::request(pos);
    } else if (pp) {
        if (pd::ppsActive() && pd::contractMv() == dial) return;
        uint16_t ma = pd::pdo(pp - 1).ma < 3000 ? pd::pdo(pp - 1).ma : 3000;
        ok = pd::requestPps(pp, dial, ma);
    } else {
        return;
    }
    if (!ok) {
        sfx(Sfx::Deny);
        toast("NOT NOW", pd::event(), nullptr, ALERT, 60);
    } else if (wasOn && pwr::out() != pwr::OUT_ON) {
        sfx(Sfx::OutOff);
        toast("OUTPUT OFF", "NEW VOLTAGE: A=ON", nullptr, GOLD_);
    }
}

void toggle() {
    if (pwr::out() == pwr::OUT_ON) {
        pwr::disableCn1();
        sfx(Sfx::OutOff);
        return;
    }
    if (!settled() && pd::state() == pd::READY) {
        if (applyAt) applyAt = 1;           // (the new voltage at once, then the output)
        onAfter = millis() | 1;
        sfx(Sfx::Knob);
        return;
    }
    onAfter = 0;
    const char *w = pwr::enableCn1(limit);
    if (w) {
        sfx(Sfx::Deny);
        toast("REFUSED", w, nullptr, ALERT, 120);
    } else {
        sfx(Sfx::OutOn);
        banner("OUTPUT ON", ON, 35);
    }
}

void service() {
    uint32_t now = millis();
    uint8_t ps = pd::state();
    // A new contract after a plug-in: the voltage set last time, at once
    // (the output is off then, as after any start).
    if (ps == pd::READY && lastPd != pd::READY && lastPd != pd::REQUESTED && dial != pd::contractMv())
        applyAt = now + 300;
    lastPd = ps;
    if (onAfter && settled()) toggle();
    else if (onAfter && now - onAfter > 3000) {
        onAfter = 0;
        sfx(Sfx::Deny);
        toast("NOT ON", "THE VOLTAGE DID", "NOT SETTLE", ALERT, 90);
    }
    if (applyAt && (int32_t)(now - applyAt) >= 0 && ps == pd::READY) apply();
    if (applyAt && ps != pd::READY && ps != pd::REQUESTED) applyAt = 0;

    uint8_t o = pwr::out();
    bool tripped = o == pwr::OUT_TRIPPED && lastOut == pwr::OUT_ON;
    if (tripped) {
        nTrip++;
        tripCur = pwr::tripMa();
        sfx(Sfx::Trip);
        banner("TRIPPED", ALERT, 70);
    }
    lastOut = o;

    if (now - gAt >= 100 || tripped) {
        gAt = now;
        bool on = o == pwr::OUT_ON;
        int32_t v = on ? pwr::tele().busMv : pwr::vbusMv();
        int32_t i = on ? pwr::tele().ma : 0;
        gv[gHead] = (uint8_t)(v < 0 ? 0 : v > 25400 ? 254 : (v + 50) / 100);
        gi[gHead] = (uint8_t)(i < 0 ? 0 : i > 2540 ? 254 : (i + 5) / 10);
        gf[gHead] = (uint8_t)((on ? 1 : 0) | (tripped ? 2 : 0));
        gHead = (uint8_t)(gHead + 1 < GN ? gHead + 1 : 0);
    }
}

// The dial's next stop: 100 mV (or `step`) through a PPS range, stopping at
// fixed voltages on the way; beyond PPS, the next fixed voltage.
static uint16_t next(uint16_t mv, int dir, uint16_t step) {
    uint16_t lo = 0, hi = 0, best = mv;
    uint8_t pp = ppsPos();
    if (pp) {
        lo = pd::pdo(pp - 1).minMv;
        hi = pd::pdo(pp - 1).maxMv;
    }
    // The nearest fixed voltage that way.
    uint16_t fx = 0;
    for (uint8_t i = 0; i < pd::pdoCount(); i++) {
        uint16_t f = pd::pdo(i).mv;
        if (pd::pdo(i).kind != pd::PDO_FIXED || f > 20000) continue;
        if (dir > 0 && f > mv && (!fx || f < fx)) fx = f;
        if (dir < 0 && f < mv && (!fx || f > fx)) fx = f;
    }
    int32_t t = (int32_t)mv + dir * (int32_t)step;
    t -= t % 100;
    if (pp && t >= lo && t <= hi && (mv >= lo || dir > 0) && (mv <= hi || dir < 0)) {
        best = (uint16_t)t;
        if (fx && ((dir > 0 && fx < best) || (dir < 0 && fx > best))) best = fx;
    } else if (pp && dir > 0 && mv < lo && t > lo) {
        best = lo;
    } else if (fx) {
        best = fx;
    }
    return best;
}

}  // namespace trig

// ===========================================================================
// The screens
// ===========================================================================
namespace pdui {

using namespace trig;

static uint8_t field;                   // 0 the voltage, 1 the limit
static uint16_t heldF;
static uint8_t menuCur, offerCur;
static uint16_t logBack;
static bool logLast;

static const char *const MENU[] = {"CHARGER OFFER", "GUIDED TOUR", "DIAGNOSTICS", "PD LOG"};
static const uint8_t N_MENU = 4;
static const char *const OUTS[] = {"OFF", "ON", "TRIPPED", "BLOCKED"};
static const char *const PDST[] = {"PD OFF", "NO POWER", "LISTENING", "ASKING", "READY", "TYPE-C 5V"};

static uint16_t budget() { return pwr::budgetMa(); }

static void tickPd() {
    if (chgame.justPressed(LEFT_BUTTON) || chgame.justPressed(RIGHT_BUTTON)) {
        field ^= 1;
        sfx(Sfx::Move);
    }
    int dir = 0;
    if (chgame.repeat(UP_BUTTON)) dir = 1;
    if (chgame.repeat(DOWN_BUTTON)) dir = -1;
    heldF = (chgame.pressed(UP_BUTTON) || chgame.pressed(DOWN_BUTTON)) ? (uint16_t)(heldF + 1) : 0;
    bool fast = heldF > 45;
    if (dir && field == 0) {
        if (pd::state() == pd::READY) {
            uint16_t n = next(dialMv(), dir, fast ? 1000 : 100);
            if (n != dialMv()) {
                setDial(n);
                sfx(Sfx::Knob);
            }
        } else {
            sfx(Sfx::Deny);
            toast("NO PD", "PLUG IN PD POWER", nullptr, GOLD_, 60);
        }
    } else if (dir && field == 1) {
        int32_t l = limitMa() + dir * (fast ? 250 : 50);
        int32_t top = budget() ? budget() : 2400;
        if (top > 2400) top = 2400;
        if (l > top) l = top;
        if (l < 100) l = 100;
        l -= l % 50;
        if (l != limitMa()) {
            setLimit((uint16_t)l);
            sfx(Sfx::Knob);
        }
    }
    if (chgame.justPressed(A_BUTTON)) toggle();
    if (chgame.justPressed(SELECT_BUTTON)) {
        sfx(Sfx::Open);
        go(S_PDMENU);
    }
}

static void tickMenu() {
    cursor(menuCur, N_MENU);
    if (chgame.justPressed(A_BUTTON)) {
        static const uint8_t TO[N_MENU] = {S_OFFER, S_GUIDES, S_STATUS, S_LOG};
        sfx(Sfx::Open);
        go(TO[menuCur]);
        offerCur = 0;
    }
    if (chgame.justPressed(B_BUTTON) || chgame.justPressed(SELECT_BUTTON)) {
        sfx(Sfx::Back);
        go(S_PD);
    }
}

static void tickOffer() {
    cursor(offerCur, pd::pdoCount());
    if (chgame.justPressed(A_BUTTON) && offerCur < pd::pdoCount()) {
        const pd::Pdo &p = pd::pdo(offerCur);
        if (p.kind == pd::PDO_FIXED && p.mv <= 20000) {
            setDial(p.mv, true);
        } else if (p.kind == pd::PDO_PPS && ppsPos()) {
            uint16_t mv = dialMv() < p.minMv ? p.minMv : dialMv() > p.maxMv ? p.maxMv : dialMv();
            setDial(mv, true);
        } else {
            sfx(Sfx::Deny);
            return;
        }
        sfx(Sfx::Mode);
        field = 0;
        go(S_PD);
    }
    if (chgame.justPressed(B_BUTTON)) {
        sfx(Sfx::Back);
        go(S_PDMENU);
    }
}

static void tickLog() {
    static const uint16_t ROWS = 13;
    uint16_t n = plog::count(logLast);
    uint16_t most = n > ROWS ? (uint16_t)(n - ROWS) : 0;
    if (chgame.repeat(UP_BUTTON) && logBack < most) logBack++;
    if (chgame.repeat(DOWN_BUTTON) && logBack) logBack--;
    if (chgame.justPressed(LEFT_BUTTON) || chgame.justPressed(RIGHT_BUTTON)) {
        logLast = !logLast;
        logBack = 0;
        sfx(Sfx::Move);
    }
    if (chgame.justPressed(A_BUTTON) && !logLast) {
        plog::setFrozen(!plog::frozen());
        sfx(Sfx::Knob);
    }
    if (chgame.justPressed(SELECT_BUTTON) && !logLast) {
        plog::clear();
        logBack = 0;
        sfx(Sfx::Knob);
    }
    if (logBack > most) logBack = most;
    if (chgame.justPressed(B_BUTTON)) {
        sfx(Sfx::Back);
        go(S_PDMENU);
    }
}

void tick() {
    switch (screen()) {
    case S_PD:     tickPd(); break;
    case S_PDMENU: tickMenu(); break;
    case S_OFFER:  tickOffer(); break;
    case S_LOG:    tickLog(); break;
    case S_STATUS:
    case S_REGS:
        if (chgame.justPressed(B_BUTTON)) {
            sfx(Sfx::Back);
            go(S_PDMENU);
        }
        break;
    }
}

// ---------------------------------------------------------------------------
static const char *word(uint8_t &c) {
    c = LIVE;
    uint8_t s = pd::state();
    if (s == pd::READY) {
        c = ON;
        if (pd::ppsActive()) return "PD 3.0 PPS";
        return pd::specRev() >= 3 ? "PD 3.0" : "PD 2.0";
    }
    if (s == pd::DETECT || s == pd::OFF) c = DIM;
    return PDST[s];
}
static void statusTop() {
    uint8_t c;
    const char *w = word(c);
    top(true, w, c);
    int32_t mw = pwr::out() == pwr::OUT_ON ? pwr::tele().mw : 0;
    agent::readout((uint32_t)((mw + 500) / 1000), LIVE, "W", "OUT");
}

// The scope: y0 .. y0 + h - 1.
static void scope(int y0, int h) {
    int bot = y0 + h - 1;
    // Scales: volts to 5, 10, 15, 20 or 25; amps to 0.5 ... 2.5.
    uint8_t vmax = 1, imax = 1;
    for (uint8_t k = 0; k < GN; k++) {
        if (gv[k] > vmax) vmax = gv[k];
        if (gi[k] > imax) imax = gi[k];
    }
    if (limitMa() / 10 > imax && pwr::out() == pwr::OUT_ON) imax = (uint8_t)(limitMa() / 10);
    uint16_t vs = (uint16_t)((vmax + 49) / 50 * 50), is = (uint16_t)((imax + 49) / 50 * 50);
    for (uint8_t k = 0; k < GN; k++) {
        uint8_t i = (uint8_t)((gHead + k) % GN);
        if (gf[i] & 1) gfx_vline(4 + k, y0, h, SPAN);
        if (gf[i] & 2) gfx_vline(4 + k, y0, h, ALERT);
    }
    for (int q = 1; q < 4; q++)
        for (int x = 4; x < 124; x += 3) gfx_pixel(x, y0 + h * q / 4, GRID);
    gfx_hline(4, bot, 120, GRID);
    int pyv = -1, pyi = -1;
    for (uint8_t k = 0; k < GN; k++) {
        uint8_t i = (uint8_t)((gHead + k) % GN);
        int yv = bot - (int)gv[i] * (h - 2) / vs;
        int yi = bot - (int)gi[i] * (h - 2) / is;
        if (k) {
            gfx_line(3 + k, pyi, 4 + k, yi, (gf[i] & 1) ? AMP : AMPDK);
            gfx_line(3 + k, pyv, 4 + k, yv, (gf[i] & 1) ? LIVE : DIM);
        }
        pyv = yv;
        pyi = yi;
    }
    Line a;
    Line b;
    a.d(vs / 10).c('V');
    b.fix(is * 10, 1).c('A');
    tiny(5, y0 + 1, a.t, LIVE);
    agent::tinyR(122, y0 + 1, b.t, AMP);
}

static void setBox(int x, bool focus, const char *label, const char *tag, const char *val, uint8_t c, bool blink) {
    agent::brackets(x, 94, 63, 20, 3, focus ? LIVE : GRID);
    tiny(x + 3, 96, label, focus ? PALE : DIM);
    tinyR(x + 59, 96, tag, DIM);
    if (!blink || (frame & 8)) gfx_text(x + 4, 104, val, c);
    if (focus) {
        int ax = x + 56;
        gfx_hline(ax, 104, 5, LIVE);
        gfx_hline(ax + 1, 103, 3, LIVE);
        gfx_pixel(ax + 2, 102, LIVE);
        gfx_hline(ax, 108, 5, LIVE);
        gfx_hline(ax + 1, 109, 3, LIVE);
        gfx_pixel(ax + 2, 110, LIVE);
    }
}

}  // namespace pdui

// A reading for the eye: the meter's numbers (the scope keeps every raw
// sample). While a reading only wanders, it is averaged (over about a third
// of a second at 30 frames a second), and the number shown moves only once
// the average has moved more than `hold` from it, so the last digit stops
// rolling. A reading outside `band` round the average is left out of it; three
// frames running on the same side, it is a real change: taken at once, the
// average starting again from there, with nothing of the old one dragged
// along (no values in between on the way).
namespace {
struct Steady {
    int32_t acc;            // the average, in sixteenths
    int32_t shown;
    uint8_t out;            // frames running outside the band
    int8_t side;
    bool primed;
    int32_t take(int32_t r, int32_t band, int32_t hold) {
        int32_t d = r - acc / 16;
        int8_t s = (int8_t)(d > band ? 1 : d < -band ? -1 : 0);
        out = (uint8_t)(s && s == side ? out + 1 : s ? 1 : 0);
        side = s;
        if (!primed || out >= 3) {
            acc = r * 16;
            shown = r;
            primed = true;
            out = 0;
            side = 0;
            return r;
        }
        if (s) return shown;            // (outside the band: no part of the average until it is a change)
        acc += (r * 16 - acc) / 8;
        int32_t a = acc / 16;
        if (a - shown >= hold || shown - a >= hold) shown = a;
        return shown;
    }
};
Steady steadyV, steadyI;
bool steadyOn;
}  // namespace

// The meter (the trigger's and the tour's): volts big, the output's state,
// amps and watts; the input's volts while the output is off.
void trig::meter(int y) {
    using pdui::OUTS;
    const pwr::Tele &t = pwr::tele();
    uint8_t o = pwr::out();
    bool on = o == pwr::OUT_ON;
    if (on != steadyOn) steadyV.primed = steadyI.primed = false;    // another source: start again
    steadyOn = on;
    // Volts in 10 mV steps (a change is 0.1 V and a fortieth); amps in 1 mA
    // steps (20 mA and a twentieth).
    int32_t rv = on ? t.busMv : pwr::vbusMv(), ri = on ? t.ma : 0;
    int32_t mv = steadyV.take(rv, 100 + rv / 40, 10);
    int32_t ma = steadyI.take(ri, 20 + (ri < 0 ? -ri : ri) / 20, 2);
    tiny(2, y + 1, on ? "OUTPUT" : "INPUT", on ? ON : DIM);
    int x = segNum(2, y + 8, mv, 2, 4, 2, on ? LIVE : MID);
    gfx_textScaled(x + 1, y + 14, "V", on ? LIVE : MID, 2);
    if (o != pwr::OUT_TRIPPED || (frame & 16)) agent::chip(78, y, 50, OUTS[o], on ? ON : o ? ALERT : DIM);
    x = segNum(78, y + 10, ma, 3, 4, 1, AMP);
    tiny(x + 1, y + 14, "A", AMP);
    Line w;
    if (on) w.mw(mv * ma / 1000).s(" PK").fix(pwr::peakMa(), 1);
    else if (trips()) w.s("TRIP ").ma(tripMa());
    tiny(78, y + 21, w.t, DIM);
}

namespace pdui {
using namespace trig;

static void drawPd() {
    statusTop();
    const pwr::Tele &t = pwr::tele();
    uint8_t o = pwr::out();
    bool on = o == pwr::OUT_ON;
    bar(0, 12, 128, 2, on && t.ma > 0 ? (uint16_t)t.ma : 0, limitMa(),
                 on && t.ma * 10 > limitMa() * 9 ? ALERT : on && t.ma * 10 > limitMa() * 7 ? GOLD_ : AMP);
    meter(16);
    scope(44, 48);
    // The settings.
    Line vv, ll, vt, lt;
    uint8_t pp = ppsPos();
    bool fixedHere = false;
    for (uint8_t i = 0; i < pd::pdoCount(); i++)
        fixedHere |= pd::pdo(i).kind == pd::PDO_FIXED && pd::pdo(i).mv == dialMv();
    if (pd::state() == pd::READY) {
        vv.mv(dialMv());
        vt.s(fixedHere ? "FIXED" : pp ? "PPS" : "");
    } else {
        vv.s("5.00V");
        vt.s(pd::state() == pd::TYPEC ? "5V ONLY" : "-");
    }
    ll.ma(limitMa());
    lt.s("MAX ").ma(budget() > 2400 ? 2400 : budget());
    setBox(0, field == 0, "SET", vt.t, vv.t, LIVE, !settled() && pd::state() == pd::READY);
    setBox(65, field == 1, "LIMIT", lt.t, ll.t, AMP, false);
    bottom(o == pwr::OUT_TRIPPED ? "A:REARM LR:KNOB SEL:MENU" : "A:OUT LR:KNOB SEL:MENU");
}

static void drawMenu() {
    statusTop();
    gfx_text(4, 16, "PD MENU", PALE);
    list(30, MENU, N_MENU, menuCur);
    bottom("A:OPEN B:BACK");
}

static void drawOffer() {
    top(true, "CHARGER OFFER", LIVE);
    Line s;
    if (pd::state() == pd::READY || pd::state() == pd::TYPEC) {
        s.s(pd::state() == pd::READY ? (pd::specRev() >= 3 ? "PD 3.0" : "PD 2.0") : "NO PD").s("  CC").d(pd::cc())
            .s("  TYPE-C ").ma(pd::typecMa());
    } else {
        s.s("NO CHARGER");
    }
    tiny(2, 14, s.t, DIM);
    uint8_t n = pd::pdoCount();
    for (uint8_t i = 0; i < n; i++) {
        const pd::Pdo &p = pd::pdo(i);
        int y = 24 + i * 12;
        bool sel = i == offerCur;
        if (sel) {
            gfx_fillRect(2, y - 2, 124, 11, PANEL);
            agent::brackets(1, y - 3, 126, 13, 3, LIVE);
        }
        Line l;
        if (p.kind == pd::PDO_FIXED) l.mv(p.mv);
        else if (p.kind == pd::PDO_PPS) l.fix(p.minMv, 1).c('-').fix(p.maxMv, 1).c('V');
        else l.s("OTHER");
        gfx_text(6, y, l.t, sel ? PALE : DIM);
        Line r;
        r.ma(p.ma).c(' ').s(p.kind == pd::PDO_FIXED ? "FIX" : p.kind == pd::PDO_PPS ? "PPS" : "");
        tinyR(122, y + 1, r.t, p.kind == pd::PDO_PPS ? SKY : AMP);
        if (pd::active() == i + 1) gfx_fillRect(0, y, 2, 7, ON);
    }
    bottom("A:USE B:BACK");
}

// Diagnostics: the CH211's registers (now, and as found at start-up), the
// INA226's alert at start-up, VBUS and its flags, the PD sink's state and
// counts, CN1, the I2C bus.
static void drawDiag() {
    top(true, "DIAGNOSTICS", LIVE);
    static const char *const NAMES[8] = {"PIN", "CFG", "CC", "OD", "HVCP", "KEY", "SCFG", "SSTA"};
    tiny(2, 14, "CH211 NOW/START", DIM);
    for (uint8_t i = 0; i < 8; i++) {
        Line l;
        l.s(NAMES[i]);
        while (l.n < 5) l.c(' ');
        l.hex(pwr::reg(i)).c(' ').hex(pwr::diag().boot[i]);
        tiny(2 + (i & 1) * 64, 21 + (i >> 1) * 7, l.t, pwr::reg(i) != pwr::diag().boot[i] ? PALE : DIM);
    }
    const pwr::Diag &g = pwr::diag();
    uint8_t ps = pwr::reg(pwr::PIN_STAT), ss = pwr::reg(pwr::SYS_STAT);
    Line a, b, c, d, e, f;
    a.s("INA ALERT@START ").hex((uint8_t)(g.inaBootMask >> 8)).hex((uint8_t)g.inaBootMask).s(" SCL ")
        .s(g.sclHeld ? "HELD" : "OK");
    tiny(2, 50, a.t, (g.inaBootMask & 0x10) || g.sclHeld ? GOLD_ : DIM);
    b.s("VBUS ").mv(pwr::vbusMv()).s(" RDY").d(ps >> 4 & 1).s(" EX").d(ss >> 1 & 1).s(" VSYS").d(ss & 1)
        .s(" OT").d(ss >> 6 & 1);
    tiny(2, 58, b.t, PALE);
    c.s("PD ").s(PDST[pd::state()]).s(" CC").d(pd::cc()).s(" REV").d(pd::specRev()).s(" HARD").d(pd::hardResets())
        .s(" SOFT").d(pd::softResets());
    tiny(2, 65, c.t, pd::state() == pd::READY ? ON : PALE);
    tiny(2, 72, pd::event(), DIM);
    d.s("CN1 ").s(OUTS[pwr::out()]).s(" LIM ").ma(pwr::limitMa()).s(" MAX ").ma(budget());
    tiny(2, 80, d.t, pwr::out() == pwr::OUT_ON ? ON : pwr::out() == pwr::OUT_OFF ? PALE : ALERT);
    e.s("PEAK ").ma(pwr::peakMa()).s(" TRIP@ ").ma(tripMa()).s(" TRIPS ").d((int32_t)trips());
    tiny(2, 87, e.t, DIM);
    f.s("I2C ERRORS ").d(g.busErrors).s("  CC FAULTS ").d(g.ccFaults);
    tiny(2, 94, f.t, g.busErrors || g.ccFaults ? GOLD_ : DIM);
    if (*pwr::why()) tiny(2, 108, pwr::why(), GOLD_);
    bottom("B:BACK");
}

static uint8_t logColour(const plog::Ev &e) {
    switch (e.kind) {
    case plog::K_RX:       return AMP;
    case plog::K_TX:       return e.b ? LIVE : ALERT;
    case plog::K_HARD_IN:
    case plog::K_HARD_OUT: return ALERT;
    case plog::K_CC:       return e.b ? DIM : ALERT;
    case plog::K_CN1:      return e.a ? DIM : ALERT;
    case plog::K_STATE:    return e.a == pd::READY ? ON : PALE;
    case plog::K_START:    return ON;
    case plog::K_PDO:      return SKY;
    case plog::K_OTHER:    return DIM;
    default:               return PALE;
    }
}

static void drawLog() {
    static const uint16_t ROWS = 13;
    uint16_t n = plog::count(logLast);
    top(true, logLast ? "PD LOG: LAST RUN" : plog::frozen() ? "PD LOG: FROZEN" : "PD LOG", LIVE);
    Line r;
    r.d((int32_t)(logLast ? n : plog::total()));
    tinyR(126, 3, r.t, DIM);
    if (!n) gfx_text(4, 20, logLast ? "Nothing kept" : "Nothing yet", DIM);
    uint16_t end = n > logBack ? (uint16_t)(n - logBack) : 0;
    uint16_t first = end > ROWS ? (uint16_t)(end - ROWS) : 0;
    for (uint16_t i = first; i < end; i++) {
        const plog::Ev &e = plog::at(logLast, i);
        char t[16], d[32];
        plog::timeText(e, t);
        plog::describe(e, d);
        int y = 14 + (i - first) * 7;
        int xx = agent::tiny(1, y, t, DIM) + 2;
        tiny(xx > 31 ? xx : 31, y, d, logColour(e));
    }
    bottom(logLast ? "LR:THIS~RUN B:BACK" : "A:FREEZE SEL:CLEAR LR:RUN B:BACK");
}

void draw() {
    switch (screen()) {
    case S_PD:     drawPd(); break;
    case S_PDMENU: drawMenu(); break;
    case S_OFFER:  drawOffer(); break;
    case S_STATUS: drawDiag(); break;
    case S_LOG:    drawLog(); break;
    }
}

}  // namespace pdui
