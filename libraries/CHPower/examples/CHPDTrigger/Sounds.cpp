// The effects (chgame/Audio.h: start Hz, end Hz (0: hold), ms). Move, Open,
// Back, Mode, Ready and Deny are the agent apps' own.
#include "Sounds.h"

AUDIO_STEPS(S_MOVE)   = { AUDIO_STEP(2400, 0, 6) };
AUDIO_STEPS(S_OPEN)   = { AUDIO_STEP(1800, 0, 30), AUDIO_REST(10), AUDIO_STEP(2700, 0, 50) };
AUDIO_STEPS(S_BACK)   = { AUDIO_STEP(2700, 0, 30), AUDIO_REST(10), AUDIO_STEP(1800, 0, 40) };
AUDIO_STEPS(S_MODE)   = { AUDIO_STEP(2000, 0, 20), AUDIO_REST(10), AUDIO_STEP(2400, 0, 20), AUDIO_REST(10),
                          AUDIO_STEP(3000, 0, 40) };
AUDIO_STEPS(S_READY)  = { AUDIO_STEP(1600, 3200, 90), AUDIO_REST(30), AUDIO_STEP(3200, 0, 30),
                          AUDIO_REST(20), AUDIO_STEP(3200, 0, 30) };
AUDIO_STEPS(S_DENY)   = { AUDIO_STEP(320, 0, 110), AUDIO_REST(40), AUDIO_STEP(240, 0, 180) };
// The output's relay: a thunk and a pip going on, the pip falling going off.
AUDIO_STEPS(S_OUTON)  = { AUDIO_STEP(700, 0, 12), AUDIO_REST(25), AUDIO_STEP(2400, 3600, 40) };
AUDIO_STEPS(S_OUTOFF) = { AUDIO_STEP(3600, 1600, 50), AUDIO_REST(15), AUDIO_STEP(600, 0, 12) };
// The cut-off: a two-tone alarm, three times.
AUDIO_STEPS(S_TRIP)   = { AUDIO_STEP(3200, 0, 70), AUDIO_STEP(2200, 0, 70), AUDIO_STEP(3200, 0, 70),
                          AUDIO_STEP(2200, 0, 70), AUDIO_STEP(3200, 0, 70), AUDIO_STEP(2200, 0, 120) };
AUDIO_STEPS(S_KNOB)   = { AUDIO_STEP(3000, 0, 3) };
AUDIO_STEPS(S_KEY)    = { AUDIO_STEP(2600, 0, 4) };
AUDIO_STEPS(S_CLICK)  = { AUDIO_STEP(1800, 0, 5) };
AUDIO_STEPS(S_PLUG)   = { AUDIO_STEP(1200, 2400, 60), AUDIO_REST(20), AUDIO_STEP(3000, 0, 30) };
AUDIO_STEPS(S_UNPLUG) = { AUDIO_STEP(2400, 1200, 60), AUDIO_REST(20), AUDIO_STEP(900, 0, 30) };
// Done: G B D G (CHSDtoSerial's success).
AUDIO_STEPS(S_DONE)   = { AUDIO_STEP(1568, 0, 50), AUDIO_STEP(1976, 0, 50), AUDIO_STEP(2349, 0, 50),
                          AUDIO_STEP(3136, 0, 110) };

static const audio::Effect SOUNDS[] = {
    AUDIO_EFFECT(S_MOVE, 0 | audio::SOFT), AUDIO_EFFECT(S_OPEN, 2),   AUDIO_EFFECT(S_BACK, 2),
    AUDIO_EFFECT(S_MODE, 2),               AUDIO_EFFECT(S_READY, 3),  AUDIO_EFFECT(S_DENY, 4),
    AUDIO_EFFECT(S_OUTON, 4),              AUDIO_EFFECT(S_OUTOFF, 4), AUDIO_EFFECT(S_TRIP, 6),
    AUDIO_EFFECT(S_KNOB, 0 | audio::SOFT), AUDIO_EFFECT(S_KEY, 1 | audio::SOFT),
    AUDIO_EFFECT(S_CLICK, 1 | audio::SOFT), AUDIO_EFFECT(S_PLUG, 3),  AUDIO_EFFECT(S_UNPLUG, 3),
    AUDIO_EFFECT(S_DONE, 5),
};

void soundsBegin() { audio::begin(SOUNDS, (uint8_t)Sfx::COUNT); }
