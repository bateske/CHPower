// The pieces every screen uses (Ui.h).
#pragma GCC optimize("Os")
#include "Ui.h"

namespace ui {

uint32_t frame;

#define NOINL __attribute__((noinline))
NOINL Line &Line::s(const char *x) {
    while (*x && n < sizeof t - 1) t[n++] = *x++;
    t[n] = 0;
    return *this;
}
NOINL Line &Line::c(char ch) {
    if (n < sizeof t - 1) t[n++] = ch;
    t[n] = 0;
    return *this;
}
NOINL Line &Line::d(int32_t v) {
    char b[12];
    fmtInt(b, v);
    return s(b);
}
NOINL Line &Line::fix(int32_t v, uint8_t places) {
    if (v < 0) {
        c('-');
        v = -v;
    }
    int32_t div = places == 1 ? 100 : places == 2 ? 10 : 1;
    v = (v + div / 2) / div;
    int32_t unit = places == 1 ? 10 : places == 2 ? 100 : 1000;
    d(v / unit).c('.');
    int32_t f = v % unit;
    if (places >= 3 && f < 100) c('0');
    if (places >= 2 && f < 10) c('0');
    return d(f);
}
NOINL Line &Line::mv(int32_t v) { return fix(v, 2).c('V'); }
NOINL Line &Line::ma(int32_t v) { return fix(v, 2).c('A'); }
NOINL Line &Line::mw(int32_t v) { return fix(v, 2).c('W'); }
NOINL Line &Line::hex(uint8_t v) {
    static const char H[] = "0123456789ABCDEF";
    return c(H[v >> 4]).c(H[v & 15]);
}
static uint8_t cur = S_INTRO;
void go(uint8_t s) {
    cur = s;
    chgame.clearButtonState();
}
uint8_t screen() { return cur; }

void seg(int x, int y, int d, int sc, uint8_t on, uint8_t off) {
    static const uint8_t SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    uint8_t m = d < 0 ? 0 : SEG[d];
    int w = 5 * sc, h = 9 * sc, t = sc;
    int mid = y + (h - t) / 2;
    int hl = h / 2 - t;
    gfx_fillRect(x + t, y, w - 2 * t, t, m & 0x01 ? on : off);                // a
    gfx_fillRect(x + w - t, y + t, t, hl, m & 0x02 ? on : off);               // b
    gfx_fillRect(x + w - t, mid + t, t, hl, m & 0x04 ? on : off);             // c
    gfx_fillRect(x + t, y + h - t, w - 2 * t, t, m & 0x08 ? on : off);        // d
    gfx_fillRect(x, mid + t, t, hl, m & 0x10 ? on : off);                     // e
    gfx_fillRect(x, y + t, t, hl, m & 0x20 ? on : off);                       // f
    gfx_fillRect(x + t, mid, w - 2 * t, t, m & 0x40 ? on : off);              // g
}

int segNum(int x, int y, int32_t v, int places, int digits, int sc, uint8_t on, uint8_t off) {
    if (v < 0) v = 0;
    int32_t div = places == 0 ? 1000 : places == 1 ? 100 : places == 2 ? 10 : 1;
    uint32_t n = (uint32_t)((v + div / 2) / div);
    uint32_t p = 1;
    for (int i = 1; i < digits; i++) p *= 10;
    int lead = digits - places;                     // digits before the point
    for (int i = 0; i < digits; i++, p /= 10) {
        bool show = n >= p || i >= lead - 1;
        seg(x, y, show ? (int)(n / p % 10) : -1, sc, on, off);
        x += 6 * sc;
        if (places && i == lead - 1) {
            gfx_fillRect(x - sc / 2 - 1, y + 9 * sc - sc, sc, sc, on);
            x += sc;
        }
    }
    return x;
}

static const uint8_t BOLT[9] = {0x0C, 0x18, 0x18, 0x30, 0x7E, 0x0C, 0x18, 0x10, 0x20};
static const uint8_t USB[9] = {0x08, 0x1C, 0x08, 0x4B, 0x4B, 0x49, 0x2A, 0x1C, 0x08};
static void icon(int x, int y, const uint8_t *bits, int sc, uint8_t c) {
    for (int r = 0; r < 9; r++)
        for (int b = 0; b < 7; b++)
            if (bits[r] >> (6 - b) & 1) gfx_fillRect(x + b * sc, y + r * sc, sc, sc, c);
}
void bolt(int x, int y, uint8_t c, int sc) { icon(x, y, BOLT, sc, c); }
void usb(int x, int y, uint8_t c, int sc) { icon(x, y, USB, sc, c); }

void top(bool power, const char *word, uint8_t c) {
    agent::statusBar(word, c);
    if (power) bolt(3, 1, c);
    else usb(3, 1, c);
}
void bottom(const char *keys) {
    gfx_hline(0, 116, 128, GRID);
    agent::keys(120, keys);
}

void list(int y0, const char *const *rows, uint8_t n, uint8_t sel, uint8_t first, uint8_t shown) {
    for (uint8_t i = first; i < n && i < first + shown; i++) {
        int y = y0 + (i - first) * 10;
        if (i == sel) {
            gfx_fillRect(2, y - 1, 124, 9, PANEL);
            agent::brackets(1, y - 2, 126, 11, 3, LIVE);
        }
        gfx_text(6, y, rows[i], i == sel ? PALE : DIM);
    }
}
bool cursor(uint8_t &c, uint8_t n) {
    uint8_t was = c;
    if (n && chgame.repeat(UP_BUTTON)) c = c ? (uint8_t)(c - 1) : (uint8_t)(n - 1);
    if (n && chgame.repeat(DOWN_BUTTON)) c = (uint8_t)(c + 1 < n ? c + 1 : 0);
    if (c != was) sfx(Sfx::Move);
    return c != was;
}

static const char *bText;
static uint8_t bCol, bLeft, bAll;
void banner(const char *text, uint8_t c, uint8_t frames) {
    bText = text;
    bCol = c;
    bLeft = bAll = frames;
}
void drawBanner() {
    if (!bLeft) return;
    uint8_t age = (uint8_t)(bAll - bLeft--);
    if (bLeft < 10 && (bLeft & 2)) return;              // blinking out
    int y = 52 - (age < 4 ? (4 - age) * 2 : 0);         // dropping in
    gfx_fillRect(0, y - 4, 128, 22, BG);
    gfx_hline(0, y - 5, 128, bCol);
    gfx_hline(0, y + 18, 128, bCol);
    agent::outlined(y, bText, bCol, PANEL);
}
void bar(int x, int y, int w, int h, uint16_t num, uint16_t den, uint8_t on, uint8_t off) {
    gfx_fillRect(x, y, w, h, off);
    int lit = den ? (int)((uint32_t)w * (num > den ? den : num) / den) : 0;
    if (num && !lit) lit = 1;
    if (lit) gfx_fillRect(x, y, lit, h, on);
}

static const char *tTitle, *tL1, *tL2;
static uint8_t tCol;
static uint16_t tLeft;
void toast(const char *title, const char *l1, const char *l2, uint8_t c, uint16_t frames) {
    tTitle = title;
    tL1 = l1;
    tL2 = l2;
    tCol = c;
    tLeft = frames;
}
bool toastUp() { return tLeft != 0; }
void drawToast() {
    if (!tLeft) return;
    tLeft--;
    agent::alert(44, tTitle, tL1, tL2, tCol);
}

}  // namespace ui
