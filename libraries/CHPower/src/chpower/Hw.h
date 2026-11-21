// Small hardware helpers shared by Power, Pd and UsbHost.
#pragma once
#include <Arduino.h>

// Rev 1.5 only, which is the rev1 board: on a Rev0 the power section's pins
// are buttons (PC14/PC15) and the H1 header (PA0), so a build for it stops
// here. (The simulator has no board.)
#if !defined(CHSIM) && !defined(ARDUINO_CHGAME_REV1)
#error "CHPower is for the CHGame Rev1 board (Rev 1.5): pick Tools > Board > CHGame Rev1 (board package 0.4.0 or later)"
#endif

#ifndef CHSIM
// A busy wait of up to about 1 ms from SysTick, which counts up to CMP and
// starts again (the core's millis() tick). Usable in an interrupt, unlike
// delayMicroseconds() (64-bit arithmetic) where time matters.
static inline void waitUs(uint32_t us) {
    uint32_t per = (uint32_t)SysTick->CMP + 1, n = us * (per / 1000u), e = 0;
    uint32_t o = (uint32_t)SysTick->CNT;
    while (e < n) {
        uint32_t c = (uint32_t)SysTick->CNT;
        e += c >= o ? c - o : per - o + c;
        o = c;
    }
}
#else
static inline void waitUs(uint32_t) {}
#endif
