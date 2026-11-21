// CHUSBHost - the Rev 1.5 board as a battery-powered USB host: a keyboard,
// a mouse or a gamepad (HID, Xbox 360 or Switch Pro), on the port directly
// or through a hub, each with a view of its own: a text field, a cursor on
// a grid, a controller. A fourth view is the log of what each device did
// (its ids, its descriptors, every request and its answer), which a PC can
// read over USB serial afterwards.
//
// It builds for the rev1 board (a Rev 1.5 is one) on the CHPower library,
// which checks for the CH211 and the INA226 at start-up; without them host
// mode is refused. Its sister app is CHPDTrigger. README.md says how to use
// it; its look is the agent house the board package's apps share (Agent.h:
// CHStlView, CHSDtoUSB, CHSDtoSerial).
//
//   LEFT / RIGHT    the views: keys, mouse, pad, log
//   B               PC mode: host off, the port a PC's (serial, the log); B again: host
//   UP / DOWN       scroll the log
//   SELECT          clear the view
//   START (3 s)     host power off, then the SD game menu
//
// The files:
//   CHUSBHost.ino     start, the intro, the exit, the frame loop
//   HostUi.*          the views, and host mode's comings and goings
//   Ui.*, Agent.*     the look (Agent.* is the apps' own, unchanged)
//   Sounds.*          the beeps
//   config.h          the version
//
// The CHUSBHost library's example, built for rev1 alone (tools/game.py's
// BOARDS; CHPower stops a build for any other board). It shows every
// driver, so it links them all (host::ALL); a sketch that wants a keyboard
// alone names host::KEYBOARD and pays for no more.
#include <CHGame.h>
#include <CHUSBHost.h>
#include "config.h"
#include "Ui.h"
#include "HostUi.h"

using namespace ui;

static const uint8_t FPS = 30;

// The agent's glass in blue (CHStlView's teals on blue-black), with amber
// for what moves (Ui.h's roles), RGB444.
static const uint16_t PALETTE[16] = {
    0x001, 0xCFF, 0x024, 0x379, 0x5DE, 0x267, 0xF32, 0x013,   // BG PALE GRID MID LIVE DIM ALERT PANEL
    0xFB3, 0x530, 0x5E8, 0x012, 0xFD5, 0x012, 0x8CF, 0xFFF,   // AMP AMPDK ON GHOST GOLD_ SPAN SKY WHITE_
};

static bool powerOk;
static uint16_t startHeld, introT;

// The exit (exitToMenu() turns host power off first: CHPower's
// chgame_beforeExit()).
static void leaveForMenu() {
    gfx_wait();
    chgame.exitToMenu();
}

static void tickIntro() {
    if (++introT > 70 || chgame.justPressed(A_BUTTON) || chgame.justPressed(B_BUTTON) ||
        chgame.justPressed(START_BUTTON)) {
        sfx(Sfx::Ready);
        if (powerOk) hostui::open(0);
        else {
            toast("NO POWER KIT", "NO CH211 / INA226:", "IS THIS A REV 1.5?", ALERT, 600);
            go(S_HOST);
        }
    }
    if (introT > 20 && introT < 38 && (introT & 1)) audio::blip(2600, 4, true);
}
static void drawIntro() {
    int t = introT;
    if (t < 26) agent::sweep(t * 5);
    if (t >= 8) agent::brackets(8, 30, 112, 62, 5, t >= 26 ? LIVE : GRID);
    if (t >= 8) usb(16, 40, t >= 26 ? LIVE : MID, 2);
    if (t >= 20) agent::typed(40, 42, "USB", (t - 20) / 2, LIVE, 2, t < 28 && (t & 4));
    if (t >= 26) agent::typed(40, 62, "HOST", (t - 26) / 2, LIVE, 2, t < 36 && (t & 4));
}

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
    agent::alert(44, "EXIT?", "HOST OFF, THEN MENU", nullptr, ALERT);
    gfx_fillRect(10, 71, 108 * startHeld / (3 * FPS), 2, ALERT);
}

#if defined(USE_CHGAME_USB_CDC) && !CHGAME_DEBUG
// The log as text, for a PC (host mode paused: the port is the PC's).
static void dumpLog() {
    Serial.write("USB HOST LOG\r\n");
    for (uint8_t i = 0; i < host::probeCount(); i++) {
        Serial.write(host::probeLine(i));
        Serial.write("\r\n");
    }
    for (uint8_t k = 0; k < host::probeEps(); k++) {
        const host::EpStat &t = host::probeEp(k);
        Line a, b;
        a.s("EP").hex(t.ep).s(" KIND ").d(t.kind).s(" OK ").d((int32_t)t.ok).s(" NAK ").d((int32_t)t.nak)
            .s(" ERR ").d((int32_t)t.err).s(" LAST ERR ").hex(t.last);
        Serial.write(a.t);
        Serial.write("\r\n  LAST REPORT ");
        for (uint8_t i = 0; i < t.len; i++) b.hex(t.data[i]).c(' ');
        Serial.write(b.t);
        Serial.write("\r\n");
    }
    plog::dump();                         // what PD did with a powered hub
    Serial.write("END\r\n");
}
#endif

void setup() {
    chgame.boot();
    chgame.startExits = false;            // the exit turns host power off first
    dbg::begin("CHUH " CHUH_VERSION);
    powerOk = pwr::begin();               // CN1 inhibited, CC a sink (charging works)
    host::begin(host::ALL | host::PD_SWAP);   // every driver, and PD: a powered hub's power taken over
    host::setTrySink(true);               // a powered hub plugged in gets to be the source (and charges us)
    plog::begin();                        // the PD log, for the PC with the USB log
#if defined(USE_CHGAME_USB_CDC) && !CHGAME_DEBUG
    Serial.begin();
#endif
    gfx_begin(GFX_DIV2, GFX_12BPP);
    pal::init(PALETTE);
    pal::setCycling(false);
    soundsBegin();
    chgame.setFrameRate(FPS);
    go(S_INTRO);
}

void loop() {
    dbg::poll();
    host::service();
    if (!chgame.nextFrame()) return;
    chgame.pollButtons();
    tickExit();
    if (!toastUp() || chgame.justPressed(A_BUTTON) || chgame.justPressed(B_BUTTON)) {
        if (toastUp()) {
            toast(nullptr, nullptr, nullptr, 0, 0);
            chgame.clearButtonState();
        }
        if (screen() == S_INTRO) tickIntro();
        else if (powerOk) hostui::tick();
    }
    pal::tick();
#if defined(USE_CHGAME_USB_CDC) && !CHGAME_DEBUG
    if (host::state() == host::OFF && Serial.available()) {
        while (Serial.available()) Serial.read();
        dumpLog();
    }
#endif
    gfx_wait();
    pal::commit();
    gfx_clear(BG);
    if (screen() == S_INTRO) drawIntro();
    else hostui::draw();
    drawBanner();
    drawToast();
    drawExit();
    frame++;
    gfx_flushAsync();
}
