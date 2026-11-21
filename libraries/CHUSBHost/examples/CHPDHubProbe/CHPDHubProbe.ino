// CHPDHubProbe - a guided bench test for powered USB-C PD hubs on the
// Rev 1.5 board: what a hub does, unpowered and powered, and whether the
// board can take its power (and charge) while it stays the USB host.
//
// The screen walks through four steps; A moves on, B jumps to the end (PC
// mode), SELECT starts again. Through all of them the port runs itself:
//   - Nothing on VBUS: battery host mode (host::enter()), as CHUSBHost.
//   - VBUS from the port (a powered hub, a charger, a PC): the PD sink, with
//     data-role swaps allowed. With a contract it waits for the hub's
//     DR_Swap, then asks for one itself; as the host (DFP) it runs the USB
//     host on the hub's power (host::enterPowered()). If no swap happens it
//     tries the USB host anyway, to see whether the hub answers regardless.
// Everything goes to three logs: the PD log (plog), the USB probe log and
// this app's own timeline (steps, statuses, decisions), which a PC reads
// over USB serial at the end:
//
//   python libraries/CHUSBHost/examples/CHUSBHost/tools/usblog.py --out out/hubprobe.txt
//
// Rev 1.5 only (the rev1 board), on CHPower and CHUSBHost.
#include <CHGame.h>
#include <CHUSBHost.h>

#if !defined(ARDUINO_CHGAME_REV1) && !defined(CHSIM)
#error "CHPDHubProbe is for the Rev 1.5 board: build it for CHGame Rev1 (chgame --board rev1)"
#endif

static const uint8_t FPS = 30;
enum : uint8_t { C_BG, C_PALE, C_GRID, C_MID, C_LIVE, C_DIM, C_ALERT, C_PANEL, C_AMP, C_AMPDK, C_ON, C11, C_GOLD, C13, C_SKY, C_WHITE };
static const uint16_t PALETTE[16] = {
    0x001, 0xCFF, 0x024, 0x379, 0x5DE, 0x267, 0xF32, 0x013,
    0xFB3, 0x530, 0x5E8, 0x012, 0xFD5, 0x012, 0x8CF, 0xFFF,
};

// ---------------------------------------------------------------------------
// The app's timeline: 40 lines with their times
// ---------------------------------------------------------------------------
static const uint8_t AN = 32;
static char alines[AN][36];
static uint8_t aHead, aN;

struct Txt {
    char t[64];
    uint8_t n = 0;
    Txt() { t[0] = 0; }
    Txt &s(const char *x) {
        while (x && *x && n < sizeof t - 1) t[n++] = *x++;
        t[n] = 0;
        return *this;
    }
    Txt &c(char ch) {
        if (n < sizeof t - 1) t[n++] = ch;
        t[n] = 0;
        return *this;
    }
    Txt &d(int32_t v) {
        char b[12];
        fmtInt(b, v);
        return s(b);
    }
    Txt &hex(uint8_t v) {
        static const char H[] = "0123456789ABCDEF";
        return c(H[v >> 4]).c(H[v & 15]);
    }
    Txt &mv(int32_t v) { return d(v / 1000).c('.').d(v / 100 % 10).c('V'); }
    Txt &ma(int32_t v) { return d(v / 1000).c('.').d(v / 100 % 10).c('A'); }
};

static void alog(const Txt &x) {
    Txt l;
    uint32_t ms = millis();
    l.d((int32_t)(ms / 1000)).c('.');
    uint32_t f = ms % 1000;
    if (f < 100) l.c('0');
    if (f < 10) l.c('0');
    l.d((int32_t)f).c(' ').s(x.t);
    char *d = alines[aHead];
    uint8_t i = 0;
    for (; l.t[i] && i < sizeof alines[0] - 1; i++) d[i] = l.t[i];
    d[i] = 0;
    aHead = (uint8_t)((aHead + 1) % AN);
    if (aN < AN) aN++;
}
static void alog(const char *s) { alog(Txt().s(s)); }
static const char *aline(uint8_t i) { return alines[(aHead + AN - aN + i) % AN]; }

// What the CH211 and PA0 say about VBUS, for the timeline.
static void logVbus(const char *why) {
    pwr::readRegs();
    Txt t;
    t.s(why).s(" VBUS ").mv(pwr::vbusMv()).s(" RDY").d(pwr::reg(pwr::PIN_STAT) & pwr::ST_VBUS_RDY ? 1 : 0)
        .s(" EX").d(pwr::reg(pwr::SYS_STAT) & pwr::SYS_VBUS_EXIST ? 1 : 0);
    alog(t);
}
static bool vbusIn() {
    return (pwr::reg(pwr::PIN_STAT) & pwr::ST_VBUS_RDY) || (pwr::reg(pwr::SYS_STAT) & pwr::SYS_VBUS_EXIST);
}

// ---------------------------------------------------------------------------
// The steps
// ---------------------------------------------------------------------------
enum Step : uint8_t { ST_START, ST_UNPOWERED, ST_ADD_POWER, ST_UNPLUG, ST_POWERED, ST_DONE, ST_N };
static const char *const PROMPT[ST_N][4] = {
    {"PD HUB PROBE", "UNPLUG THE PC AND", "THE HUB. A: START", ""},
    {"1/4 PLUG IN THE HUB", "WITHOUT ITS CHARGER,", "A KEYBOARD ON ITS", "USB-A. TYPE. THEN A"},
    {"2/4 NOW PLUG THE", "CHARGER INTO THE HUB", "(HUB STAYS PLUGGED)", "WAIT 5 S, TYPE, A"},
    {"3/4 UNPLUG THE HUB", "FROM THE CH-GAME", "(CHARGER STAYS ON)", "THEN A"},
    {"4/4 PLUG THE HUB IN", "WITH ITS CHARGER ON", "TYPE ON THE KEYBOARD", "THEN A"},
    {"DONE: PLUG INTO PC", "RUN USBLOG.PY", "SELECT: AGAIN", ""},
};
static uint8_t step;

// ---------------------------------------------------------------------------
// The port: battery host, or the PD sink and a host on the hub's power
// ---------------------------------------------------------------------------
enum Mode : uint8_t { M_IDLE, M_HOST, M_PD };
enum Ps : uint8_t { PS_CONTRACT, PS_SWAP, PS_HOST, PS_NOHOST };
static uint8_t mode, ps;
static uint32_t psAt, detectSince;
static bool asked, anyway;
static char typed[22];
static uint8_t typedN;

static bool lost() {
    pd::State s = pd::state();
    if (s != pd::READY && s != pd::TYPEC && s != pd::REQUESTED) return true;
    return !anyway && !pd::dfp();
}

static void toIdle() {
    if (host::state() != host::OFF) host::leave();
    if (pd::state() != pd::OFF) pd::stop();
    mode = M_IDLE;
}
static void toHost() {
    if (pd::state() != pd::OFF) pd::stop();
    const char *e = host::enter();
    if (e) {
        alog(Txt().s("HOST: ").s(e));
        mode = M_IDLE;
        return;
    }
    alog("MODE: BATTERY HOST");
    mode = M_HOST;
}
static void toPd() {
    if (host::state() != host::OFF) host::leave();
    pd::allowDataSwap(true);
    pd::start();
    alog("MODE: PD SINK (SWAPS OK)");
    mode = M_PD;
    ps = PS_CONTRACT;
    psAt = millis();
    detectSince = 0;
    asked = anyway = false;
}

static void logPdo1() {
    uint32_t p = pd::srcPdo(0);
    Txt t;
    t.s("PDO1 ").hex((uint8_t)(p >> 24)).hex((uint8_t)(p >> 16)).hex((uint8_t)(p >> 8)).hex((uint8_t)p);
    t.s(" DRP").d(p & pd::PDO_DRP ? 1 : 0).s(" UNC").d(p & pd::PDO_UNCONSTRAINED ? 1 : 0);
    t.s(" USB").d(p & pd::PDO_USB_COMM ? 1 : 0).s(" DRD").d(p & pd::PDO_DRD ? 1 : 0);
    alog(t);
    alog(Txt().s("CONTRACT ").mv(pd::contractMv()).c(' ').ma(pd::contractMa()).s(" PD").d(pd::specRev()));
}

static void startPoweredHost(const char *why) {
    const char *e = host::enterPowered(lost);
    alog(Txt().s(why).s(e ? ": " : "").s(e));
    ps = e ? PS_NOHOST : PS_HOST;
    psAt = millis();
}

static void servicePd() {
    uint32_t now = millis();
    pd::State s = pd::state();
    // Unplugged (no source for a second): the battery host again.
    if (s == pd::DETECT) {
        if (!detectSince) detectSince = now ? now : 1;
        if (now - detectSince > 1000 && host::state() == host::OFF) {
            alog("NO SOURCE ON CC: UNPLUGGED");
            toHost();
        }
        return;
    }
    detectSince = 0;
    switch (ps) {
    case PS_CONTRACT:
        if (s == pd::READY) {
            logPdo1();
            ps = PS_SWAP;
            psAt = now;
        } else if (s == pd::TYPEC) {
            alog("NO PD OFFER: TYPE-C 5V ONLY");
            anyway = true;
            startPoweredHost("HOST ANYWAY (NO PD)");
        }
        break;
    case PS_SWAP:
        if (pd::dfp()) {
            startPoweredHost(asked ? "WE ASKED, NOW DFP: HOST ON HUB POWER" : "HUB SWAPPED US TO DFP: HOST ON");
        } else if (!asked && now - psAt > 700) {
            asked = true;
            bool ok = pd::requestDataSwap();
            alog(ok ? "NO SWAP FROM HUB: WE SENT DR_SWAP" : "DR_SWAP COULD NOT BE SENT");
            psAt = now;
        } else if (asked && !pd::dataSwapPending() && now - psAt > 400) {
            alog("STILL UFP: TRYING HOST ANYWAY");
            anyway = true;
            startPoweredHost("HOST ANYWAY (UFP)");
        }
        break;
    case PS_HOST:
        if (host::state() == host::OFF) {
            alog(Txt().s("POWERED HOST ENDED: ").s(host::status()));
            ps = PS_NOHOST;
            psAt = now;
        } else if (host::state() == host::WAIT_DEVICE && now - psAt > 4000 && now - psAt < 4100) {
            alog("NO USB DEVICE SEEN IN 4 S");
        }
        break;
    case PS_NOHOST:
        // The contract back (a soft reset, a new offer): look again.
        if (s == pd::READY && now - psAt > 1500) {
            ps = PS_CONTRACT;
            asked = anyway = false;
        }
        break;
    }
}

static void servicePort() {
    if (mode == M_HOST) {
        // A powered hub or a charger: VBUS while we wait for a sink.
        if (host::state() == host::WAIT_SINK && vbusIn()) {
            logVbus("VBUS FROM PORT");
            toPd();
        } else if (host::state() == host::OFF) {
            // Try.SNK found a source (a powered hub): straight to the PD sink.
            pwr::readRegs();
            logVbus(host::status());
            if (vbusIn()) toPd();
            else mode = M_IDLE;
        }
    } else if (mode == M_PD) {
        servicePd();
    } else if (step != ST_START && step != ST_DONE) {
        static uint32_t at;
        if (millis() - at > 1000) {         // try again every second
            at = millis();
            pwr::readRegs();
            if (vbusIn()) toPd();
            else toHost();
        }
    }
}

static void goStep(uint8_t s) {
    step = s;
    plog::add(plog::K_MARK, s);
    alog(Txt().s("=== STEP ").d(s).s(": ").s(PROMPT[s][0]));
    logVbus("  AT STEP");
    typedN = 0;
    typed[0] = 0;
    if (s == ST_DONE) {
        toIdle();
        alog("PC MODE: SERIAL UP");
    } else if (s == ST_UNPOWERED && mode == M_IDLE) {
        pwr::readRegs();
        if (vbusIn()) toPd();
        else toHost();
    }
}

// ---------------------------------------------------------------------------
// The log for a PC
// ---------------------------------------------------------------------------
#if defined(USE_CHGAME_USB_CDC) && !CHGAME_DEBUG
static void dumpAll() {
    Serial.write("PD HUB PROBE LOG\r\n=== APP TIMELINE ===\r\n");
    for (uint8_t i = 0; i < aN; i++) {
        Serial.write(aline(i));
        Serial.write("\r\n");
    }
    Serial.write("=== USB HOST LOG ===\r\n");
    for (uint8_t i = 0; i < host::probeCount(); i++) {
        Serial.write(host::probeLine(i));
        Serial.write("\r\n");
    }
    Serial.write("=== PD LOG ===\r\n");
    plog::dump();
    Serial.write("END\r\n");
}
#endif

// ---------------------------------------------------------------------------
// The screen
// ---------------------------------------------------------------------------
static uint32_t frames;
static uint32_t frame() { return frames; }

static const char *const PDST[] = {"OFF", "NO SOURCE", "WAIT CAPS", "REQUESTED", "READY", "TYPE-C",
                                   "SRC CAPS", "SRC READY", "PR SWAP"};
static const char *const HST[] = {"OFF", "WAIT SINK", "POWERING", "WAIT DEVICE", "READY", "UNSUPPORTED", "FAILED",
                                  "SETTLING", "TRY SINK"};

static void draw() {
    gfx_clear(C_BG);
    gfx_fillRect(0, 0, 128, 11, C_PANEL);
    gfx_text(3, 3, "PD HUB PROBE", C_LIVE);
    for (uint8_t i = 0; i < 4; i++) gfx_text(3, 15 + i * 9, PROMPT[step][i], i ? C_PALE : C_GOLD);
    gfx_hline(0, 52, 128, C_GRID);
    int y = 56;
    gfx_text(3, y, mode == M_HOST ? "BATTERY HOST" : mode == M_PD ? "PD SINK" : "IDLE", mode ? C_ON : C_DIM);
    if (pd::state() != pd::OFF) {
        Txt r;
        r.s(pd::dfp() ? "DFP" : "UFP");
        if (pd::contractMv()) r.c(' ').mv(pd::contractMv()).c(' ').ma(pd::contractMa());
        gfx_text(60, y, r.t, pd::dfp() ? C_ON : C_AMP);
    }
    y += 9;
    if (pd::state() != pd::OFF) gfx_text(3, y, Txt().s("PD ").s(PDST[pd::state() < 9 ? pd::state() : 0]).s(" ").s(pd::event()).t, C_MID);
    y += 9;
    gfx_text(3, y, Txt().s("USB ").s(HST[host::state() < 9 ? host::state() : 0]).t, C_MID);
    y += 9;
    gfx_text(3, y, host::status(), C_PALE);
    y += 9;
    gfx_text(3, y, host::device(), C_SKY);
    y += 9;
    gfx_text(3, y, Txt().s("> ").s(typed).t, (frame() & 16) ? C_WHITE : C_PALE);
    y += 10;
    gfx_hline(0, y - 2, 128, C_GRID);
    uint8_t first = aN > 2 ? (uint8_t)(aN - 2) : 0;
    for (uint8_t i = first; i < aN; i++) gfx_text(3, y + (i - first) * 8, aline(i) + 0, C_DIM);
}

void setup() {
    chgame.boot();
    chgame.startExits = true;
    bool ok = pwr::begin();
    plog::begin();
    host::begin(host::KEYBOARD | host::MOUSE | host::HUB | host::LOG | host::PD_SWAP);
    host::setTrySink(true);               // a powered dual-role hub gets to be the source
#if defined(USE_CHGAME_USB_CDC) && !CHGAME_DEBUG
    Serial.begin();
#endif
    gfx_begin(GFX_DIV2, GFX_12BPP);
    pal::init(PALETTE);
    pal::setCycling(false);
    chgame.setFrameRate(FPS);
    alog(ok ? "START: POWER KIT FOUND" : "START: NO CH211 (NOT A REV 1.5?)");
    step = ST_START;
}

void loop() {
    if (mode == M_PD) pd::service();
    host::service();
    servicePort();
    static const char *lastStat;
    if (host::status() != lastStat && *host::status()) {
        lastStat = host::status();
        alog(Txt().s("USB: ").s(lastStat));
    }
    host::Key k;
    while (host::nextKey(k))
        if (k.ch >= ' ' && typedN < sizeof typed - 1) {
            typed[typedN++] = k.ch;
            typed[typedN] = 0;
        } else if (k.ch == '\b' && typedN) {
            typed[--typedN] = 0;
        }
#if defined(USE_CHGAME_USB_CDC) && !CHGAME_DEBUG
    if (host::state() == host::OFF && Serial.available()) {
        while (Serial.available()) Serial.read();
        dumpAll();
    }
#endif
    if (!chgame.nextFrame()) return;
    chgame.pollButtons();
    if (chgame.justPressed(A_BUTTON) && step < ST_DONE) goStep((uint8_t)(step + 1));
    if (chgame.justPressed(B_BUTTON) && step != ST_DONE) goStep(ST_DONE);
    if (chgame.justPressed(SELECT_BUTTON)) goStep(ST_UNPOWERED);
    pal::tick();
    gfx_wait();
    pal::commit();
    draw();
    frames++;
    gfx_flushAsync();
}
