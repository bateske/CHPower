// CHPDTrigger - the Rev 1.5 board's USB-PD trigger: a USB-C charger as a
// bench supply (its fixed voltages, and any voltage in a PPS range, 20 mV at
// a time) on the protected output CN1, with a meter, a scope, a current
// limit that cuts the output off, a guided tour, and the diagnostics behind
// it (the CH211's registers, the PD log).
//
// It builds for the rev1 board (a Rev 1.5 is one) on the CHPower library,
// which checks for the CH211 and the INA226 at start-up; without them every
// power feature is refused. Its sister app is CHUSBHost (battery-powered USB
// host). README.md says how to use it; its look is the agent house the
// board package's apps share (Agent.h: CHStlView, CHSDtoUSB, CHSDtoSerial).
//
//   UP / DOWN       turn the bracketed knob (hold for big steps); pick
//   LEFT / RIGHT    the other knob
//   A               the output on, off, or on again after a cut-off; open
//   B               back
//   SELECT          the menu
//   START (3 s)     the output off, then the SD game menu
//
// The files:
//   CHPDTrigger.ino   start, the intro, the exit, the frame loop
//   PdUi.*            the trigger and its menu
//   Guide.*           the guided tour
//   Ui.*, Agent.*     the look (Agent.* is the apps' own, unchanged)
//   Sounds.*          the beeps
//   Saved.*           the settings and the PD log's last events in flash
//   config.h          the version
//
// The CHPower library's example, built for rev1 alone (tools/game.py's
// BOARDS; CHPower stops a build for any other board).
#include <CHGame.h>
#include <CHPower.h>
#include "config.h"
#include "Ui.h"
#include "PdUi.h"
#include "Guide.h"
#include "Saved.h"

using namespace ui;
using agent::tiny;

static const uint8_t FPS = 30;

// Amber glass with cyan for current (Ui.h's roles), RGB444.
static const uint16_t PALETTE[16] = {
    0x100, 0xFEC, 0x431, 0x963, 0xFB3, 0x986, 0xF33, 0x210,   // BG PALE GRID MID LIVE DIM ALERT PANEL
    0x3DF, 0x157, 0x5E5, 0x311, 0xFD5, 0x220, 0x48F, 0xFFF,   // AMP AMPDK ON GHOST GOLD_ SPAN SKY WHITE_
};

static bool powerOk;
static uint16_t startHeld, introT;
static uint32_t savedAt;

// The exit: everything off first.
static void leaveForMenu() {
    pwr::safeDown();
    gfx_wait();
    saved::keepLog();                     // the PD log's last events, and the settings
    chgame.exitToMenu();
}

// ---------------------------------------------------------------------------
// The intro
// ---------------------------------------------------------------------------
static void tickIntro() {
    if (++introT > 70 || chgame.justPressed(A_BUTTON) || chgame.justPressed(B_BUTTON) ||
        chgame.justPressed(START_BUTTON)) {
        sfx(Sfx::Ready);
        if (!powerOk) toast("NO POWER KIT", "NO CH211 / INA226:", "IS THIS A REV 1.5?", ALERT, 600);
        go(S_PD);
    }
    if (introT > 20 && introT < 38 && (introT & 1)) audio::blip(2600, 4, true);
}
static void drawIntro() {
    int t = introT;
    if (t < 26) agent::sweep(t * 5);
    if (t >= 8) agent::brackets(8, 30, 112, 62, 5, t >= 26 ? LIVE : GRID);
    if (t >= 8) bolt(16, 40, t >= 26 ? LIVE : MID, 2);
    if (t >= 20) agent::typed(40, 42, "PD", (t - 20) / 2, LIVE, 2, t < 26 && (t & 4));
    if (t >= 26) agent::typed(40, 62, "TRIGGER", (t - 26) / 2, LIVE, 2, t < 40 && (t & 4));
}

// ---------------------------------------------------------------------------
// START held: the exit, with its progress under the alert
// ---------------------------------------------------------------------------
static void tickExit() {
    if (chgame.pressed(START_BUTTON)) {
        startHeld++;
        if (startHeld % 21 == 0) audio::note(900, 45, 7);
        if (startHeld >= 3 * FPS) leaveForMenu();
    } else {
        startHeld = 0;
    }
}
static void drawExit() {
    if (startHeld < FPS / 2) return;
    agent::alert(44, "EXIT?", "OUTPUT OFF, THEN MENU", nullptr, ALERT);
    gfx_fillRect(10, 71, 108 * startHeld / (3 * FPS), 2, ALERT);
}

// The debug build's own command: D, the PD log as text (what any byte does
// on a release build's serial port).
#if CHGAME_DEBUG
static bool debugHook(char cmd, const char *) {
    if (cmd != 'D') return false;
    plog::dump();
    return true;
}
#endif

// ---------------------------------------------------------------------------
void setup() {
    chgame.boot();
    chgame.startExits = false;            // the exit turns the output off first
    dbg::begin("CHPD " CHPD_VERSION);
#if CHGAME_DEBUG
    dbg::hook = debugHook;
#endif
    // The power section before anything else: CN1 inhibited, CC a sink.
    powerOk = pwr::begin();
    plog::begin();                        // the PD log records from here (its PD LOG screen)
#if defined(USE_CHGAME_USB_CDC) && !CHGAME_DEBUG
    Serial.begin();
#endif
    gfx_begin(GFX_DIV2, GFX_12BPP);
    pal::init(PALETTE);
    pal::setCycling(false);
    soundsBegin();
    chgame.setFrameRate(FPS);
    saved::begin();                       // (the last run's PD log to plog::)
    trig::begin();
    if (powerOk) {
        delay(25);                        // PA0 settles as an analog input
        plog::add(plog::K_START, pwr::reg(pwr::SYS_STAT), pwr::vbusMv(), pwr::reg(pwr::PIN_STAT));
        pd::start();
    }
    go(S_INTRO);
}

void loop() {
    dbg::poll();
    pd::service();                        // GoodCRC and replies are the interrupt's; this is the rest
    if (!chgame.nextFrame()) return;
    chgame.pollButtons();
    if (powerOk) pwr::poll();
    trig::service();
    tickExit();
    if (!toastUp() || chgame.justPressed(A_BUTTON) || chgame.justPressed(B_BUTTON)) {
        if (toastUp()) {
            toast(nullptr, nullptr, nullptr, 0, 0);     // a press dismisses the box
            chgame.clearButtonState();
        }
        switch (screen()) {
        case S_INTRO:    tickIntro(); break;
        case S_GUIDES:
        case S_GUIDE:    guide::tick(); break;
        default:         pdui::tick(); break;
        }
    }
    pal::tick();

#if defined(USE_CHGAME_USB_CDC) && !CHGAME_DEBUG
    // Any byte from a PC: the PD log (the last run's, then this run's) as text.
    if (Serial.available()) {
        while (Serial.available()) Serial.read();
        plog::dump();
    }
#endif

    gfx_wait();
    uint32_t now = millis();
    if (plog::keepDue() && pd::quiet()) {
        saved::keepLog();
        savedAt = now;
    } else if (saved::dirty() && pd::quiet() && now - savedAt > 3000) {
        saved::save();
        savedAt = now;
    }
    pal::commit();
    gfx_clear(BG);
    switch (screen()) {
    case S_INTRO:    drawIntro(); break;
    case S_GUIDES:
    case S_GUIDE:    guide::draw(); break;
    default:         pdui::draw(); break;
    }
    drawBanner();
    drawToast();
    drawExit();
    frame++;
    gfx_flushAsync();
}
