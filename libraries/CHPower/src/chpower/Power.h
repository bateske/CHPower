// The Rev 1.5 power section: the CH211 (0x35, fast read 0x34), the INA226
// (0x40), PA0 (VBUS sense and host-power enable) and CN1, the protected
// output. Written from docs/design.md (sections 2 to 5 and 8) and the
// board handoff (sections 7 and 8); the rules that matter most:
//   - CN1 is permitted only by enableCn1()'s transaction, with the INA226
//     configured and checked while CN1 is still inhibited;
//   - while CN1 is permitted, the INA226's Mask/Enable (0x06) is never read
//     and its Configuration (0x00) never written (either can clear the trip
//     latch); telemetry reads 0x01-0x04 one at a time and leaves the
//     pointer on 0x02;
//   - every CH211 write is read-modify-write of the owned bits, read back;
//   - PA0 is driven only for host power; VBUS is read only while it is an
//     analog input.
// The I2C is this file's own (GPIO, 100 kHz): the chip has no general open-
// drain GPIO mode, so a line is released by making it an input (the board's
// 4.7k pull-ups) and pulled low as a push-pull output whose latch is 0.
//
// What a sketch pays for: begin(), poll() and the PA0/CC calls are the core.
// CN1 (enableCn1(), setLimit(), tele()) is linked, and its checks join
// poll(), only once the sketch calls one of them.
#pragma once
#include <stdint.h>

namespace pwr {

// CH211 registers and the bits used (CH211 manual 5.6).
enum : uint8_t { PIN_STAT = 0, PIN_CFG, CC_CTRL, OD_CTRL, HVCP_CTRL, HVIO_KEY, SYS_CFG, SYS_STAT };
enum : uint8_t { ST_HVLI = 0x04, ST_VBUS_RDY = 0x10, ST_VBUS_OV = 0x20 };
enum : uint8_t { SYS_VBUS_EXIST = 0x02, SYS_OT_RST = 0x40 };
// CC_CTRL values (docs/design.md section 4).
enum : uint8_t { CC_SINK = 0x44, CC_PD_BOTH = 0x77, CC_PD_1 = 0x47, CC_PD_2 = 0x74, CC_HOST = 0x33 };

enum Out : uint8_t { OUT_OFF, OUT_ON, OUT_TRIPPED, OUT_BLOCKED };

struct Tele {
    int32_t busMv;      // CN1's voltage (VUSB_OUT), 1.25 mV a count
    int32_t ma;         // CN1's current, toward CN1 positive, 250 uA a count
    int32_t mw;         // 6.25 mW a count
    int32_t shuntUv;    // 2.5 uV a count
};

struct Diag {
    bool ch211, ina;        // found and checked by begin()
    bool sclHeld;           // begin() found SCL held low (INT_PIN left on by other firmware)
    uint8_t boot[8];        // the CH211's registers as begin() found them
    uint16_t inaBootMask;   // the INA226's 0x06 as begin() found it (0x10: a trip was latched)
    uint16_t busErrors;     // transfers that failed or did not read back
    uint16_t ccFaults;      // CC_CTRL found changed (the CH211's over-temperature reset)
};

// The devices checked, CN1 inhibited, CC as a plain sink (Rd on both lines,
// paths open), the INA226 reset and calibrated. False: no CH211 or no
// INA226; every power feature is then refused.
bool begin();
// The CH211's registers, CN1's enable node, the telemetry, the checks on
// them (a trip, I2C lost, VBUS off the contract, a CC reset). No more than
// every 10 ms, whoever calls it: pd::service() and host::service() do, so
// an app need not (calling it too is harmless).
void poll();
const Diag &diag();
const char *why();          // the last refusal or fault, for the screen ("" if none)
void setWhy(const char *s);

// VBUS from PA0 (VBUS / 101), 8 samples. Valid only while PA0 is an analog
// input and has settled: 0 otherwise.
uint16_t vbusMv();
bool vbusValid();
uint8_t reg(uint8_t r);     // the CH211's register from the last poll
bool readRegs();            // read all eight now

// CN1.
Out out();
// nullptr when CN1 is on; otherwise why not. The contract is the library's
// own (the PD sink tells it: see onContract()): refused without one, with
// the limit over its budget (budgetMa()) or under 100 mA, or in host mode.
// Then the INA226's rearm transaction and the CH211's permit, each read
// back. CN1 gets VBUS as the charger delivers it: the contract is the
// charger's word, and a new voltage (pd::request()) carries on to CN1
// unless the sketch asks otherwise (pd::setOutputOffOnChange()).
const char *enableCn1(uint16_t limitMa);
// The inhibit, read back, with HVLI low: true when CN1 is known off. On
// false, poll() keeps trying, and callers that need CN1 off must not go on.
bool disableCn1();
bool cn1OffFailed();
// A new trip limit (the INA226's alert limit, read back when CN1 is on),
// kept for the next enable when off. Clamped to 100 mA .. budgetMa() (2.4 A
// at most): false when it was clamped or did not read back.
bool setLimit(uint16_t limitMa);
// The PD contract (the PD sink calls this, from its interrupt too: it only
// takes note; poll() acts). While CN1 is on, a contract that ends turns it
// off, and one whose budget is under the limit lowers the limit (or turns
// CN1 off under 100 mA).
void onContract(uint16_t mv, uint16_t ma);
uint16_t budgetMa();        // allowMa() of the contract now (0 without one)
// For the PD interrupt (no I2C): CN1 is inhibited and read back so, and not
// being turned on. Otherwise a new offer keeps the voltage it has.
bool cn1SurelyOff();
const Tele &tele();
uint16_t limitMa();
int32_t peakMa();           // the highest current since CN1 went on
int32_t tripMa();           // the last reading before the trip
uint32_t tripMs();          // when the trip was seen (millis)
uint16_t allowMa(uint16_t contractMv, uint16_t contractMa);   // the budget for CN1

// For the PD sink and a USB host (CHUSBHost).
bool ccSet(uint8_t v);      // CC_CTRL, read back; poll() then expects it
// The PD sink stopped if it is running (true if it was), and started again:
// for a USB host that takes the port. Neither links the PD sink in.
bool pausePd();
void resumePd();
uint8_t ccWanted();
void pa0Low();
// Host power on: refused (false, PA0 left low) while CN1 is on, or while
// the CH211 sees VBUS (another source on the port): 5 V never goes onto
// another source's 5 V.
bool pa0High();
void pa0Analog();
bool pa0Driven();

// Before every reset: host power off, CN1 inhibited, the INA226 reset, the
// PD peripheral off, CC back to a plain sink. The CHGame library calls it
// itself before its exit to the menu (START held, exitToMenu(), the debug
// protocol's B), through chgame_beforeExit(), which this library defines.
// (An upload's 1200-baud touch and a crash do not pass through it: the
// INA226's cut-off and the core's crash handler, PA0 low, guard those.)
void safeDown();

}  // namespace pwr
