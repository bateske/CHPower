// The PD sink's notes as text (PdLog.h): for its event() and the log alike,
// so neither needs the other.
#include "PdLog.h"

namespace plog {

static const char *const NOTES[N_COUNT] = {
    "",
    "NO SOURCE",
    "SOURCE ON CC1",
    "SOURCE ON CC2",
    "UNPLUGGED",
    "CC RESET: OVER-TEMP?",
    "PD OFF",
    "NO PD: 5 V",
    "OVER 5.5 V, NO OFFER",
    "OFFER IN, ASKED",
    "REQUEST UNHEARD",
    "REQUEST SENT",
    "CN1 STUCK ON",
    "CONTRACT",
    "REJECTED",
    "SOURCE SAID WAIT",
    "SOFT RESET IN",
    "SOFT RESET SENT",
    "HARD RESET SENT",
    "HARD RESET IN",
    "NO REPLY IN TIME",
    "PHY STUCK: RESTART",
    "BUSY: TRY AGAIN",
    "HARD RESET: VBUS 0",
    "DR SWAP IN: ACCEPTED",
    "DR SWAP IN: REJECTED",
    "DR SWAP SENT",
    "DR SWAP ACCEPTED",
    "DR SWAP REFUSED",
    "DR SWAP: NO REPLY",
    "DATA ROLE: HOST (DFP)",
    "DATA ROLE: DEVICE (UFP)",
    "PD SOURCE: OFFERING 5V",
    "SOURCE CONTRACT 5V",
    "NO PD SINK: OFFER STOPS",
    "PR SWAP IN: ACCEPTED",
    "PR SWAP IN: REJECTED",
    "PR SWAP: 5V OFF, RD ON",
    "PR SWAP DONE: NOW SINK",
    "PR SWAP FAILED",
};
const char *noteText(uint8_t n) { return n < N_COUNT ? NOTES[n] : "?"; }

}  // namespace plog
