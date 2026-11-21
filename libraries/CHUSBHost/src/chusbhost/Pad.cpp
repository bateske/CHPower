// The gamepads' shared state (GAMEPAD, XINPUT, SWITCH_PRO): what pad() and
// rawReport() give, linked with the first gamepad driver.
#include "Core.h"

namespace host {
using namespace in;

static Pad padV;
static uint8_t raw[64], rawLen;

const Pad &pad() { return padV; }
Pad &padState() { return padV; }
const uint8_t *rawReport(uint8_t &len) {
    len = rawLen;
    return raw;
}
void padRaw(const uint8_t *r, uint8_t len) {
    memcpy(raw, r, len);
    rawLen = len;
}

static void clear() {
    memset(&padV, 0, sizeof padV);
    padV.hat = 8;
    rawLen = 0;
}
void usePad() { padClear = clear; }

}  // namespace host
