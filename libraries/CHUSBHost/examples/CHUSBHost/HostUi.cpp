// HOST mode (HostUi.h).
#pragma GCC optimize("Os")
#include "HostUi.h"
#include "Ui.h"
#include <CHUSBHost.h>
#include <string.h>

using namespace ui;
using agent::tiny;
using agent::tinyR;

namespace hostui {

static uint8_t tab;                     // 0 keyboard, 1 mouse, 2 gamepad, 3 the log
static const char *refused;             // why host mode did not start
// Host mode starts by itself whenever the port is free. With nothing
// plugged in it looks for a PC or a charger now and then, as a sink for a
// moment (a source sees no other source): if one is there, host mode
// pauses, the serial port comes back, and the log is ready for the PC.
static bool paused, sinkLook;
// A powered PD hub (Apple's USB-C Digital AV Multiport Adapter, say). With
// VBUS on the port the PD sink runs, data-role swaps allowed: a hub hands
// us the host role (DR_Swap) and the host runs on its 5 V, charging. A PC
// or a charger never does, and host mode stays paused as before. (The
// library does the rest: Try.SNK on attach lets a powered hub be the
// source, and PD_SWAP lets a hub that gets its charger later take over the
// power while we stay the host.)
static bool pdOn;
// connecting: VBUS came, and for up to 1.5 s the PD sink waits to see
// whether it is a hub that hands us the host role (it does so within about
// 0.3 s of its contract), before host mode shows as paused for a PC or a
// charger: no "PC MODE" blip and no unplug sound on the way to a hub.
static bool connecting;
static uint32_t pdAt;
static bool hubLost() {
    pd::State p = pd::state();
    return !pd::dfp() || (p != pd::READY && p != pd::REQUESTED);
}
static void pdOff() {
    if (pdOn) pd::stop();
    pdOn = false;
}
static bool pcMode;                     // B: host mode off by hand (a C-to-C cable to a PC works then)
static uint32_t lookAt, freeAt, waitAt;
static uint8_t logTop;
static bool logFollow = true;
static uint8_t lastState;
static bool hadKb, hadMs, hadPad;
static uint32_t rateAt, rateBase;
static uint16_t rate;                   // reports a second

// The text field: 6 lines of 19.
static const uint8_t TL = 6, TC = 19;
static char txt[TL][TC + 1];
static uint8_t row, col;
static uint8_t lastUsage;
static uint16_t keyCount;

// The mouse: the cursor, its trail, the clicks.
static int16_t cx = 62, cy = 29, wheel;
struct Ping {
    uint8_t x, y, age, c;
};
static Ping pings[4];
static uint8_t lastBtn, lastPadBtn;

static void clearText() {
    memset(txt, 0, sizeof txt);
    row = col = 0;
}

void open(uint8_t t) {
    tab = t;
    refused = nullptr;
    paused = sinkLook = false;
    if (host::state() == host::OFF) {
        pdOff();
        refused = host::enter();
        if (refused) sfx(Sfx::Deny);
        else sfx(Sfx::Mode);
    }
    go(S_HOST);
}

// Host mode on and off by itself (see above). B pauses it by hand.
static void autoHost() {
    uint32_t now = millis();
    uint8_t s = host::state();
    if (chgame.justPressed(B_BUTTON)) {
        pcMode = !pcMode;
        if (pcMode) {
            host::leave();                      // a sink again: a PC's port powers it, serial back
            pdOff();
            paused = true;
            refused = nullptr;
            sfx(Sfx::Unplug);
            return;
        }
        paused = false;
        freeAt = now - 2000;                    // host mode at once, if the port is free
        sfx(Sfx::Mode);
    }
    if (pcMode) return;
    bool vbusIn = (pwr::reg(pwr::PIN_STAT) & pwr::ST_VBUS_RDY) || (pwr::vbusValid() && pwr::vbusMv() > 4000);
    if (s == host::WAIT_SINK) {
        if (!waitAt) waitAt = now;
        if (now - waitAt > 3000) {              // nothing for 3 s: a look as a sink
            host::leave();
            sinkLook = true;
            lookAt = now;
            waitAt = 0;
        }
        return;
    }
    waitAt = 0;
    if (s != host::OFF) return;
    if (vbusIn) {                               // a PC, a charger, or a powered hub
        if (!pdOn) {
            pd::allowDataSwap(true);
            pd::start();
            pdOn = true;
            pdAt = now;
        }
        if (pd::dfp() && pd::state() == pd::READY) {   // a hub: the host on its power
            const char *e = host::enterPowered(hubLost);
            if (!e) {
                paused = connecting = false;
                refused = nullptr;
                sfx(Sfx::Mode);
                return;
            }
        }
        connecting = !paused && now - pdAt < 1500 && pd::state() != pd::TYPEC;
        if (connecting) {
            sinkLook = false;
            freeAt = now;
            return;
        }
        if (!paused) {
            paused = true;
            refused = nullptr;
            sfx(Sfx::Unplug);
        }
        sinkLook = false;
        freeAt = now;
        return;
    }
    if (pdOn && pd::state() != pd::DETECT && pd::state() != pd::OFF) return;   // (VBUS dips: a hub's swap)
    pdOff();
    connecting = false;
    if (sinkLook && now - lookAt < 1200) return;   // (a source answers Rd within 1 s)
    sinkLook = false;
    if (now - freeAt < 1500) return;
    freeAt = now;
    const char *w = host::enter();          // (the PD sink is off here: pdOff() above)
    refused = paused ? nullptr : w;
    if (!w) {
        if (paused) sfx(Sfx::Mode);
        paused = false;
    }
}

static void type(char ch) {
    if (ch == '\b') {
        if (col) txt[row][--col] = 0;
        else if (row) col = (uint8_t)strlen(txt[--row]);
        return;
    }
    if (ch == '\n' || col >= TC) {
        if (row + 1 < TL) {
            row++;
        } else {
            memmove(txt[0], txt[1], (TL - 1) * (TC + 1));
            memset(txt[TL - 1], 0, TC + 1);
        }
        col = 0;
        if (ch == '\n') return;
    }
    if (ch == '\t') ch = ' ';
    txt[row][col++] = ch;
    txt[row][col] = 0;
}

void tick() {
    uint8_t s = host::state();
    if (pdOn) pd::service();
    autoHost();
    // A: 5 V on anyway, for a USB-C device without its CC resistors (it
    // never shows itself on CC, so host mode would wait for ever).
    if (chgame.justPressed(A_BUTTON) && host::state() == host::WAIT_SINK) {
        waitAt = millis();                      // (no sink look meanwhile)
        if (host::force()) sfx(Sfx::Mode);
        else sfx(Sfx::Deny);
    }
    if (chgame.justPressed(LEFT_BUTTON)) {
        tab = (uint8_t)(tab ? tab - 1 : 3);
        sfx(Sfx::Move);
    }
    if (chgame.justPressed(RIGHT_BUTTON)) {
        tab = (uint8_t)(tab < 3 ? tab + 1 : 0);
        sfx(Sfx::Move);
    }
    if (chgame.justPressed(SELECT_BUTTON)) {        // clear the view
        clearText();
        memset(pings, 0, sizeof pings);
        wheel = 0;
        sfx(Sfx::Knob);
    }
    // The log: UP/DOWN scroll it (it follows the newest at the bottom).
    if (tab == 3) {
        uint8_t n = host::probeCount(), most = n > 9 ? (uint8_t)(n - 9) : 0;
        if (chgame.repeat(UP_BUTTON) && logTop) {
            logTop--;
            logFollow = false;
        }
        if (chgame.repeat(DOWN_BUTTON) && logTop < most) logTop++;
        if (logTop >= most) logFollow = true;
        if (logFollow) logTop = most;
    }

    // Arrivals and departures: a sound, and the view of what arrived. Not
    // while a hub's power swap brings its devices back: they never left, as
    // far as the screen goes.
    bool kb = host::keyboard(), ms = host::mouse(), pad = host::gamepad();
    bool riding = host::reconnecting();
    if (riding) {
        kb |= hadKb;
        ms |= hadMs;
        pad |= hadPad;
    }
    if (riding) {
    } else if ((kb && !hadKb) || (ms && !hadMs) || (pad && !hadPad)) {
        sfx(Sfx::Plug);
        banner("CONNECTED", ON);
        bool here = tab == 0 ? kb : tab == 1 ? ms : tab == 2 ? pad : true;
        if (!here) tab = kb && !hadKb ? 0 : ms && !hadMs ? 1 : 2;
    } else if ((!kb && hadKb) || (!ms && hadMs) || (!pad && hadPad)) {
        sfx(Sfx::Unplug);
    }
    hadKb = kb;
    hadMs = ms;
    hadPad = pad;
    if (s != lastState && s == host::FAILED && !riding) sfx(Sfx::Deny);
    lastState = s;

    uint32_t now = millis();
    if (now - rateAt >= 1000) {
        rate = (uint16_t)(host::reports() - rateBase);
        rateBase = host::reports();
        rateAt = now;
    }

    host::Key k;
    while (host::nextKey(k)) {
        keyCount++;
        lastUsage = k.usage;
        if (k.usage == 0x29) clearText();               // Esc
        else if (k.ch) type(k.ch);
        sfx(Sfx::Key);
    }
    int16_t dx, dy;
    int8_t dw;
    host::takeMouse(dx, dy, dw);
    if (dx || dy) {
        cx = (int16_t)(cx + dx / 2);
        cy = (int16_t)(cy + dy / 2);
        if (cx < 0) cx = 0;
        if (cx > 121) cx = 121;
        if (cy < 0) cy = 0;
        if (cy > 55) cy = 55;
    }
    wheel = (int16_t)(wheel + dw);
    if (wheel > 20) wheel = 20;
    if (wheel < -20) wheel = -20;
    uint8_t b = host::buttons(), pressed = (uint8_t)(b & ~lastBtn);
    if (pressed) {
        sfx(Sfx::Click);
        for (uint8_t i = 0; i < 3; i++)
            if (pressed >> i & 1) {
                Ping &p = pings[i];
                p = Ping{(uint8_t)cx, (uint8_t)cy, 1, (uint8_t)(i == 0 ? LIVE : i == 1 ? AMP : SKY)};
            }
    }
    lastBtn = b;
    for (Ping &p : pings)
        if (p.age && ++p.age > 30) p.age = 0;
    uint8_t pb = (uint8_t)host::pad().buttons;
    if (pb & ~lastPadBtn) sfx(Sfx::Click);
    lastPadBtn = pb;
}

// ---------------------------------------------------------------------------
static const char *stateWord(uint8_t &c) {
    static const char *const W[] = {"WAITING", "WAITING", "POWERING", "STARTING", "READY", "UNKNOWN", "FAILED",
                                    "POWER OFF", "CONNECTING"};
    uint8_t s = host::state();
    c = s == host::READY ? ON : s == host::FAILED || s == host::UNSUPPORTED ? ALERT : LIVE;
    if (connecting) {
        c = LIVE;
        return "CONNECTING";
    }
    if (host::reconnecting()) {
        c = LIVE;
        return "SWITCHING";
    }
    return paused ? "PAUSED" : refused ? "NO HOST" : W[s];
}

static void hint(const char *a, const char *b, const char *c) {
    agent::brackets(4, 26, 120, 60, 5, MID);
    gfx_text(10, 36, a, PALE);
    if (b) gfx_text(10, 50, b, DIM);
    if (c) gfx_text(10, 60, c, DIM);
}

// The force button: 5 V on without the device's CC (A). Lit while forced.
static void forceButton() {
    bool on = host::forcedOn();
    int x = 24, y = 56, w = 80, h = 20;
    gfx_fillRect(x + 1, y + 1, w - 2, h - 2, on ? ALERT : PANEL);
    agent::brackets(x, y, w, h, 4, on ? PALE : LIVE);
    agent::chip(x + 6, y + 6, 9, "A", on ? PALE : LIVE);
    gfx_text(x + 20, y + 6, on ? "5V FORCED" : "FORCE 5V", on ? PALE : LIVE);
}

static void drawKeyboard() {
    agent::brackets(2, 24, 124, 60, 4, MID);
    for (uint8_t r = 0; r < TL; r++) gfx_text(6, 28 + r * 9, txt[r], PALE);
    if (frame & 16) gfx_fillRect(6 + col * 6, 28 + row * 9, 5, 7, LIVE);
    // The modifiers, left or right: bits 0-3 and 4-7.
    static const char *const MODS[4] = {"CTRL", "SHIFT", "ALT", "GUI"};
    uint8_t m = host::mods();
    for (uint8_t i = 0; i < 4; i++) agent::chip(2 + i * 31, 87, 29, MODS[i], (m & (0x11 << i)) ? LIVE : GRID);
    Line l;
    l.s("KEY ").hex(lastUsage);
    l.s("  PRESSES ").d(keyCount);
    tiny(2, 97, l.t, DIM);
}

static void drawMouse() {
    const int fx0 = 2, fy0 = 24;
    agent::brackets(fx0, fy0, 124, 60, 4, MID);
    for (int y = fy0 + 6; y < fy0 + 60; y += 12)
        for (int x = fx0 + 4; x < fx0 + 124; x += 4) gfx_pixel(x, y, GRID);
    for (int x = fx0 + 8; x < fx0 + 124; x += 16)
        for (int y = fy0 + 2; y < fy0 + 58; y += 4) gfx_pixel(x, y, GRID);
    for (const Ping &p : pings)
        if (p.age) gfx_circle(fx0 + 1 + p.x, fy0 + 2 + p.y, p.age / 2 + 2, p.age > 20 ? GRID : p.c);
    int x = fx0 + 1 + cx, y = fy0 + 2 + cy;
    uint8_t c = host::buttons() ? PALE : LIVE;
    gfx_hline(x - 6, y, 4, c);
    gfx_hline(x + 3, y, 4, c);
    gfx_vline(x, y - 6, 4, c);
    gfx_vline(x, y + 3, 4, c);
    gfx_pixel(x, y, PALE);
    // The wheel: a bar on the right edge.
    gfx_vline(122, fy0 + 9, 41, GRID);
    int wy = fy0 + 29 - wheel * 2;
    gfx_fillRect(120, wy - 1, 5, 3, AMP);
    static const char *const BTN[3] = {"LEFT", "MIDDLE", "RIGHT"};
    static const uint8_t ORD[3] = {0, 2, 1};
    uint8_t b = host::buttons();
    for (uint8_t i = 0; i < 3; i++) agent::chip(2 + i * 42, 87, 40, BTN[i], b >> ORD[i] & 1 ? LIVE : GRID);
    Line l;
    l.s("X ").d(cx).s("  Y ").d(cy).s("  WHEEL ").d(wheel);
    tiny(2, 97, l.t, DIM);
}

static bool padBtn(uint8_t i) { return i < 32 && (host::pad().buttons >> i & 1); }

static void drawPad() {
    const host::Pad &p = host::pad();
    // Buttons by place: pads number them 1..12, most with 5/6 the
    // shoulders, 7/8 the triggers, 9/10 select/start, 11/12 the sticks; an
    // Xbox pad's are A B X Y LB RB BACK START LS RS GUIDE.
    bool x = p.xinput;
    uint8_t l1 = 4, r1 = 5, sel = x ? 6 : 8, sta = x ? 7 : 9, ls = x ? 8 : 10, rs = x ? 9 : 11;
    gfx_fillCircle(34, 74, 14, PANEL);
    gfx_fillCircle(94, 74, 14, PANEL);
    gfx_fillRect(20, 40, 88, 36, PANEL);
    agent::brackets(18, 38, 92, 40, 6, GRID);
    // Shoulders and triggers.
    gfx_fillRect(22, 34, 26, 5, padBtn(l1) ? LIVE : GRID);
    gfx_fillRect(80, 34, 26, 5, padBtn(r1) ? LIVE : GRID);
    uint8_t lt = (p.axes & host::AX_LT) ? p.lt : padBtn(6) && !x ? 255 : 0;
    uint8_t rt = (p.axes & host::AX_RT) ? p.rt : padBtn(7) && !x ? 255 : 0;
    bar(22, 27, 26, 4, lt, 255, AMP);
    bar(80, 27, 26, 4, rt, 255, AMP);
    // The d-pad, lit by the hat (diagonals light two arms).
    static const uint8_t ARMS[9] = {1, 1 | 2, 2, 2 | 4, 4, 4 | 8, 8, 8 | 1, 0};   // up right down left
    uint8_t a = ARMS[p.hat > 8 ? 8 : p.hat];
    gfx_fillRect(33, 46, 7, 7, a & 1 ? LIVE : GRID);
    gfx_fillRect(40, 53, 7, 7, a & 2 ? LIVE : GRID);
    gfx_fillRect(33, 60, 7, 7, a & 4 ? LIVE : GRID);
    gfx_fillRect(26, 53, 7, 7, a & 8 ? LIVE : GRID);
    gfx_fillRect(33, 53, 7, 7, GRID);
    // Face buttons: bottom 1, right 2, left 3, top 4 (Xbox: A B X Y).
    static const int8_t FX[4] = {0, 7, -7, 0}, FY[4] = {7, 0, 0, -7};
    for (uint8_t i = 0; i < 4; i++) {
        bool on = padBtn(i);
        gfx_fillCircle(92 + FX[i], 56 + FY[i], 4, on ? LIVE : BG);
        gfx_circle(92 + FX[i], 56 + FY[i], 4, on ? PALE : MID);
        char n[2] = {(char)(x ? "ABXY"[i] : p.nswitch ? "BAYX"[i] : '1' + i), 0};
        tiny(91 + FX[i], 54 + FY[i], n, on ? BG : DIM);
    }
    gfx_fillRect(53, 47, 9, 3, padBtn(sel) ? LIVE : GRID);
    gfx_fillRect(66, 47, 9, 3, padBtn(sta) ? LIVE : GRID);
    // The sticks.
    struct S {
        int x, y;
        int8_t ax, ay;
        uint8_t b;
    };
    S st[2] = {{50, 69, p.x, p.y, ls}, {78, 69, p.rx, p.ry, rs}};
    for (const S &k : st) {
        gfx_fillCircle(k.x, k.y, 8, BG);
        gfx_circle(k.x, k.y, 8, padBtn(k.b) ? LIVE : MID);
        gfx_fillCircle(k.x + k.ax * 5 / 127, k.y + k.ay * 5 / 127, 2, AMP);
    }
    // Every button by number, and the report as sent.
    uint8_t n = p.nButtons > 16 ? 16 : p.nButtons;
    for (uint8_t i = 0; i < n; i++) {
        char s[3] = {(char)('0' + (i + 1) / 10), (char)('0' + (i + 1) % 10), 0};
        agent::chip(i * 8, 86, 7, (i + 1) < 10 ? s + 1 : s, padBtn(i) ? LIVE : GRID);
    }
    uint8_t len;
    const uint8_t *r = host::rawReport(len);
    Line l;
    for (uint8_t i = 0; i < len && i < 10; i++) l.hex(r[i]).c(' ');
    tiny(2, 97, len ? l.t : "NO REPORT YET", DIM);
}

// The log: what the host did with each device (the CHUSBHost library's probe):
// ids, interfaces and endpoints, control transfers and their results, the
// statuses; and each endpoint's polls with the last report's bytes.
static void drawLog() {
    uint8_t n = host::probeCount();
    if (!n) gfx_text(6, 30, "Nothing yet", DIM);
    for (uint8_t i = 0; i < 9 && logTop + i < n; i++) tiny(2, 24 + i * 7, host::probeLine(logTop + i), PALE);
    for (uint8_t k = 0; k < host::probeEps() && k < 2; k++) {
        const host::EpStat &t = host::probeEp(k);
        Line a, b;
        a.s("EP").hex(t.ep).s(" OK ").d((int32_t)t.ok).s(" NAK ").d((int32_t)t.nak).s(" ERR ").d((int32_t)t.err);
        if (t.err) a.c(' ').hex(t.last);
        tiny(2, 87 + k * 7, a.t, AMP);
        (void)b;
    }
}

void draw() {
    uint8_t c;
    const char *w = stateWord(c);
    top(false, w, c);
    agent::readout(rate, LIVE, "RPT", "/S");
    static const char *const TABS[4] = {"KEYS", "MOUSE", "PAD", "LOG"};
    agent::tabs(14, TABS, 4, tab);
    uint8_t s = host::state();
    bool have = tab == 0 ? host::keyboard() : tab == 1 ? host::mouse() : host::gamepad();
    // A hub's power swap: the view stays as it was while its devices come back.
    bool riding = host::reconnecting() && (tab == 0 ? hadKb : tab == 1 ? hadMs : tab == 2 ? hadPad : false);
    if (riding) {
        have = true;
        s = host::READY;
    }
    if (tab == 3) {
        drawLog();
    } else if (connecting || s == host::TRY_SINK) {
        hint("Connecting...", nullptr, nullptr);
    } else if (paused) {
        hint("PC MODE", "Host paused", "Log ready");
    } else if (refused) {
        hint("Unplug the charger", "(host mode runs on", "the battery)");
    } else if (s == host::WAIT_SINK || s == host::SETTLING || s == host::OFF) {
        hint("Plug in a device", nullptr, nullptr);
        forceButton();
    } else if (s == host::POWERING || s == host::WAIT_DEVICE) {
        hint("Powering it...", nullptr, nullptr);
        if (host::forcedOn()) forceButton();
    } else if (s == host::FAILED || s == host::UNSUPPORTED) {
        hint(s == host::FAILED ? "It did not answer" : "Not one I know", "Try another one", nullptr);
    } else if (!have) {
        hint(tab == 0 ? "No keyboard" : tab == 1 ? "No mouse" : "No gamepad", "LEFT/RIGHT: the", "other views");
    } else if (tab == 0) {
        drawKeyboard();
    } else if (tab == 1) {
        drawMouse();
    } else {
        drawPad();
    }
    tiny(2, 104, paused ? "SEND ANY BYTE FOR LOG" : refused ? refused
                 : (s == host::READY && *host::device()) ? host::device() : host::status(), DIM);
    Line pw;
    if (host::poweredByPort() && pd::contractMv()) {
        // On a hub's power: its contract, and the battery charging from it.
        pw.s("HUB POWER ").mv(pd::contractMv()).c(' ').ma(pd::contractMa()).s("  CHARGING");
        tiny(2, 110, pw.t, ON);
    } else {
        pw.s("5V ON IN ").d(host::vbusOnMs()).s("MS  PLUGS ").d(host::attaches()).s("  FAULTS ").d(host::faults());
        tiny(2, 110, pw.t, host::faults() ? GOLD_ : GRID);
    }
    bottom(pcMode ? "B:HOST LR:VIEW SEL:CLEAR" : "B:PC LR:VIEW SEL:CLEAR");
}

}  // namespace hostui
