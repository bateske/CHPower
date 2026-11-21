/// @file CHPower.h
/// @brief The CHGame Rev 1.5 board's power section, in one include.
///
/// - chpower/Power.h (pwr::): the CH211 and the INA226 on their own I2C
///   (CN2's PC18/PC19), PA0 (VBUS sense, and host power), and the protected
///   output CN1 with its cut-off.
/// - chpower/Pd.h (pd::): the USB-PD sink, PD 2.0 and 3.0, fixed and PPS
///   supplies, from WCH's USBPD_SNK example.
/// - chpower/PdLog.h (plog::): what the PD sink and the CH211 did, with times.
///
/// A sketch is linked with what it calls and no more: pwr::begin() is the
/// core; CN1, the PD sink and the log each come in only with their first
/// call (enableCn1()/setLimit()/tele(), pd::start(), plog::begin()). The
/// battery-powered USB host is its own library, CHUSBHost, on this one.
///
/// Rev 1.5 only (a rev1 board): pwr::begin() finds no CH211 on any other
/// board and every power feature is then refused. The design is
/// docs/design.md in this library's repository; the example is CHPDTrigger.
#pragma once
// Rev 1.5 only, which is the rev1 board: on a Rev0 the power section's pins
// are buttons (PC14/PC15) and the H1 header (PA0), so a build for it stops
// here. (The simulator has no board.)
#if !defined(CHSIM) && !defined(ARDUINO_CHGAME_REV1)
#error "CHPower is for the CHGame Rev1 board (Rev 1.5): pick Tools > Board > CHGame Rev1 (board package 0.4.0 or later)"
#endif
#include "chpower/Power.h"
#include "chpower/Pd.h"
#include "chpower/PdLog.h"
