// What the power section's own files share (not for sketches).
//
// The library is linked a piece at a time: the core (Power.cpp) is in every
// sketch that calls pwr::begin(); CN1 (Cn1.cpp), the PD sink (Pd.cpp) and
// the log (PdLog.cpp) only in a sketch that calls them. The core reaches
// them only through the hooks below, which each piece fills in when the
// sketch first uses it, so nothing in the core pulls a piece in.
#pragma once
#include "Power.h"

namespace pwr {
namespace in {

extern Diag dg;
extern const char *whyText;
extern uint8_t regs[8];
extern Out outState;
extern bool driven;                     // PA0 an output
extern bool offFailed;                  // the inhibit did not read back: poll() retries
extern volatile bool enabling;          // enableCn1() between the permit and its result
extern volatile uint16_t conMv, conMa;  // the PD contract (onContract())
extern volatile bool conNew;

// CN1's checks and telemetry (Cn1.cpp), from its first use: poll() runs
// them every 10 ms; a trip records the reading it happened at.
extern void (*cn1Poll)(uint32_t now);
extern void (*cn1Trip)();
// The PD sink (Pd.cpp), from its first start(): pausePd() stops it (true if
// it was running), resumePd() starts it again.
extern bool (*pdHalt)();
extern void (*pdResume)();

// CN1 off by a fault (outState TRIPPED, the inhibit retried until it reads
// back).
void trip(const char *why);
#ifndef CHSIM
bool inaRead(uint8_t r, uint16_t &v);
bool inaWrite(uint8_t r, uint16_t v);
bool inhibit();
bool permit();
bool inhibitOk();
#endif

}  // namespace in
}  // namespace pwr
