/// @file CHUSBHost.h
/// @brief Battery-powered USB host for the CHGame Rev 1.5 board: keyboards,
/// mice and gamepads, on the port or behind a hub (or a hub behind a hub).
///
/// On the CHPower library (the power section: host power on PA0, CC, the
/// PD sink paused while the host has the port), which comes with it:
///
///     pwr::begin();                               // in setup(), first
///     host::begin(host::KEYBOARD | host::MOUSE);  // the drivers to link
///     host::enter();                              // host mode on
///     host::service();                            // from loop()
///
/// Only the drivers named in host::begin() are linked (chusbhost/UsbHost.h
/// has the list; host::ALL is every one). The example is CHUSBHost.
#pragma once
#include <CHPower.h>
#include "chusbhost/UsbHost.h"
