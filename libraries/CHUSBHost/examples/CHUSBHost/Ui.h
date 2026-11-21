// The look: the agent house (Agent.h, shared with CHStlView and CHSDtoUSB)
// in this app's colours, an amber meter's glass with cyan for current, and
// the pieces every screen uses: the seven-segment readouts at any size, the
// icons, the key hints, a line of text built in pieces, the alert box that
// stays a moment. The screens themselves are in Home.cpp (the start), PdUi
// (the PD trigger and its menu), HostUi (keyboard, mouse, gamepad) and
// Guide (the guided tour).
#pragma once
#include <CHGame.h>
#include "Agent.h"
#include "Sounds.h"

namespace ui {

enum : uint8_t {
    BG = agent::BG, PALE = agent::PALE, GRID = agent::GRID, MID = agent::MID, LIVE = agent::LIVE,
    DIM = agent::DIM, ALERT = agent::ALERT, PANEL = agent::PANEL,
    AMP = 8,        // current
    AMPDK,          // current, dim
    ON,             // the output on, things that worked
    GHOST,          // seven-segment bars that are off
    GOLD_,          // highlights
    SPAN,           // the graph's ground while the output is on
    SKY,           // the gamepad's accents
    WHITE_,
};

// Screens.
enum Screen : uint8_t {
    S_INTRO, S_HOME, S_PD, S_PDMENU, S_OFFER, S_STATUS, S_REGS, S_LOG, S_GUIDES, S_GUIDE, S_HOSTMENU, S_HOST,
};
void go(uint8_t s);             // with the screen's sound left to the caller
uint8_t screen();
extern uint32_t frame;          // frames drawn since start

// A line of text built in pieces (31 characters of the 3x5 font to a line).
struct Line {
    char t[40];
    uint8_t n;
    Line() : n(0) { t[0] = 0; }
    // (Out of line, in Ui.cpp: inlined at the hundred places that build a
    // line, they cost over a kilobyte.)
    Line &s(const char *x);
    Line &c(char ch);
    Line &d(int32_t v);
    // A fixed-point number: v in thousandths, shown with `places` decimals.
    Line &fix(int32_t v, uint8_t places);
    Line &mv(int32_t v);    // 5.08V
    Line &ma(int32_t v);    // 0.31A
    Line &mw(int32_t v);    // 2.81W
    Line &hex(uint8_t v);
};

// A seven-segment digit at scale sc (5sc x 9sc; d < 0: all off), and a
// fixed-point number: v in thousandths, `places` decimals, `digits` in all
// (leading zeros off); returns the x after it. The point is a 1 x sc dot.
void seg(int x, int y, int d, int sc, uint8_t on, uint8_t off = GHOST);
int segNum(int x, int y, int32_t v, int places, int digits, int sc, uint8_t on, uint8_t off = GHOST);

// 7 x 9 icons (at scale sc) for the status bar and the home screen.
void bolt(int x, int y, uint8_t c, int sc = 1);
void usb(int x, int y, uint8_t c, int sc = 1);

// The screen's frame: the status bar with its icon and word, the key hints
// along the bottom under a rule.
void top(bool power, const char *word, uint8_t c);
void bottom(const char *keys);

// A list of rows in the agent style (the selected row on PANEL with LIVE
// brackets), y0 its first row, 10 px a row.
void list(int y0, const char *const *rows, uint8_t n, uint8_t sel, uint8_t first = 0, uint8_t shown = 9);
// UP/DOWN through n rows with the move sound; true when it moved.
bool cursor(uint8_t &c, uint8_t n);

// A word in big outlined letters across the middle for a moment (what the
// other apps' Sizzle banners do, without their flash), drawn through
// drawBanner(); and a thin bar lit to num/den (agent::gauge without its
// 64-bit division).
void banner(const char *text, uint8_t c, uint8_t frames = 45);
void drawBanner();
void bar(int x, int y, int w, int h, uint16_t num, uint16_t den, uint8_t on, uint8_t off = GRID);

// The alert box for `frames` frames over whatever is on the screen (the
// screens draw it last through drawToast()).
void toast(const char *title, const char *l1, const char *l2, uint8_t c, uint16_t frames = 90);
void drawToast();
bool toastUp();

}  // namespace ui
