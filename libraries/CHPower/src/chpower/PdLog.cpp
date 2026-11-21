// The PD log (PdLog.h).
#include <CHGame.h>
#include <string.h>
#include "PdLog.h"

namespace plog {

// A line of text built in pieces (the apps have their own; this is the
// log's, so the library needs none of theirs).
struct Line {
    char t[40];
    uint8_t n = 0;
    Line() { t[0] = 0; }
    Line &s(const char *x) {
        while (*x && n < sizeof t - 1) t[n++] = *x++;
        t[n] = 0;
        return *this;
    }
    Line &c(char ch) {
        if (n < sizeof t - 1) t[n++] = ch;
        t[n] = 0;
        return *this;
    }
    Line &d(int32_t v) {
        char b[12];
        fmtInt(b, v);
        return s(b);
    }
    Line &fix(int32_t v) {          // thousandths, two decimals
        v = (v + 5) / 10;
        d(v / 100).c('.');
        if (v % 100 < 10) c('0');
        return d(v % 100);
    }
    Line &mv(int32_t v) { return fix(v).c('V'); }
    Line &ma(int32_t v) { return fix(v).c('A'); }
    Line &hex(uint8_t v) {
        static const char H[] = "0123456789ABCDEF";
        return c(H[v >> 4]).c(H[v & 15]);
    }
};

static const uint16_t N = 160;
static Ev ring[N];
static uint16_t head, held;
static uint32_t tot;
static volatile bool frz;
static Ev prev[KEEP];
static uint8_t prevN;
static bool due;
static uint32_t keptAt;
static uint8_t keeps;


#ifndef CHSIM
// Interrupts off around the ring, from the loop or the PD interrupt alike:
// MIE cleared, and set again only if it was set. Through gintenr (CSR
// 0x800, QingKe's user-mode view of mstatus.MIE/MPIE, which the core's
// __disable_irq() uses too): the sketch runs in user mode (the startup's
// mret leaves MPP at 0), where touching mstatus is an illegal instruction.
static inline uint32_t irqSave() {
    uint32_t m;
    __asm volatile("csrrci %0, 0x800, 8" : "=r"(m)::"memory");
    return m;
}
static inline void irqRestore(uint32_t m) {
    if (m & 8) __asm volatile("csrsi 0x800, 8" ::: "memory");
}
static inline void stamp(Ev &e) {
    uint32_t per = (uint32_t)SysTick->CMP + 1;
    uint32_t ms = millis(), cnt = (uint32_t)SysTick->CNT;
    uint32_t ms2 = millis();
    if (ms2 != ms) {
        ms = ms2;
        cnt = (uint32_t)SysTick->CNT;
    }
    uint32_t us = cnt / (per / 1000u);
    e.ms = ms;
    e.us = (uint16_t)(us > 999 ? 999 : us);
}
#else
static inline uint32_t irqSave() { return 0; }
static inline void irqRestore(uint32_t) {}
static inline void stamp(Ev &e) {
    e.ms = millis();
    e.us = 0;
}
#endif

static void record(uint8_t kind, uint8_t a, uint16_t b, uint16_t c) {
    if (kind == K_KEEP) {
        due = true;
        return;
    }
    if (frz) return;
    Ev e;
    stamp(e);
    e.kind = kind;
    e.a = a;
    e.b = b;
    e.c = c;
    uint32_t m = irqSave();
    ring[head] = e;
    head = (uint16_t)(head + 1 < N ? head + 1 : 0);
    if (held < N) held++;
    tot++;
    irqRestore(m);
}
void begin() { hook = record; }

uint16_t count(bool last) { return last ? prevN : held; }
const Ev &at(bool last, uint16_t i) {
    if (last) return prev[i < prevN ? i : 0];
    uint16_t first = (uint16_t)((head + N - held) % N);
    return ring[(first + i) % N];
}
uint32_t total() { return tot; }
bool frozen() { return frz; }
void setFrozen(bool f) { frz = f; }
void clear() {
    uint32_t m = irqSave();
    head = held = 0;
    tot = 0;
    irqRestore(m);
}

// ---------------------------------------------------------------------------
// As text
// ---------------------------------------------------------------------------
// The messages a sink sees most by name; the rest by number.
static void msgName(Line &l, uint8_t a) {
    static const uint8_t ID[] = {0x01, 0x03, 0x04, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                 0x0D, 0x10, 0x81, 0x82, 0x84, 0x86, 0x8F};
    static const char *const NAME[] = {"GOODCRC",  "ACCEPT",    "REJECT",   "PS_RDY",   "GET SRC CAP", "GET SNK CAP",
                                       "DR_SWAP",  "PR_SWAP",   "VCONN_SWAP", "WAIT",   "SOFT RESET",  "NOT SUPP",
                                       "SRC CAPS", "REQUEST",   "SNK CAPS", "ALERT",    "VDM"};
    uint8_t k = (uint8_t)(a & 0x9F);
    for (uint8_t i = 0; i < sizeof ID; i++)
        if (ID[i] == k) {
            l.s(NAME[i]);
            return;
        }
    l.s(a & 0x80 ? "DATA " : "CTRL ").d(a & 0x1F);
}
static const char *const STATES[] = {"OFF",         "NO SOURCE", "WAITING",   "REQUESTED", "READY",
                                     "TYPE-C ONLY", "SRC CAPS",  "SRC READY", "PR SWAP"};

void describe(const Ev &e, char *out) {
    Line l;
    switch (e.kind) {
    case K_START:
        l.s("START VBUS ").mv(e.b).s(" VSYS ").d(e.a & 1);
        break;
    case K_STATE:
        l.s("> ").s(e.a < 9 ? STATES[e.a] : "?");
        break;
    case K_SOURCE:
        l.s("SOURCE CC").d(e.a).s(" TYPE-C ").ma(e.b);
        break;
    case K_CC:
        l.s("CC_CTRL ").hex(e.a).s(e.b ? "" : " FAILED");
        break;
    case K_RX:
        l.s("RX ");
        msgName(l, e.a);
        if (e.a & 0x80) l.s(" X").d(e.b >> 12 & 7);
        l.s(" ID").d(e.b >> 9 & 7);
        if ((e.b >> 6 & 3) == 2) l.s(" R3");
        break;
    case K_TX:
        l.s("TX ");
        msgName(l, e.a);
        if ((e.a & 0x9F) == 0x82) l.s(" PDO").d(e.c);
        if (!e.b) l.s(" NO GOODCRC");
        else if (e.b > 1) l.s(" TRY ").d(e.b);
        break;
    case K_OTHER:
        l.s("RX OTHER N").d(e.a);
        break;
    case K_HARD_IN:
        l.s("HARD RESET IN");
        break;
    case K_HARD_OUT:
        l.s("HARD RESET SENT");
        break;
    case K_PDO:
        l.s("PDO").d(e.a & 0x7F).c(' ');
        if (e.a & 0x80) l.s("PPS TO ");
        if (e.b) l.mv(e.b).c(' ').ma(e.c);
        else l.s("OTHER KIND");
        break;
    case K_VBUS:
        l.s("VBUS ").mv(e.b);
        break;
    case K_STAT:
        l.s("RDY").d(e.a >> 4 & 1).s(" EX").d(e.a >> 1 & 1).s(" VSYS").d(e.a & 1).s(" HVLI").d(e.b >> 2 & 1);
        if (e.a & 0x40) l.s(" OT");
        break;
    case K_NOTE:
        l.s(noteText(e.a));
        break;
    case K_CN1:
        l.s(e.a ? "CN1 OFF" : "CN1 WOULD NOT TURN OFF");
        break;
    case K_RAW:
        l.s("PDO").d(e.a).s(" RAW ").hex((uint8_t)(e.c >> 8)).hex((uint8_t)e.c).hex((uint8_t)(e.b >> 8))
            .hex((uint8_t)e.b);
        break;
    case K_MARK:
        l.s("=== STEP ").d(e.a).s(" ===");
        break;
    case K_HOST: {
        static const char *const WHY[] = {"?", "CC GONE", "VBUS FELL", "OVER-TEMP", "SWAP FAILED"};
        l.s("5V OFF: ").s(e.a < 5 ? WHY[e.a] : "?").s(" CC").d(e.b >> 4).c('/').d(e.b & 15);
        if (e.c) l.c(' ').d(e.c).s("MS");
        break;
    }
    default:
        l.s("?").d(e.kind);
        break;
    }
    l.t[24] = 0;
    strcpy(out, l.t);
}

void timeText(const Ev &e, char *out, bool micro) {
    Line l;
    l.d((int32_t)(e.ms / 1000)).c('.');
    uint32_t f = e.ms % 1000;
    if (f < 100) l.c('0');
    if (f < 10) l.c('0');
    l.d((int32_t)f);
    if (micro) {
        if (e.us < 100) l.c('0');
        if (e.us < 10) l.c('0');
        l.d(e.us);
    }
    strcpy(out, l.t);
}

// ---------------------------------------------------------------------------
// The last run, kept by the app
// ---------------------------------------------------------------------------
bool keepDue() { return due && keeps < 40 && (!keeps || millis() - keptAt >= 1500); }
uint8_t tail(Ev *out) {
    uint16_t n = held < KEEP ? held : KEEP;
    for (uint16_t i = 0; i < n; i++) out[i] = at(false, (uint16_t)(held - n + i));
    due = false;
    keptAt = millis();
    keeps++;
    return (uint8_t)n;
}
void setLastRun(const Ev *in, uint8_t n) {
    prevN = n > KEEP ? 0 : n;
    memcpy(prev, in, prevN * sizeof(Ev));
}

// ---------------------------------------------------------------------------
// Serial: as text on the port in a release build, through the debug
// protocol's D command in a debug build.
// ---------------------------------------------------------------------------
#if CHGAME_DEBUG
static void out(const char *s) { dbg::print(s); }
#define CHPOWER_LOG_OUT 1
#elif defined(USE_CHGAME_USB_CDC)
static void out(const char *s) { Serial.write(s); }
#define CHPOWER_LOG_OUT 1
#endif

#ifdef CHPOWER_LOG_OUT
static void dumpRun(bool last) {
    uint16_t n = count(last);
    out(last ? "PD LOG: THE LAST RUN, " : "PD LOG: THIS RUN, ");
    Line h;
    h.d(n).s(" EVENTS");
    if (!last) h.s(" (").d((int32_t)tot).s(" IN ALL)");
    out(h.t);
    out("\r\n");
    for (uint16_t i = 0; i < n; i++) {
        const Ev &e = at(last, i);
        char t[16], d[32];
        timeText(e, t, true);
        describe(e, d);
        out(t);
        out("  ");
        out(d);
        Line raw;
        raw.s("   [").d(e.kind).c(' ').hex(e.a).c(' ').hex((uint8_t)(e.b >> 8)).hex((uint8_t)e.b).c(' ')
            .hex((uint8_t)(e.c >> 8)).hex((uint8_t)e.c).c(']');
        out(raw.t);
        out("\r\n");
    }
}
void dump() {
#if !CHGAME_DEBUG
    if (!Serial) return;
#endif
    dumpRun(true);
    dumpRun(false);
}
#else
void dump() {}
#endif

}  // namespace plog
