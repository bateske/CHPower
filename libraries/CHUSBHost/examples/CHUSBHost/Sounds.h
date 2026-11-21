// The instrument's beeps: short, high and clean, as the other apps' (the
// agent house: CHStlView, CHSDtoUSB), plus the bench sounds: the output's
// relay, the knob's detents, the trip alarm, keys and clicks.
#pragma once
#include <CHGame.h>

enum class Sfx : uint8_t {
    Move, Open, Back, Mode, Ready, Deny, OutOn, OutOff, Trip, Knob, Key, Click, Plug, Unplug, Done, COUNT
};

void soundsBegin();
inline void sfx(Sfx s) { audio::sfx((uint8_t)s); }
