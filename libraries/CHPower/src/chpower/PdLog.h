// A record of what the PD sink and the CH211 did, for the PD LOG screen and
// the serial port: the last 160 events in RAM, each with the time it
// happened (to the microsecond), and the last 16 kept in flash with the test
// results at the moments that matter (a hard reset, the source gone, a
// request not answered), so a run that ended in a power loss can still be
// read after the next start ("the last run").
#pragma once
#include <stdint.h>

namespace plog {

enum Kind : uint8_t {
    K_START,    // a SYS_STAT, b VBUS mV, c PIN_STAT
    K_STATE,    // a the PD state (pd::State)
    K_SOURCE,   // a the CC line, b its Type-C current, mA
    K_CC,       // a CC_CTRL asked for, b 1 read back, 0 not
    K_RX,       // a message type | 0x80 for a data message, b its header
    K_TX,       // a type | 0x80 data, b tries (0: no GoodCRC), c a Request's PDO
    K_OTHER,    // a packet not answered: a byte count, b STATUS
    K_HARD_IN,  // the source's hard reset
    K_HARD_OUT, // ours
    K_PDO,      // a its position, b mV (0: not a fixed supply), c mA
    K_VBUS,     // b mV on PA0
    K_STAT,     // a SYS_STAT, b PIN_STAT, when a watched bit changes
    K_NOTE,     // a a Note
    K_CN1,      // a 1 CN1 off, 0 it would not turn off
    K_RAW,      // a a source PDO's position, b its low 16 bits, c its high 16 (the flags)
    K_MARK,     // a an app's step marker (the PD hub probe's steps)
    K_HOST,     // CHUSBHost cut its 5 V: a why (1 CC gone, 2 VBUS fell, 3 over-temperature, 4 swap failed),
                // b CC1's level << 4 | CC2's (0 under 0.22 V, 1 to 0.66, 2 to 1.23, 3 over), c ms gone
};

enum Note : uint8_t {
    N_NONE, N_WAITING, N_SOURCE_CC1, N_SOURCE_CC2, N_UNPLUGGED, N_CC_RESET, N_PD_OFF, N_NO_PD,
    N_VBUS_HIGH, N_CAPS_REQ, N_REQ_NOACK, N_REQ_SENT, N_CN1_STUCK, N_CONTRACT, N_REJECTED, N_WAIT,
    N_SOFT_IN, N_SOFT_SENT, N_HARD_SENT, N_HARD_IN, N_NO_ANSWER, N_TX_STUCK, N_BUSY, N_VBUS_LOW_OK,
    // Data roles (pd::allowDataSwap()): a DR_Swap asked of us, or asked by us.
    N_DR_IN_OK, N_DR_IN_NO, N_DR_SENT, N_DR_DONE, N_DR_REFUSED, N_DR_NO_REPLY, N_NOW_DFP, N_NOW_UFP,
    // The source side (pd::startSource()).
    N_SRC_START, N_SRC_CONTRACT, N_SRC_NOPD, N_PR_IN_OK, N_PR_IN_NO, N_PR_RD_ON, N_PR_DONE, N_PR_FAILED,
    N_COUNT
};

struct Ev {
    uint32_t ms;        // millis()
    uint16_t us;        // and the microseconds within it
    uint8_t kind, a;
    uint16_t b, c;
};
static_assert(sizeof(Ev) == 12, "twelve bytes an event: sixteen fit the save page");

static const uint8_t KEEP = 16;     // events kept in flash

// Recording starts with begin(): until then (and in a sketch that never
// calls it) the PD sink and the power section log nothing, and the log's
// 2.1 KB of RAM and its code are not linked.
void begin();

// From anywhere, the PD interrupt included: nothing before begin().
extern void (*hook)(uint8_t kind, uint8_t a, uint16_t b, uint16_t c);
static const uint8_t K_KEEP = 0xFF;  // (not an event: keepSoon())
void add(uint8_t kind, uint8_t a = 0, uint16_t b = 0, uint16_t c = 0);
inline void note(uint8_t n) { add(K_NOTE, n); }
const char *noteText(uint8_t n);    // (the PD sink's notes: no log needed)

// The run now (last = false) or the run before this start (last = true),
// oldest first.
uint16_t count(bool last);
const Ev &at(bool last, uint16_t i);
uint32_t total();               // events this run, the ones rolled out too
bool frozen();                  // nothing more is recorded
void setFrozen(bool f);
void clear();

// "RX SRC CAPS X5 #0 R3": the event without its time, at most 24 characters
// (out holds 32). timeText: "12.345" seconds, or with micro: "12.345678".
void describe(const Ev &e, char *out);
void timeText(const Ev &e, char *out, bool micro = false);

// The last run: the app keeps this run's last events (in its save data, say)
// when keepDue() says so (after a hard reset, an unplug, a request the
// source never heard; no more than every 1.5 s), and hands them back at the
// next start.
inline void keepSoon() { add(K_KEEP); }    // (the PD sink calls it)
bool keepDue();
uint8_t tail(Ev *out);          // this run's last KEEP events into out, oldest first; how many
void setLastRun(const Ev *in, uint8_t n);

// Both runs as text on the serial port (when a PC sends any byte).
void dump();

}  // namespace plog
