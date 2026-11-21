// The USB-PD sink: the CH32X035's USBPD peripheral on PC14/PC15, reached
// through the CH211's CC switches, with the CH211 keeping Rd on both lines.
// The protocol is WCH's USBPD_SNK example (PD_Process.c, V1.0.1), carried
// over with: the answers to the source sent from the interrupt (a source
// waits only about 25 ms for a Request, and a frame here can take longer),
// no blocking delays outside it, millis() timers, the CC path connected
// through the CH211 (both lines while detecting, then the active one), a
// requested voltage instead of always PDO 1, and detach seen as VBUS
// falling (except while a hard reset takes it to 0 V). Fixed PDOs only; a
// source without PD is a Type-C 5 V contract at the current its CC level
// advertises. Everything it does goes to the PD log (PdLog.h).
#pragma once
#include <stdint.h>

namespace pd {

enum State : uint8_t {
    OFF,        // not running (host mode, or before start())
    DETECT,     // no source on CC
    WAIT_CAPS,  // a source: waiting for Source_Capabilities
    REQUESTED,  // Request sent: waiting for Accept, then PS_RDY
    READY,      // a PD contract
    TYPEC,      // a source without PD: 5 V at its Type-C current
    // The source side (startSource(), CHUSBHost's PD_SWAP): host power on,
    // our 5 V offered, and a power role swap when the partner asks for it.
    SRC_CAPS,   // our Source_Capabilities going out (stopped after 30 unheard)
    SRC_READY,  // the partner took our 5 V
    SRC_SWAP,   // PR_Swap accepted: our 5 V off, Rd on, waiting for the partner's
};

enum PdoKind : uint8_t { PDO_FIXED, PDO_PPS, PDO_OTHER };
struct Pdo {
    uint8_t kind;
    uint16_t mv;            // a fixed supply's voltage; 0 for any other kind
    uint16_t minMv, maxMv;  // the range (a PPS supply's; a fixed one's is mv..mv)
    uint16_t ma;            // the most current it offers
};

void start();           // CC to the MCU, the PHY on, detection
void stop();            // the PHY off, CC a plain sink (0x44)
void service();         // from loop(), every pass: CN1 off, VBUS, detection, timeouts
State state();
uint8_t cc();           // 1 or 2: the line in use; 0 none
uint16_t typecMa();     // the source's Type-C advertisement: 500, 1500, 3000
uint8_t pdoCount();
const Pdo &pdo(uint8_t i);      // 0-based
uint8_t active();       // the accepted PDO, 1-based; 0 none
uint16_t contractMv();  // 0 without a contract
uint16_t contractMa();
// Ask for a fixed PDO (1-based). CN1 stays as it is: a bench supply's
// output follows its knob (but see setOutputOffOnChange()). False if there
// is no PD source or a request is already in flight.
bool request(uint8_t pos);
// Ask a PPS PDO (1-based) for a voltage (20 mV steps) and a current limit
// (50 mA steps): the source then regulates like a bench supply, and the
// sketch repeats the request every 8 s as PPS needs. CN1 stays as it is.
// Needs PD 3.0 talk.
bool requestPps(uint8_t pos, uint16_t mv, uint16_t ma);
// true: CN1 goes off before a request for another supply (any fixed one,
// or the PPS one from a fixed one), and when the charger sends a new offer,
// so the load never sees the voltage move. false, the default: it stays on.
// Either way it goes off when the contract ends (unplugged, a hard reset).
void setOutputOffOnChange(bool off);
bool ppsActive();       // the contract is a PPS one
uint8_t specRev();      // the PD revision spoken: 2 or 3
bool busy();            // an exchange in flight: no flash saves now
bool quiet();           // no message due from the source: a flash write may stop the CPU
uint32_t changedAt();   // millis of the last contract change, or detach
const char *event();    // the last thing that happened, for the screen
// Data roles. A sink starts as the USB device (UFP); a powered hub, whose
// upstream port is a device, gives the host role (DFP) to the sink it powers
// with a DR_Swap. Rejected unless allowed (the default: CHPDTrigger and the
// like stay the device). Each swap is logged; detach, a hard reset and
// stop() put the role back to UFP.
void allowDataSwap(bool on);
bool requestDataSwap();     // send DR_Swap (a contract needed); false if it could not go out
bool dataSwapPending();     // ours sent, no answer yet
bool dfp();                 // true: we are the USB host (DFP)
uint32_t srcPdo(uint8_t i); // the source's PDO as sent (0-based): PDO 1 holds its flags
// The flags of a fixed PDO 1 (USB PD 3.0 table 6-9).
enum : uint32_t { PDO_DRP = 1u << 29, PDO_SUSPEND = 1u << 28, PDO_UNCONSTRAINED = 1u << 27,
                  PDO_USB_COMM = 1u << 26, PDO_DRD = 1u << 25 };
// The source side, for a USB host on battery power (host power on, our Rp
// on CC, the line in use given): offers 5 V 0.5 A as a dual-role port, and
// with allowPowerSwap() accepts a PR_Swap: our 5 V off (PA0 low), the
// CH211's Rd on, then the partner's 5 V, as a sink that stays the USB host.
// swapPhase(): 0 none, 1-3 under way (the host must not take CC or VBUS
// going as a detach), 4 done (now the sink: WAIT_CAPS then READY), 5 failed.
bool startSource(uint8_t line);
void allowPowerSwap(bool on);
uint8_t swapPhase();
uint16_t hardResets();
uint16_t softResets();
uint16_t rejects();

}  // namespace pd
