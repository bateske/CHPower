// The guided tour (Guide.h).
#pragma GCC optimize("Os")
#include "Guide.h"
#include <CHPower.h>
#include "Ui.h"
#include "PdUi.h"

using namespace ui;
using agent::tiny;
using agent::tinyR;

namespace guide {

// What a step does when it starts, and what ends it.
enum Act : uint8_t { A_NONE, A_5V, A_9V, A_LIM500, A_LIM1000, A_SWEEP, A_HOST };
enum Wait : uint8_t { W_A, W_PD, W_9V, W_5V, W_ON, W_OFF, W_TRIP, W_SWEEP, W_NOVBUS };
// What it shows under its text.
enum Show : uint8_t { V_NONE, V_METER, V_OFFER, V_TRIP };

struct Step {
    const char *text;           // lines split by '\n', 19 characters each at most
    uint8_t act, wait, show;
};
struct Tour {
    const char *name;
    const Step *steps;
    uint8_t n;
};

static const Step G_FIRST[] = {
    {"Plug a USB-C PD\ncharger into the\nport. The battery\nkeeps me running.", A_NONE, W_PD, V_METER},
    {"It offers these.\nThe trigger can\nask for any.", A_NONE, W_A, V_OFFER},
    {"The output is CN1:\npin 2 +, pin 1\nground, at the\nvoltage you set.", A_NONE, W_A, V_NONE},
};
static const Step G_VOLT[] = {
    {"UP and DOWN on the\ntrigger set the\nvoltage. Here: 9 V.", A_9V, W_9V, V_METER},
    {"PPS chargers go\nanywhere in range:\nwatch 5 V to 12 V.", A_SWEEP, W_SWEEP, V_METER},
    {"Hold UP or DOWN to\nstep 1 V at a time.\nBack to 5 V.", A_5V, W_5V, V_METER},
};
static const Step G_PROJECT[] = {
    {"Wire your project\nto CN1. A switches\nthe output on.", A_LIM1000, W_ON, V_METER},
    {"The meter shows\nwhat it draws.\nA: off.", A_NONE, W_OFF, V_METER},
    {"The limit cuts it\noff on overload or\na short: 0.5 A now.\nA on, then load it.", A_LIM500, W_TRIP, V_METER},
    {"Cut off: it stays\noff until A again.\n(Limit: 1 A.)", A_LIM1000, W_A, V_TRIP},
};
#define TOUR(n, s) {n, s, sizeof s / sizeof s[0]}
static const Tour TOURS[] = {
    TOUR("FIRST POWER", G_FIRST), TOUR("SET A VOLTAGE", G_VOLT), TOUR("POWER A PROJECT", G_PROJECT),
};
static const uint8_t N = sizeof TOURS / sizeof TOURS[0];

static uint8_t sel, tour, step;
static bool done;                       // the step's wait is over
static uint32_t stepAt, doneAt, tripBase;
static uint16_t sweepMv;
static const char *note;                // what the step saw, or why it cannot go on

static const Tour &T() { return TOURS[tour]; }
static const Step &S() { return T().steps[step]; }

// The fixed PDO nearest 9 V without going over 12 V, or PPS at 9 V.
static void ask9() {
    uint16_t best = trig::ppsPos() ? 9000 : 0;  // (PPS can; else the first fixed step over 5 V up to 12 V)
    for (uint8_t i = 0; i < pd::pdoCount() && !best; i++) {
        uint16_t m = pd::pdo(i).mv;
        if (pd::pdo(i).kind == pd::PDO_FIXED && m > 5000 && m <= 12000) best = m;
    }
    if (best) trig::setDial(best, true);
    else note = "NO VOLTAGE OVER 5 V OFFERED";
}

static void enter(uint8_t s) {
    step = s;
    done = false;
    note = nullptr;
    stepAt = millis();
    tripBase = trig::trips();
    switch (S().act) {
    case A_5V:      if (pd::state() == pd::READY) trig::setDial(5000, true); break;
    case A_9V:      ask9(); break;
    case A_LIM500:  trig::setLimit(500); break;
    case A_LIM1000: trig::setLimit(1000); break;
    case A_SWEEP:
        if (!trig::ppsPos()) note = "NO PPS ON THIS CHARGER";
        sweepMv = 5000;
        trig::setDial(sweepMv, true);
        break;
    default: break;
    }
}

static bool near(uint16_t v, uint16_t want) { return v + 300 > want && v < want + 300; }

static bool waitOver() {
    switch (S().wait) {
    case W_PD:     return pd::state() == pd::READY || pd::state() == pd::TYPEC;
    case W_9V:     return trig::settled() && pd::contractMv() > 5000;
    case W_5V:     return pd::state() != pd::READY || (trig::settled() && near(pd::contractMv(), 5000));
    case W_ON:     return pwr::out() == pwr::OUT_ON;
    case W_OFF:    return pwr::out() != pwr::OUT_ON;
    case W_TRIP:   return trig::trips() != tripBase;
    case W_NOVBUS: return pwr::vbusValid() && pwr::vbusMv() < 1000;
    case W_SWEEP:
        if (note) return true;                  // (no PPS: on to the next step)
        // 5 V to 12 V in 0.5 V steps, then back to 5 V; each once settled.
        if (!trig::settled() || millis() - stepAt < 350) return false;
        stepAt = millis();
        if (sweepMv >= 12000) {
            trig::setDial(5000, true);
            return true;
        }
        sweepMv = (uint16_t)(sweepMv + 500);
        trig::setDial(sweepMv, true);
        sfx(Sfx::Knob);
        return false;
    default:       return false;
    }
}

void tick() {
    if (screen() == S_GUIDES) {
        cursor(sel, N);
        if (chgame.justPressed(A_BUTTON)) {
            tour = sel;
            sfx(Sfx::Open);
            go(S_GUIDE);
            enter(0);
        }
        if (chgame.justPressed(B_BUTTON)) {
            sfx(Sfx::Back);
            go(S_PDMENU);
        }
        return;
    }
    uint8_t w = S().wait;
    bool outStep = w == W_ON || w == W_OFF || w == W_TRIP;
    if (!done && waitOver()) {
        done = true;
        doneAt = millis();
        if (w != W_A) sfx(Sfx::Ready);
    }
    if (chgame.justPressed(A_BUTTON)) {
        if (outStep && !done) {
            trig::toggle();
        } else if (w == W_A || done) {
            if (step + 1 < T().n) {
                sfx(Sfx::Move);
                enter((uint8_t)(step + 1));
            } else {
                sfx(Sfx::Done);
                banner("DONE", ON);
                go(S_GUIDES);
            }
        }
    }
    // Automatic steps move on by themselves a moment after they are done.
    if (done && w != W_A && millis() - doneAt > 1500 && step + 1 < T().n) enter((uint8_t)(step + 1));
    if (chgame.justPressed(B_BUTTON)) {
        sfx(Sfx::Back);
        go(S_GUIDES);
    }
}

// ---------------------------------------------------------------------------
static void lines(int x, int y, const char *s, uint8_t c) {
    char b[24];
    while (*s) {
        uint8_t n = 0;
        while (*s && *s != '\n' && n < sizeof b - 1) b[n++] = *s++;
        b[n] = 0;
        gfx_text(x, y, b, c);
        y += 9;
        if (*s == '\n') s++;
    }
}

// The charger's offer in two lines: the fixed voltages, then a PPS range.
static void offer(int y) {
    Line l, q;
    for (uint8_t i = 0; i < pd::pdoCount() && i < 7; i++) {
        const pd::Pdo &p = pd::pdo(i);
        if (p.kind == pd::PDO_FIXED) l.d(p.mv / 1000).s("V ");
        else if (p.kind == pd::PDO_PPS) q.s("PPS ").fix(p.minMv, 1).c('-').fix(p.maxMv, 1).c('V');
    }
    tiny(4, y, pd::pdoCount() ? l.t : "NO OFFER YET", LIVE);
    tiny(4, y + 8, q.t, SKY);
}

void draw() {
    if (screen() == S_GUIDES) {
        top(true, "GUIDED TOUR", LIVE);
        static const char *rows[N];
        for (uint8_t i = 0; i < N; i++) rows[i] = TOURS[i].name;
        list(18, rows, N, sel);
        bottom("A:START B:BACK");
        return;
    }
    top(true, T().name, LIVE);
    Line c;
    c.d(step + 1).c('/').d(T().n);
    tinyR(126, 3, c.t, PALE);
    // The steps as a row of dots.
    for (uint8_t i = 0; i < T().n; i++) gfx_fillRect(4 + i * 8, 14, 6, 2, i < step ? MID : i == step ? LIVE : GRID);
    lines(4, 20, S().text, PALE);
    int wy = 64;
    switch (S().show) {
    case V_METER: trig::meter(wy); break;
    case V_OFFER: offer(wy); break;
    case V_TRIP: {
        Line l;
        l.s("CUT OFF AT ").ma(trig::tripMa());
        gfx_text(4, wy + 4, l.t, ALERT);
        break;
    }
    default: break;
    }
    uint8_t w = S().wait;
    if (note) {
        tiny(4, 104, note, ALERT);
    } else if (w != W_A) {
        if (done) agent::chip(4, 102, 40, "GOT IT", ON);
        else {
            static const char *const DOTS[4] = {"WAITING", "WAITING.", "WAITING..", "WAITING..."};
            tiny(4, 104, DOTS[(frame / 12) & 3], GOLD_);
        }
    }
    bool outStep = (w == W_ON || w == W_OFF || w == W_TRIP) && !done;
    bottom(outStep ? "A:OUTPUT B:QUIT" : (w == W_A || done) ? "A:NEXT B:QUIT" : "B:QUIT");
}

}  // namespace guide
