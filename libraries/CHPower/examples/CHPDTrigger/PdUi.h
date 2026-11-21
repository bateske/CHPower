// PD mode: the trigger (a voltage, a current limit and an output, with a
// meter and a scope) and its menu: the charger's offer, the guided tour,
// and the diagnostics (live status, the CH211's registers, the PD log).
#pragma once
#include <stdint.h>

namespace trig {

void begin();               // the last settings (saved::)
void service();             // every frame, on every screen: applies the dial, watches for trips, samples
// The voltage asked for: a fixed PDO's, or anything in a PPS range (20 mV
// steps). Applied half a second after the last change (now: at once).
uint16_t dialMv();
void setDial(uint16_t mv, bool now = false);
bool settled();             // the contract is what the dial asks for
uint16_t limitMa();
void setLimit(uint16_t ma);
void toggle();              // A: the output on, off, or rearmed after a trip (sounds, messages)
uint8_t ppsPos();           // the charger's PPS PDO (1-based; 0: none, or no PD 3.0)
uint32_t trips();           // trips seen since start
int32_t tripMa();           // the reading at the last one
void meter(int y);          // the meter, 27 px tall

}  // namespace trig

namespace pdui {
void tick();                // the current screen's input (S_PD, S_PDMENU, S_OFFER, S_STATUS, S_REGS, S_LOG)
void draw();
}  // namespace pdui
