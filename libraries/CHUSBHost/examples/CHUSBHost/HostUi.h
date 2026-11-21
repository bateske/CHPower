// HOST mode: the board powers a USB device from its battery and reads it.
// Three views on tabs: a text field for a keyboard, a scope with a cursor
// for a mouse, a controller for a gamepad. Status and power along the
// bottom.
#pragma once
#include <stdint.h>

namespace hostui {
void open(uint8_t tab);     // host mode on (if the charger is out), showing tab 0 keyboard, 1 mouse, 2 gamepad
void tick();
void draw();
}  // namespace hostui
