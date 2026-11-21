// The USB-PD sink (Pd.h).
//
// The PHY and message handling are WCH's USBPD_SNK example (PD_Process.c,
// V1.0.1, (c) 2023 Nanjing Qinheng Microelectronics, "used for
// microcontroller manufactured by Nanjing Qinheng Microelectronics": this
// runs on their CH32X035). What changed is listed in Pd.h.
//
// Who does what. The interrupt answers the source: the GoodCRC within
// 195 us, then the reply the message needs (a Request for an offer, Accept
// for a soft reset, and so on) at once, since a source waits no more than
// about 25 ms for it and a frame of this sketch can take longer. It does
// no I2C. service(), from the loop, does what needs the I2C or the ADC (CN1
// off, CC_CTRL, VBUS), detection and the timeouts, with the PD interrupt
// held off only around its own short sections.
#include <Arduino.h>
#include "Pd.h"
#include "PdLog.h"
#include "Power.h"
#include "Internal.h"
#include "Hw.h"

namespace pd {

static volatile State st = OFF;
static uint8_t line;                    // 1 or 2
static uint16_t tcMa;
static uint32_t caps[7];                // the source's PDOs as sent
static Pdo pdos[7];
static volatile uint8_t npdo, activePos, reqPos;
static volatile uint16_t cMv, cMa, wantMv = 5000;
static volatile uint32_t t0, changed;
static uint32_t lastVbusAt, hardAt, txSince, detAt;
static uint8_t detN;
static volatile bool accepted;
static bool softSent;
static volatile uint8_t evId;
static volatile uint16_t nHard, nSoft, nReject;
static volatile uint8_t hardSeen, capsIn;   // for service(): CN1 off (the I2C is the loop's)
static uint16_t loggedMv;
// PD 3.0: the Specification Revision field (1 = 2.0, 2 = 3.0), taken from
// the source's offer; PPS needs 3.0.
static volatile uint8_t rev = 1;
// PPS: the voltage and current asked for, the last request (for its 8 s
// repeat) and the asked-for values while a request is in flight.
static volatile uint16_t ppsMv, ppsMa, reqMv, reqMa;
static uint32_t ppsAt;
static volatile uint8_t wantPps;        // the PPS PDO asked for last (0: none): asked again after a new offer
// While a PPS request repeats itself (every 8 s), its messages are not
// logged: they would fill the log with the same lines.
static volatile bool hush;
// Data roles: dfp is ours now (the header's Port Data Role bit), swapOk
// whether a partner's DR_Swap is accepted, drAt when ours went out (0: none
// in flight).
static volatile bool dfp_, swapOk;
static volatile uint32_t drAt;
// The source side: srcRole is ours now (the header's Port Power Role bit);
// prOk whether a partner's PR_Swap is accepted; swapPh its phase
// (swapPhase()), swapAt when that phase began; partnerRdy the partner's
// PS_RDY at its end.
static volatile bool srcRole, prOk, partnerRdy;
static volatile uint8_t swapPh, capsN;
static volatile uint32_t swapAt, capsAt;

State state() { return st; }
uint8_t cc() { return st == OFF || st == DETECT ? 0 : line; }
uint16_t typecMa() { return tcMa; }
uint8_t pdoCount() { return npdo; }
const Pdo &pdo(uint8_t i) { return pdos[i < 7 ? i : 6]; }
// A contract stays in force while a new request is answered.
static bool inForce() { return st == READY || st == TYPEC || (st == REQUESTED && activePos); }
uint8_t active() { return st == READY || st == REQUESTED ? activePos : 0; }
uint16_t contractMv() { return inForce() ? cMv : 0; }
uint16_t contractMa() { return inForce() ? cMa : 0; }
bool busy() { return st == REQUESTED || st == WAIT_CAPS; }
bool quiet() {
    // A flash write stops the CPU, the PD interrupt with it: only while the
    // source has nothing to send (or is recovering from a hard reset).
    return st == OFF || st == DETECT || st == TYPEC || st == READY ||
           (st == WAIT_CAPS && hardAt && millis() - hardAt < 400);
}
uint32_t changedAt() { return changed; }
const char *event() { return plog::noteText(evId); }
uint16_t hardResets() { return nHard; }
uint16_t softResets() { return nSoft; }
uint16_t rejects() { return nReject; }
uint8_t specRev() { return rev >= 2 ? 3 : 2; }
void allowDataSwap(bool on) { swapOk = on; }
void allowPowerSwap(bool on) { prOk = on; }
uint8_t swapPhase() { return swapPh; }
bool dataSwapPending() { return drAt != 0; }
bool dfp() { return dfp_; }
uint32_t srcPdo(uint8_t i) { return i < npdo ? caps[i] : 0; }
bool ppsActive() { return active() && pdos[activePos - 1].kind == PDO_PPS; }

static void say(uint8_t n) {
    evId = n;
    plog::note(n);
}
static void setState(State s) {
    if (s == st) return;
    st = s;
    if (!hush) plog::add(plog::K_STATE, s);
}

// The contract, told to the power section too (CN1's budget and VBUS check
// go by it, whatever the app does).
static void setContract(uint16_t mv, uint16_t ma) {
    if (mv != cMv || ma != cMa) changed = millis();
    cMv = mv;
    cMa = ma;
    pwr::onContract(mv, ma);
}

// The offer kept, and logged when it differs from the one before.
static void savePdos(const uint8_t *b, uint8_t n) {
    if (n > 7) n = 7;
    bool same = n == npdo;
    for (uint8_t i = 0; i < n; i++) {
        uint32_t p = b[4 * i] | (uint32_t)b[4 * i + 1] << 8 | (uint32_t)b[4 * i + 2] << 16 |
                     (uint32_t)b[4 * i + 3] << 24;
        if (caps[i] != p) same = false;
        caps[i] = p;
        Pdo &d = pdos[i];
        if ((p >> 30) == 0) {                   // fixed: 50 mV and 10 mA units
            d.kind = PDO_FIXED;
            d.mv = d.minMv = d.maxMv = (uint16_t)(((p >> 10) & 0x3FF) * 50);
            d.ma = (uint16_t)((p & 0x3FF) * 10);
        } else if ((p >> 28) == 0xC) {          // an augmented PDO, SPR PPS: 100 mV and 50 mA units
            d.kind = PDO_PPS;
            d.mv = 0;
            d.maxMv = (uint16_t)(((p >> 17) & 0xFF) * 100);
            d.minMv = (uint16_t)(((p >> 8) & 0xFF) * 100);
            d.ma = (uint16_t)((p & 0x7F) * 50);
        } else {
            d.kind = PDO_OTHER;
            d.mv = d.minMv = d.maxMv = d.ma = 0;
        }
    }
    npdo = n;
    if (!same)
        for (uint8_t i = 0; i < n; i++) {
            const Pdo &d = pdos[i];
            plog::add(plog::K_RAW, (uint8_t)(i + 1), (uint16_t)caps[i], (uint16_t)(caps[i] >> 16));
            // PPS logged with its top voltage and 0x80 in the position.
            plog::add(plog::K_PDO, (uint8_t)((i + 1) | (d.kind == PDO_PPS ? 0x80 : 0)), d.maxMv, d.ma);
        }
}

#ifndef CHSIM
// ---------------------------------------------------------------------------
// Registers (WCH's ch32x035_usbpd.h)
// ---------------------------------------------------------------------------
enum : uint16_t {
    PD_ALL_CLR = 1 << 1, CC_SEL = 1 << 2, PD_DMA_EN = 1 << 3,
    IE_RX_ACT = 1 << 13, IE_RX_RESET = 1 << 14, IE_TX_END = 1 << 15,
};
enum : uint8_t { PD_TX_EN = 1 << 0, BMC_START = 1 << 1 };
enum : uint8_t {
    BUF_ERR = 1 << 2, IF_RX_BIT = 1 << 3, IF_RX_BYTE = 1 << 4, IF_RX_ACT = 1 << 5,
    IF_RX_RESET = 1 << 6, IF_TX_END = 1 << 7, MASK_PD_STAT = 0x03, PD_RX_SOP0 = 0x01,
};
enum : uint16_t {
    PA_CC_AI = 1 << 0, CC_PD = 1 << 1, CC_PU_80 = 3 << 2, CC_LVE = 1 << 4, CC_CMP_MASK = 7 << 5,
    CC_CMP_22 = 2 << 5, CC_CMP_66 = 5 << 5, CC_CMP_123 = 7 << 5,
};
static const uint32_t USBPD_IN_HVT = 1u << 9, USBPD_PHY_V33 = 1u << 8;
static const uint16_t TMR_TX_48M = 80 - 1, TMR_RX_48M = 120 - 1;
static const uint8_t UPD_SOP0 = 0x00;           // SYNC1 SYNC1 SYNC1 SYNC2
static const uint8_t UPD_HARD_RESET = 0x01 | 0x08 | 0x20 | 0x80;   // RST1 RST1 RST1 RST2
// Message types: control (no data objects) and data.
enum : uint8_t {
    T_GOODCRC = 0x01, T_ACCEPT = 0x03, T_REJECT = 0x04, T_PING = 0x05, T_PS_RDY = 0x06,
    T_GET_SRC_CAP = 0x07, T_GET_SNK_CAP = 0x08, T_DR_SWAP = 0x09, T_PR_SWAP = 0x0A,
    T_VCONN_SWAP = 0x0B, T_WAIT = 0x0C, T_SOFT_RESET = 0x0D, T_NOT_SUPPORTED = 0x10,
};
enum : uint8_t { D_SRC_CAP = 0x01, D_REQUEST = 0x02, D_BIST = 0x03, D_SNK_CAP = 0x04, D_ALERT = 0x06,
                 D_VENDOR = 0x0F };

alignas(4) static uint8_t rxBuf[34];
alignas(4) static uint8_t txBuf[34];
alignas(4) static uint8_t ackBuf[2];
static volatile uint8_t msgId;                  // in header byte 1 bits 1-3: steps of 2

// The loop's sections that the interrupt must not cut into. Short: a
// message that ends while the interrupt is held off gets its GoodCRC late.
static inline void hold() { NVIC_DisableIRQ(USBPD_IRQn); }
static inline void release() {
    if (st != OFF) NVIC_EnableIRQ(USBPD_IRQn);
}
// The interrupt's GoodCRC is on the wire: nothing else may be sent until
// its end (the interrupt then answers the message and receives again).
static inline bool sending() { return (USBPD->CONTROL & PD_TX_EN) != 0; }

static void sinkPorts() {
    // CC_PD marks the port a sink: WCH's USBPD_SNK main.c says there is no
    // 5.1k pull-down in the chip and the bit only tells sink from source (the
    // CH211's Rd is the real one). The 0.66 V comparator is the receiver.
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PD;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PD;
}

static void rxMode() {
    USBPD->CONFIG |= PD_ALL_CLR;
    USBPD->CONFIG &= ~PD_ALL_CLR;
    USBPD->CONFIG &= ~IE_TX_END;                // set only by the interrupt, for its GoodCRC
    USBPD->CONFIG |= IE_RX_ACT | IE_RX_RESET | PD_DMA_EN;
    USBPD->DMA = (uint16_t)(uint32_t)rxBuf;
    USBPD->CONTROL &= ~PD_TX_EN;
    USBPD->BMC_CLK_CNT = TMR_RX_48M;
    USBPD->CONTROL |= BMC_START;
    NVIC_EnableIRQ(USBPD_IRQn);
}

// PD_Phy_SendPack: wait = send and switch back to receive for the GoodCRC.
static void sendPack(bool wait, uint8_t *buf, uint8_t len, uint8_t sop) {
    if (USBPD->CONFIG & CC_SEL) USBPD->PORT_CC2 |= CC_LVE;
    else USBPD->PORT_CC1 |= CC_LVE;
    USBPD->BMC_CLK_CNT = TMR_TX_48M;
    USBPD->DMA = (uint16_t)(uint32_t)buf;
    USBPD->TX_SEL = sop;
    USBPD->BMC_TX_SZ = len;
    USBPD->CONTROL |= PD_TX_EN;
    USBPD->STATUS &= 0;                         // BMC_AUX invalid (WCH's "&= BMC_AUX_INVALID")
    USBPD->CONTROL |= BMC_START;
    if (!wait) return;
    for (uint32_t i = 0; i < 20000 && !(USBPD->STATUS & IF_TX_END); i++) { }
    USBPD->STATUS |= IF_TX_END;
    if (USBPD->CONFIG & CC_SEL) USBPD->PORT_CC2 &= ~CC_LVE;
    else USBPD->PORT_CC1 &= ~CC_LVE;
    USBPD->CONFIG |= PD_ALL_CLR;
    USBPD->CONFIG &= ~PD_ALL_CLR;
    USBPD->CONTROL &= ~PD_TX_EN;
    USBPD->DMA = (uint16_t)(uint32_t)rxBuf;
    USBPD->BMC_CLK_CNT = TMR_RX_48M;
    USBPD->CONTROL |= BMC_START;
}

// PD_Load_Header: sink, our data role (UFP unless swapped), the revision
// the source spoke (2.0 or 3.0).
static void header(uint8_t type) {
    txBuf[0] = (uint8_t)(type | rev << 6 | (dfp_ ? 0x20 : 0));
    txBuf[1] = (uint8_t)((msgId & 0x0E) | (srcRole ? 1 : 0));
}

// PD_Send_Handle: up to three tries, each waiting 750 us for the GoodCRC.
// From the interrupt or from a held-off section of the loop. Logged with the
// tries it took (0: never acknowledged); tag is a Request's PDO.
static bool sendMsg(const uint8_t *data, uint8_t len, uint16_t tag = 0) {
    uint8_t kind = (uint8_t)((txBuf[0] & 0x1F) | (len ? 0x80 : 0));
    txBuf[1] |= (uint8_t)((len >> 2) << 4);
    for (uint8_t i = 0; i < len; i++) txBuf[2 + i] = data[i];
    uint8_t tries = 0;
    bool ok = false;
    while (tries < 3 && !ok) {
        tries++;
        NVIC_DisableIRQ(USBPD_IRQn);
        sendPack(true, txBuf, (uint8_t)(len + 2), UPD_SOP0);
        for (uint16_t n = 250; n; n--) {
            if (USBPD->STATUS & IF_RX_ACT) {
                USBPD->STATUS |= IF_RX_ACT;
                if (USBPD->BMC_BYTE_CNT == 6 && (rxBuf[0] & 0x1F) == T_GOODCRC) {
                    msgId = (uint8_t)(msgId + 2);
                    ok = true;
                    break;
                }
            }
            waitUs(3);
        }
    }
    rxMode();
    if (!hush || !ok) plog::add(plog::K_TX, kind, ok ? tries : 0, tag);
    return ok;
}
static bool sendControl(uint8_t type) {
    header(type);
    return sendMsg(nullptr, 0);
}

// A Request: the RDO, and what the contract will be once the source says
// PS_RDY.
static bool sendRdo(uint8_t pos, uint32_t rdo, uint16_t mv, uint16_t ma) {
    uint8_t d[4] = {(uint8_t)rdo, (uint8_t)(rdo >> 8), (uint8_t)(rdo >> 16), (uint8_t)(rdo >> 24)};
    header(D_REQUEST);
    reqPos = pos;
    reqMv = mv;
    reqMa = ma;
    accepted = false;
    t0 = millis();
    if (sendMsg(d, 4, pos)) {
        setState(REQUESTED);
        return true;
    }
    return false;
}
// A fixed PDO at its full current, 3 A at most (the cable and the board).
static bool sendRequest(uint8_t pos) {
    uint32_t cur = caps[pos - 1] & 0x3FF;
    if (cur > 300) cur = 300;
    return sendRdo(pos, (uint32_t)pos << 28 | 1u << 25 | 1u << 24 | cur << 10 | cur, pdos[pos - 1].mv,
                   (uint16_t)(cur * 10));
}
// A PPS PDO: the output voltage in 20 mV units (bits 19-9) and the current
// limit in 50 mA units (bits 6-0).
static bool sendPps(uint8_t pos, uint16_t mv, uint16_t ma) {
    uint32_t rdo = (uint32_t)pos << 28 | 1u << 25 | 1u << 24 | (uint32_t)(mv / 20) << 9 | (uint32_t)(ma / 50);
    return sendRdo(pos, rdo, mv, ma);
}

// The highest fixed PDO at or below the wanted voltage (PDO 1 is 5 V).
static uint8_t pick() {
    uint8_t best = 1;
    for (uint8_t i = 0; i < npdo; i++)
        if (pdos[i].mv && pdos[i].mv <= wantMv && pdos[i].mv >= pdos[best - 1].mv) best = (uint8_t)(i + 1);
    return best;
}

// No contract, 5 V wanted: what a hard reset leaves, either way.
static void roleReset() {
    if (dfp_) plog::note(plog::N_NOW_UFP);
    dfp_ = false;
    drAt = 0;
}
static void dropContract() {
    roleReset();
    msgId = 0;
    wantMv = 5000;
    wantPps = 0;
    hush = false;
    activePos = 0;
    setContract(0, 0);
    setState(WAIT_CAPS);
    t0 = millis();
}

// PD 3.0: what this sink does not do is answered Not_Supported (PD 2.0:
// a source's Get_Source_Cap is Rejected, the rest ignored).
static void notSupported() {
    if (rev >= 2) sendControl(T_NOT_SUPPORTED);
}

// The source side's ports: our 80 uA Rp on both lines (as the host has
// them), the 0.66 V comparator the receiver on the line in use.
static void srcPorts() {
    USBPD->PORT_CC1 = (uint16_t)(CC_PU_80 | (line == 1 ? CC_CMP_66 : CC_CMP_22));
    USBPD->PORT_CC2 = (uint16_t)(CC_PU_80 | (line == 2 ? CC_CMP_66 : CC_CMP_22));
}
// 5 V 0.5 A fixed, dual-role power, USB communications, dual-role data.
static const uint8_t SRC_PDO[4] = {0x32, 0x90, 0x01, 0x26};
static bool sendSrcCaps() {
    header(D_SRC_CAP);
    return sendMsg(SRC_PDO, 4);
}
// The source side's answers, from the interrupt (PD 2.0, which we offer in).
static void srcMessage(uint8_t type, uint8_t ndo, bool ext) {
    if (ext) return;
    if (ndo) {
        if (type == D_REQUEST) {
            // Object position, RDO bits 30-28: only our one PDO. The 5 V is
            // already on, so PS_RDY follows the Accept at once.
            if ((rxBuf[5] >> 4 & 7) == 1) {
                if (sendControl(T_ACCEPT) && sendControl(T_PS_RDY)) {
                    setState(SRC_READY);
                    say(plog::N_SRC_CONTRACT);
                }
            } else {
                sendControl(T_REJECT);
            }
        } else if (type == D_VENDOR && !(rxBuf[2] & 0xC0)) {
            header(D_VENDOR);
            rxBuf[2] |= 0x80;
            sendMsg(rxBuf + 2, 4);
        }
        return;
    }
    switch (type) {
    case T_GET_SRC_CAP:
        sendSrcCaps();
        break;
    case T_GET_SNK_CAP: {
        // 5 V 3 A, dual-role power, higher capability, USB communications, dual-role data.
        static const uint8_t SNK[4] = {0x2C, 0x91, 0x01, 0x36};
        header(D_SNK_CAP);
        sendMsg(SNK, 4);
        break;
    }
    case T_PR_SWAP:
        if (prOk && st == SRC_READY && !swapPh) {
            if (sendControl(T_ACCEPT)) {
                swapPh = 1;
                swapAt = millis();
                partnerRdy = false;
                setState(SRC_SWAP);
                say(plog::N_PR_IN_OK);
            }
        } else {
            sendControl(T_REJECT);
            say(plog::N_PR_IN_NO);
        }
        break;
    case T_DR_SWAP:                             // we stay the host
    case T_VCONN_SWAP:
        sendControl(T_REJECT);
        break;
    case T_SOFT_RESET:
        msgId = 0;
        sendControl(T_ACCEPT);
        setState(SRC_CAPS);
        capsN = 0;
        capsAt = 0;
        break;
    default:
        break;
    }
}

// A message our GoodCRC acknowledged: the answer it needs, from the
// interrupt. Nothing here may use the I2C (the loop may be mid-transfer).
static void message() {
    uint8_t type = rxBuf[0] & 0x1F;
    uint8_t ndo = (rxBuf[1] >> 4) & 7;
    bool ext = (rxBuf[1] & 0x80) != 0;
    if (!hush) plog::add(plog::K_RX, (uint8_t)(type | (ndo ? 0x80 : 0)), (uint16_t)(rxBuf[0] | rxBuf[1] << 8));
    if (srcRole) {
        srcMessage(type, ndo, ext);
        return;
    }
    if (ext) {
        notSupported();                         // extended messages: not taken
    } else if (ndo) {
        switch (type) {
        case D_SRC_CAP: {
            // A new offer: its revision is the one spoken from here on. While
            // CN1 is on, the voltage stays where it is (the same supply asked
            // for again, a PPS supply at its setting); with CN1 off, the one
            // wanted.
            uint8_t r = (uint8_t)(rxBuf[0] >> 6 & 3);
            rev = r >= 2 ? 2 : 1;
            hush = false;
            drAt = 0;                           // (a new offer ends any swap of ours)
            savePdos(rxBuf + 2, ndo);
            capsIn = 1;
            bool off = pwr::cn1SurelyOff();
            uint8_t pp = wantPps;
            bool ok;
            if (pp && pp <= npdo && pdos[pp - 1].kind == PDO_PPS && rev >= 2 && ppsMv >= pdos[pp - 1].minMv &&
                ppsMv <= pdos[pp - 1].maxMv) {
                ok = sendPps(pp, ppsMv, ppsMa <= pdos[pp - 1].ma ? ppsMa : pdos[pp - 1].ma);
            } else {
                uint8_t pos = 1;
                if (off) {
                    pos = pick();
                } else {
                    for (uint8_t i = 0; i < npdo; i++)
                        if (pdos[i].mv && pdos[i].mv == cMv) pos = (uint8_t)(i + 1);
                    wantMv = pdos[pos - 1].mv;
                }
                wantPps = 0;
                ok = sendRequest(pos);
            }
            if (ok) {
                say(plog::N_CAPS_REQ);
            } else {
                say(plog::N_REQ_NOACK);
                setState(WAIT_CAPS);
                t0 = millis();
                plog::keepSoon();
            }
            break;
        }
        case D_VENDOR:
            if (!(rxBuf[2] & 0xC0)) {           // a request: NAK it (WCH's answer)
                header(D_VENDOR);
                rxBuf[2] |= 0x80;
                sendMsg(rxBuf + 2, 4);
            }
            break;
        case D_BIST:
        case D_ALERT:                           // (nothing is owed for these)
            break;
        default:
            if (type >= 0x07) notSupported();   // Get_Country_Info, Enter_USB, EPR ...
            break;
        }
    } else {
        switch (type) {
        case T_ACCEPT:
            if (drAt) {                         // our DR_Swap: the roles change now
                drAt = 0;
                dfp_ = !dfp_;
                say(plog::N_DR_DONE);
                say(dfp_ ? plog::N_NOW_DFP : plog::N_NOW_UFP);
            } else if (st == REQUESTED) {
                accepted = true;
                t0 = millis();
            }
            break;
        case T_PS_RDY:
            if (st == SRC_SWAP && swapPh == 3) {
                // The partner's 5 V is on: a sink now, its offer to come
                // (it may follow at once, so the switch is made here).
                partnerRdy = true;
                swapPh = 4;
                tcMa = 500;
                softSent = false;
                hardAt = 0;
                detAt = lastVbusAt = t0 = millis();
                setState(WAIT_CAPS);
                say(plog::N_PR_DONE);
            } else if (st == REQUESTED) {
                activePos = reqPos;
                setContract(reqMv, reqMa);
                if (pdos[reqPos - 1].kind == PDO_PPS) {
                    ppsMv = reqMv;
                    ppsMa = reqMa;
                    wantPps = reqPos;
                    ppsAt = millis();
                } else {
                    wantPps = 0;
                }
                setState(READY);
                if (!hush) say(plog::N_CONTRACT);
                hush = false;
            }
            break;
        case T_REJECT:
        case T_WAIT:
        case T_NOT_SUPPORTED:
            if (drAt) {                         // our DR_Swap, refused: the request state is untouched
                drAt = 0;
                say(plog::N_DR_REFUSED);
                break;
            }
            if (type == T_NOT_SUPPORTED) break;
            if (type == T_WAIT) {
                hush = false;
                say(plog::N_WAIT);
                setState(activePos ? READY : WAIT_CAPS);
                t0 = millis();
                break;
            }
            nReject++;
            hush = false;
            say(plog::N_REJECTED);
            setState(activePos ? READY : WAIT_CAPS);
            t0 = millis();
            break;
        case T_GET_SNK_CAP: {
            // One fixed 5 V 3 A PDO, with "higher capability" (more is welcome).
            static const uint8_t SNK[4] = {0x2C, 0x91, 0x01, 0x10};
            header(D_SNK_CAP);
            sendMsg(SNK, 4);
            break;
        }
        case T_SOFT_RESET:
            msgId = 0;
            hush = false;
            sendControl(T_ACCEPT);
            setState(WAIT_CAPS);
            t0 = millis();
            say(plog::N_SOFT_IN);
            break;
        case T_DR_SWAP:
            // Accepted only when allowed, with a contract, and none of ours in
            // flight; the Accept goes in the old role, the next in the new.
            if (swapOk && st == READY && !drAt) {
                if (sendControl(T_ACCEPT)) {
                    dfp_ = !dfp_;
                    say(plog::N_DR_IN_OK);
                    say(dfp_ ? plog::N_NOW_DFP : plog::N_NOW_UFP);
                }
            } else {
                sendControl(T_REJECT);
                say(plog::N_DR_IN_NO);
            }
            break;
        case T_VCONN_SWAP:
        case T_PR_SWAP:
            sendControl(T_REJECT);
            break;
        case T_GET_SRC_CAP:                     // a sink has none
            if (rev >= 2) notSupported();
            else sendControl(T_REJECT);
            break;
        default:
            // Get_Source_Cap_Extended, Get_Status, FR_Swap, Get_PPS_Status, ...
            if (type > T_NOT_SUPPORTED) notSupported();
            break;
        }
    }
}

extern "C" void USBPD_IRQHandler(void) __attribute__((interrupt));
extern "C" void USBPD_IRQHandler(void) {
    if (USBPD->STATUS & IF_RX_ACT) {
        USBPD->STATUS |= IF_RX_ACT;
        uint8_t n = USBPD->BMC_BYTE_CNT;
        bool sop0 = (USBPD->STATUS & MASK_PD_STAT) == PD_RX_SOP0;
        if (sop0 && n >= 6 && (n != 6 || (rxBuf[0] & 0x1F) != T_GOODCRC)) {
            if (st != DETECT && st != OFF) {
                // GoodCRC (due within 195 us); the answer follows at its end.
                waitUs(30);
                // GoodCRC, UFP, in the revision of the message (2.0 or 3.0).
                uint8_t r = (uint8_t)(rxBuf[0] >> 6 & 3);
                ackBuf[0] = (uint8_t)(0x01 | (r >= 2 ? 2 : 1) << 6 | (dfp_ ? 0x20 : 0));
                ackBuf[1] = (uint8_t)((rxBuf[1] & 0x0E) | (srcRole ? 1 : 0));   // its MessageID; our power role
                USBPD->CONFIG |= IE_TX_END;
                sendPack(false, ackBuf, 2, UPD_SOP0);
            } else {
                // Not while detecting: the line is not known yet, and the
                // source sends again.
                plog::add(plog::K_OTHER, n, USBPD->STATUS);
            }
        } else if (!sop0 || n != 6) {
            plog::add(plog::K_OTHER, n, USBPD->STATUS);
        }
    }
    if (USBPD->STATUS & IF_TX_END) {
        // Our GoodCRC went out: answer the message, then receive again.
        USBPD->CONFIG &= ~IE_TX_END;
        USBPD->PORT_CC1 &= ~CC_LVE;
        USBPD->PORT_CC2 &= ~CC_LVE;
        USBPD->STATUS |= IF_TX_END;
        message();
        rxMode();
    }
    if (USBPD->STATUS & IF_RX_RESET) {
        USBPD->STATUS |= IF_RX_RESET;
        plog::add(plog::K_HARD_IN);
        if (srcRole) {                          // a source starts its offer again
            nHard++;
            msgId = 0;
            srcPorts();
            swapPh = 0;
            setState(SRC_CAPS);
            capsN = 0;
            capsAt = 0;
            return;
        }
        sinkPorts();
        if (st != DETECT && st != OFF) {
            nHard++;
            dropContract();
            say(plog::N_HARD_IN);
            hardSeen = 1;
        }
    }
}

static void phyInit() {
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC;
    RCC->AHBPCENR |= RCC_USBPD;
    AFIO->CTLR |= USBPD_IN_HVT | USBPD_PHY_V33;   // a 3.3 V PHY on a 3.3 V supply
    USBPD->CONFIG = PD_DMA_EN;
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;
    msgId = 0;
    USBPD->CONFIG &= ~CC_SEL;
    sinkPorts();
    rxMode();
}

// One line's comparator against a threshold: 1 if CC is above it.
static bool above(uint8_t which, uint16_t cmp) {
    volatile uint16_t *p = which == 1 ? &USBPD->PORT_CC1 : &USBPD->PORT_CC2;
    *p = (uint16_t)((*p & ~(CC_CMP_MASK | PA_CC_AI)) | cmp);
    waitUs(2);
    bool a = (*p & PA_CC_AI) != 0;
    *p = (uint16_t)((*p & ~CC_CMP_MASK) | CC_CMP_66);
    return a;
}

// PD 3.0 collision avoidance: a sink starts an exchange only while the
// source's Rp says SinkTxOK (3 A: above the 1.23 V comparator).
static bool sinkTxOk() { return rev < 2 || above(line, CC_CMP_123); }
// Up to 30 ms for SinkTxOK (the source's own exchange to end), with its
// messages still answered; then the request goes anyway: a source that
// never says OK (a 1.5 A Rp) is not left unanswerable.
static void waitTxOk() {
    for (uint8_t i = 0; i < 30 && !sinkTxOk(); i++) delay(1);
}

static void setCc(uint8_t v) {
    bool ok = pwr::ccSet(v);
    plog::add(plog::K_CC, v, ok);
}

static void toDetect(uint8_t why) {
    hold();
    hush = false;
    roleReset();
    srcRole = false;
    swapPh = 0;
    setState(DETECT);
    hardSeen = capsIn = 0;
    rev = 1;
    wantPps = 0;
    detN = 0;
    wantMv = 5000;
    npdo = 0;
    activePos = 0;
    setContract(0, 0);
    if (!pwr::disableCn1()) plog::add(plog::K_CN1, 0);
    setCc(pwr::CC_PD_BOTH);
    phyInit();
    say(why);
}

// For pwr::pausePd() / resumePd(): a USB host stops the sink while it has
// the port, without linking it.
static bool halt() {
    bool was = st != OFF;
    stop();
    return was;
}
static void hookIn() {
    pwr::in::pdHalt = halt;
    pwr::in::pdResume = start;
}

void start() {
    hookIn();
    // Not without the CH211, nor in host mode (its 5 V on the port).
    if (st != OFF || !pwr::diag().ch211 || pwr::pa0Driven()) return;
    loggedMv = 0;
    toDetect(plog::N_WAITING);
}

void stop() {
    if (st == OFF) return;
    hold();
    USBPD->CONFIG = 0;
    USBPD->PORT_CC1 = 0;
    USBPD->PORT_CC2 = 0;
    setState(OFF);
    roleReset();
    srcRole = false;
    swapPh = 0;
    npdo = 0;
    setContract(0, 0);
    pwr::disableCn1();
    setCc(pwr::CC_SINK);
    say(plog::N_PD_OFF);
}

// CN1 left on through a new voltage, as a bench supply's output follows its
// knob, unless the sketch asked for it off first.
static bool offOnChange;
void setOutputOffOnChange(bool off) { offOnChange = off; }

bool request(uint8_t pos) {
    if (st != READY || pos < 1 || pos > npdo || !pdos[pos - 1].mv || pdos[pos - 1].mv > 20000) return false;
    if (offOnChange && !pwr::disableCn1()) {    // CN1 off before the voltage moves
        say(plog::N_CN1_STUCK);
        return false;
    }
    waitTxOk();
    hold();
    bool ok = false;
    if (st != READY || sending()) {
        say(plog::N_BUSY);
    } else {
        wantMv = pdos[pos - 1].mv;
        wantPps = 0;
        ok = sendRequest(pos);
        say(ok ? plog::N_REQ_SENT : plog::N_REQ_NOACK);
    }
    release();
    return ok;
}

bool requestPps(uint8_t pos, uint16_t mv, uint16_t ma) {
    if (st != READY || pos < 1 || pos > npdo || pdos[pos - 1].kind != PDO_PPS || rev < 2) return false;
    const Pdo &d = pdos[pos - 1];
    if (mv < d.minMv) mv = d.minMv;
    if (mv > d.maxMv) mv = d.maxMv;
    mv = (uint16_t)(mv - mv % 20);
    if (ma > d.ma) ma = d.ma;
    if (ma > 3000) ma = 3000;
    if (ma < 50) ma = 50;
    ma = (uint16_t)(ma - ma % 50);
    // Another supply, if the sketch wants CN1 off for that: off first. The
    // PPS one in use: a turn of its knob, always.
    if (offOnChange && activePos != pos && !pwr::disableCn1()) {
        say(plog::N_CN1_STUCK);
        return false;
    }
    waitTxOk();
    hold();
    bool ok = false;
    if (st != READY || sending()) {
        say(plog::N_BUSY);
    } else {
        ok = sendPps(pos, mv, ma);
        say(ok ? plog::N_REQ_SENT : plog::N_REQ_NOACK);
    }
    release();
    return ok;
}

bool startSource(uint8_t l) {
    hookIn();
    if (st != OFF || !pwr::diag().ch211 || !pwr::pa0Driven() || (l != 1 && l != 2)) return false;
    hold();
    line = l;
    srcRole = true;
    dfp_ = true;
    rev = 1;                                    // PD 2.0: no collision avoidance on our 80 uA Rp
    msgId = 0;
    swapPh = 0;
    partnerRdy = false;
    npdo = 0;
    activePos = 0;
    drAt = 0;
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC;
    RCC->AHBPCENR |= RCC_USBPD;
    AFIO->CTLR |= USBPD_IN_HVT | USBPD_PHY_V33;
    USBPD->CONFIG = PD_DMA_EN;
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;
    if (l == 1) USBPD->CONFIG &= ~CC_SEL;
    else USBPD->CONFIG |= CC_SEL;
    srcPorts();
    setState(SRC_CAPS);
    capsN = 0;
    capsAt = 0;
    say(plog::N_SRC_START);
    rxMode();
    return true;
}

// The source side, from service(): the offer until it is heard, and a power
// role swap's steps.
static void srcService(uint32_t now) {
    switch (st) {
    case SRC_CAPS: {
        if (capsN >= 30 || (int32_t)(now - capsAt) < 150) break;
        hold();
        if (st == SRC_CAPS && !sending()) {
            capsAt = now;
            if (sendSrcCaps()) capsAt = now + 850;  // heard: the Request is the interrupt's; again in 1 s if none
            else if (++capsN >= 30) say(plog::N_SRC_NOPD);
        }
        release();
        break;
    }
    case SRC_SWAP:
        if (swapPh == 1 && now - swapAt >= 25) {
            // tSrcTransition: our 5 V off, PA0 analog to watch it fall.
            pwr::pa0Low();
            pwr::pa0Analog();
            swapPh = 2;
            swapAt = now;
        } else if (swapPh == 2 && pwr::vbusValid() && (pwr::vbusMv() < 800 || now - swapAt > 600)) {
            // vSafe0V: our Rp off, the CH211's Rd on the line, PS_RDY as a sink.
            hold();
            srcRole = false;
            USBPD->PORT_CC1 = 0;
            USBPD->PORT_CC2 = 0;
            setCc(line == 1 ? pwr::CC_PD_1 : pwr::CC_PD_2);
            sinkPorts();
            rxMode();
            hold();
            bool ok = sendControl(T_PS_RDY);
            swapPh = 3;
            swapAt = millis();
            release();
            say(plog::N_PR_RD_ON);
            if (!ok) {
                swapPh = 5;
                say(plog::N_PR_FAILED);
            }
        } else if (swapPh == 3 && now - swapAt > 600) {
            swapPh = 5;                         // (its PS_RDY is the interrupt's)
            say(plog::N_PR_FAILED);
        }
        break;
    default:
        break;
    }
}

bool requestDataSwap() {
    if (st != READY || drAt) return false;
    waitTxOk();
    hold();
    bool ok = false;
    if (st == READY && !drAt && !sending()) {
        uint32_t now = millis();
        drAt = now ? now : 1;                   // (before the send: the Accept may come at once)
        header(T_DR_SWAP);
        ok = sendMsg(nullptr, 0);
        if (ok) say(plog::N_DR_SENT);
        else drAt = 0;
    }
    release();
    return ok;
}

// From the loop, held off: a soft reset, or a hard one if that is not
// acknowledged.
static void softReset() {
    nSoft++;
    msgId = 0;
    if (sendControl(T_SOFT_RESET)) {
        say(plog::N_SOFT_SENT);
        setState(WAIT_CAPS);
        t0 = millis();
    } else {
        NVIC_DisableIRQ(USBPD_IRQn);
        sendPack(true, nullptr, 0, UPD_HARD_RESET);
        rxMode();
        nHard++;
        plog::add(plog::K_HARD_OUT);
        dropContract();
        hardAt = millis();
        hardSeen = 1;                           // CN1 off, as for the source's
        say(plog::N_HARD_SENT);
        plog::keepSoon();
    }
}

void service() {
    pwr::poll();                                // (every 10 ms at most: the checks run whatever the app does)
    if (st == OFF) return;
    uint32_t now = millis();
    if (srcRole || st == SRC_SWAP) {
        srcService(now);
        return;
    }

    // What the interrupt left for the I2C: CN1 off after a hard reset (VBUS
    // goes to 0 V), and after a new offer if the sketch wants it so.
    if (hardSeen) {
        hardSeen = 0;
        hardAt = now;
        plog::add(plog::K_CN1, pwr::disableCn1());
        plog::keepSoon();
    }
    if (capsIn) {
        capsIn = 0;
        if (offOnChange && pwr::out() == pwr::OUT_ON) plog::add(plog::K_CN1, pwr::disableCn1());
    }

    // The PHY left sending (an interrupt's GoodCRC that never ended).
    if (sending()) {
        if (!txSince) txSince = now ? now : 1;
        else if (now - txSince > 20) {
            hold();
            rxMode();
            say(plog::N_TX_STUCK);
            txSince = 0;
        }
    } else {
        txSince = 0;
    }

    // Detach: VBUS gone (PA0 is analog whenever PD runs), except for the
    // second or so a hard reset takes VBUS to 0 V, and the half second after
    // CC is seen (a source turns VBUS on up to 275 ms after it sees Rd). And
    // the CH211's CC.
    if (st != DETECT && now - lastVbusAt >= 50) {
        lastVbusAt = now;
        if (pwr::vbusValid()) {
            uint16_t v = pwr::vbusMv();
            // Logged when it moves by an eighth (0.5 V at least): at 20 V a
            // charger's output reads 1 V either side from one look to the next.
            int32_t dv = (int32_t)v - loggedMv, step = loggedMv / 8;
            if (step < 500) step = 500;
            if (dv > step || dv < -step) {
                plog::add(plog::K_VBUS, 0, v);
                loggedMv = v;
            }
            if (v < 3500 && now - detAt >= 500) {
                if (hardAt && now - hardAt < 2500) {
                    if (evId != plog::N_VBUS_LOW_OK) say(plog::N_VBUS_LOW_OK);
                } else {
                    toDetect(plog::N_UNPLUGGED);
                    plog::keepSoon();
                    return;
                }
            }
        }
        if (pwr::reg(pwr::CC_CTRL) != pwr::ccWanted()) {
            // The CH211 reset CC_CTRL itself (over-temperature): start again.
            toDetect(plog::N_CC_RESET);
            plog::keepSoon();
            return;
        }
    }

    // Our DR_Swap unanswered (tSenderResponse is 24-30 ms; a slow partner
    // gets 100).
    if (drAt && now - drAt > 100) {
        hold();
        if (drAt && millis() - drAt > 100) {
            drAt = 0;
            say(plog::N_DR_NO_REPLY);
        }
        release();
    }

    // PPS: the request again every 8 s (a source drops a PPS contract after
    // 15 s without one), unlogged unless it fails.
    if (st == READY && wantPps && activePos == wantPps && now - ppsAt > 8000) {
        waitTxOk();
        hold();
        if (st == READY && !sending()) {
            hush = true;
            ppsAt = now;
            if (!sendPps(wantPps, ppsMv, ppsMa)) {
                hush = false;
                say(plog::N_REQ_NOACK);
            }
        } else {
            ppsAt = now - 7900;                 // again in 100 ms
        }
        release();
    }

    switch (st) {
    case DETECT: {
        static uint32_t at;
        if (now - at < 5) break;
        at = now;
        uint8_t l = above(1, CC_CMP_22) ? 1 : above(2, CC_CMP_22) ? 2 : 0;
        if (!l) {
            detN = 0;
            break;
        }
        if (++detN < 5) break;
        line = l;
        tcMa = above(l, CC_CMP_123) ? 3000 : above(l, CC_CMP_66) ? 1500 : 500;
        hold();
        if (l == 1) USBPD->CONFIG &= ~CC_SEL;
        else USBPD->CONFIG |= CC_SEL;
        setCc(l == 1 ? pwr::CC_PD_1 : pwr::CC_PD_2);
        plog::add(plog::K_SOURCE, l, tcMa);
        softSent = false;
        hardAt = 0;
        detAt = now;
        setState(WAIT_CAPS);
        t0 = now;
        say(l == 1 ? plog::N_SOURCE_CC1 : plog::N_SOURCE_CC2);
        rxMode();
        break;
    }
    case WAIT_CAPS: {
        uint32_t since = now - t0;
        if (since <= 1000) break;
        uint16_t v = pwr::vbusMv();
        hold();
        if (st == WAIT_CAPS && !sending()) {
            // A contract above 5 V outlives a reset: ask the source to offer again.
            if (!softSent && v > 5500) {
                softSent = true;
                softReset();
            } else if (now - t0 > 3000) {
                if (v > 5500) {
                    // No offer, and VBUS is not 5 V: no contract to go by.
                    softSent = false;
                    t0 = now;
                    say(plog::N_VBUS_HIGH);
                } else {
                    setState(TYPEC);
                    setContract(5000, tcMa);
                    say(plog::N_NO_PD);
                }
            }
        }
        release();
        break;
    }
    case REQUESTED:
        if (now - t0 > (accepted ? 550u : 500u)) {
            hold();
            if (st == REQUESTED && !sending() && millis() - t0 > (accepted ? 550u : 500u)) {
                hush = false;
                say(plog::N_NO_ANSWER);
                setState(activePos ? READY : WAIT_CAPS);
                softReset();
            }
            release();
        }
        break;
    default:
        break;
    }
}

#else  // CHSIM

bool startSource(uint8_t) { return false; }
bool requestDataSwap() {
    if (st != READY) return false;
    dfp_ = !dfp_;
    say(plog::N_DR_DONE);
    return true;
}

static bool offOnChange;
void setOutputOffOnChange(bool off) { offOnChange = off; }

static bool halt() {
    bool was = st != OFF;
    stop();
    return was;
}
// A charger like the bench's: 5, 9, 15 and 20 V at 3 A, and PPS 3.3-21 V 3 A.
void start() {
    pwr::in::pdHalt = halt;
    pwr::in::pdResume = start;
    line = 1;
    tcMa = 3000;
    rev = 2;
    plog::add(plog::K_SOURCE, 1, 3000);
    static const uint8_t SIM[] = {0x2C, 0x91, 0x01, 0x08, 0x2C, 0xD1, 0x02, 0x00, 0x2C, 0xB1, 0x04, 0x00,
                                  0x2C, 0x41, 0x06, 0x00, 0x3C, 0x21, 0xA4, 0xC1};
    plog::add(plog::K_RX, 0x81, 0x51A1);
    savePdos(SIM, 5);
    plog::add(plog::K_TX, 0x82, 1, 1);
    plog::add(plog::K_RX, 0x03, 0x03A3);
    plog::add(plog::K_RX, 0x06, 0x05A6);
    activePos = 1;
    setContract(5000, 3000);
    setState(READY);
    say(plog::N_CONTRACT);
}
void stop() {
    st = OFF;
    setContract(0, 0);
}
void service() {}
bool request(uint8_t pos) {
    if (pos < 1 || pos > npdo || pdos[pos - 1].kind != PDO_FIXED) return false;
    if (offOnChange) pwr::disableCn1();
    activePos = pos;
    wantPps = 0;
    setContract(pdos[pos - 1].mv, pdos[pos - 1].ma);
    say(plog::N_CONTRACT);
    return true;
}
bool requestPps(uint8_t pos, uint16_t mv, uint16_t ma) {
    if (pos < 1 || pos > npdo || pdos[pos - 1].kind != PDO_PPS) return false;
    const Pdo &d = pdos[pos - 1];
    if (mv < d.minMv) mv = d.minMv;
    if (mv > d.maxMv) mv = d.maxMv;
    mv = (uint16_t)(mv - mv % 20);
    if (ma > d.ma) ma = d.ma;
    if (ma < 50) ma = 50;
    ma = (uint16_t)(ma - ma % 50);
    if (offOnChange && activePos != pos) pwr::disableCn1();
    activePos = pos;
    wantPps = pos;
    ppsMv = mv;
    ppsMa = ma;
    setContract(mv, ma);
    say(plog::N_CONTRACT);
    return true;
}
#endif

}  // namespace pd
