// The probe log (LOG): what the host does, as lines for a screen or a PC:
// each device's ids and descriptors, every control transfer (but the hub's
// port status, read every 25 ms) with its result, each new status with its
// time, and each endpoint's polls. 48 lines of 32 characters.
#include "Core.h"

namespace host {
using namespace in;

#ifndef CHSIM
static const uint8_t PN = 48;
static char lines[PN][32];
static uint8_t pHead, pN;
static EpStat stats[4];
static uint8_t nStat;

uint8_t probeCount() { return pN; }
const char *probeLine(uint8_t i) { return lines[(pHead + PN - pN + i) % PN]; }
uint8_t probeEps() { return nStat; }
const EpStat &probeEp(uint8_t i) { return stats[i < 4 ? i : 0]; }

static char *pstr(char *q, const char *s) {
    while (*s) *q++ = *s++;
    return q;
}
static char *phex(char *q, uint8_t v) {
    static const char H[] = "0123456789ABCDEF";
    *q++ = H[v >> 4];
    *q++ = H[v & 15];
    *q++ = ' ';
    return q;
}
static void pline(const char *s) {
    char *d = lines[pHead];
    uint8_t i = 0;
    while (s[i] && i < 31) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
    pHead = (uint8_t)((pHead + 1) % PN);
    if (pN < PN) pN++;
}

// "C1 01 0001 0000 14 >00 20": the setup, the result, the bytes; then the
// first bytes that came back.
static void onCtl(const uint8_t *s, uint8_t r, uint16_t got, const uint8_t *data) {
    if (s[0] == 0xA3) return;
    char b[32], *q = b;
    q = phex(phex(q, s[0]), s[1]);
    q = phex(phex(q, s[3]), s[2]) - 1;
    q = phex(phex(q, s[5]), s[4]) - 1;
    q = phex(q, s[6]);
    *q++ = '>';
    q = phex(phex(q, r), (uint8_t)got);
    *q = 0;
    pline(b);
    if (got && data && (s[0] & 0x80)) {
        q = pstr(b, "  ");
        for (uint8_t i = 0; i < got && i < 9; i++) q = phex(q, data[i]);
        *q = 0;
        pline(b);
    }
}
static void onDev(const uint8_t *dd) {
    char b[32], *q = b;
    q = phex(phex(pstr(q, "VID "), dd[9]) - 1, dd[8]);
    q = phex(phex(pstr(q, "PID "), dd[11]) - 1, dd[10]);
    q = phex(phex(phex(pstr(q, "C "), dd[4]), dd[5]), dd[6]);
    *q = 0;
    pline(b);
}
static void onDesc(const uint8_t *x) {
    char b[32], *q = b;
    if (x[1] == 4) q = phex(phex(phex(phex(pstr(q, "IF"), x[2]), x[5]), x[6]), x[7]);
    else q = phex(phex(phex(phex(pstr(q, " EP"), x[2]), x[3]), x[4]), x[6]);
    *q = 0;
    pline(b);
}
static void onPoll(uint8_t dev, const Ep &e, uint8_t s, const uint8_t *r, uint8_t len) {
    uint8_t k = 0;
    while (k < nStat && (stats[k].dev != dev || stats[k].ep != e.num)) k++;
    if (k == nStat && nStat < 4) stats[nStat++] = EpStat{dev, e.num, e.kind, 0, 0, 0, 0, {0}, 0};
    if (k >= 4) return;
    EpStat &t = stats[k];
    if (s == E_OK) {
        t.ok++;
        t.len = (uint8_t)(len > 10 ? 10 : len);
        memcpy(t.data, r, t.len);
    } else if (s == (PID_NAK | E_XFER)) {
        t.nak++;
    } else {
        t.err++;
        t.last = s;
    }
}
// Every new status line, with the time (a status said again is not).
static void onStatus(const char *s) {
    static const char *last;
    if (s == last || !*s) return;
    last = s;
    char b[48], *q = b;
    uint32_t t = millis() / 1000;
    *q++ = (char)('0' + t / 100 % 10);
    *q++ = (char)('0' + t / 10 % 10);
    *q++ = (char)('0' + t % 10);
    q = pstr(q, "S ");
    q = pstr(q, s);
    *q = 0;
    pline(b);
}
// What CC showed when 5 V was forced: 0 under 0.22 V, 1 to 0.66, 2 to 1.23, 3 over.
static void onForced(uint8_t c1, uint8_t c2) {
    char b[32], *q = pstr(b, "5V FORCED CC ");
    *q++ = (char)('0' + c1);
    *q++ = ' ';
    *q++ = (char)('0' + c2);
    *q++ = ' ';
    *q = 0;
    pline(b);
}

static const Probe LOG_PROBE = {onCtl, onDev, onDesc, onPoll, onStatus, onForced};
void useLog() { probe = &LOG_PROBE; }

#else  // CHSIM: made up

static const char *const SIMLOG[] = {"80 06 01 0000 0012 >00 12", "VID 057E PID 2009 C 00 00 00",
                                     "IF00 03 00 00", " EP81 03 40 08", "000S READY"};
static const EpStat simStat = {0, 1, 3, 1200, 34, 0, 0, {0x30, 0x12}, 2};
static bool used;
uint8_t probeCount() { return used && st != OFF ? 5 : 0; }
const char *probeLine(uint8_t i) { return SIMLOG[i < 5 ? i : 0]; }
uint8_t probeEps() { return used && st != OFF ? 1 : 0; }
const EpStat &probeEp(uint8_t) { return simStat; }
void useLog() { used = true; }
#endif

}  // namespace host
