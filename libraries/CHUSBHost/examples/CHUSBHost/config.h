// CHUSBHost's version, prefix and ids. CHUH is this sketch's prefix, its
// debug hello ("CHUH") and its save magic (the repository's CLAUDE.md,
// rule 7: no other sketch uses them).
#pragma once
#define CHUH_VERSION "0.2"

// Rev 1.5 hardware only. A Rev 1.5 is the rev1 board (the IDE's CHGame
// Rev1), so this cannot tell a Rev 1 from a 1.5: the CHPower library checks
// for the CH211 and the INA226 at start-up and refuses every power feature
// without them. On a Rev0 it does not build.
#if !defined(ARDUINO_CHGAME_REV1) && !defined(CHSIM)
#error "CHUSBHost is for the Rev 1.5 board: build it for CHGame Rev1 (chgame --board rev1)"
#endif
