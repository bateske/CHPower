// The guided tour: short walkthroughs that show how to use the board's
// power features, each a few steps that watch for what the user does (a
// charger plugged in, the output switched on, a trip) and say what they
// saw. It replaces the bench test's PASS/FAIL list; every check it made is
// still here, told as what happened.
#pragma once
#include <stdint.h>

namespace guide {
void tick();                // S_GUIDES (the list) and S_GUIDE (a walkthrough)
void draw();
}  // namespace guide
