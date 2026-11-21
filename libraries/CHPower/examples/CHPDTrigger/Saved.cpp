// Saved.h.
#include <CHGame.h>
#include <CHPower.h>
#include <string.h>
#include "Saved.h"

namespace saved {

struct Data {
    uint16_t mv, limit;
    uint8_t logN;
    plog::Ev log[plog::KEEP];
};
static_assert(sizeof(Data) <= save::MAX_DATA, "one save page");
static const uint32_t MAGIC = save::magic("CHPD");
static const uint8_t VERSION = 3;     // (2 had a VBUS check setting)
static Data d;
static bool dirtyFlag;

void begin() {
    if (!save::load(MAGIC, VERSION, d)) {
        memset(&d, 0, sizeof d);
        d.mv = 5000;
        d.limit = 1000;
    }
    plog::setLastRun(d.log, d.logN);
}
bool dirty() { return dirtyFlag; }
void touch() { dirtyFlag = true; }
void save() {
    save::store(MAGIC, VERSION, d);
    dirtyFlag = false;
}
void keepLog() {
    d.logN = plog::tail(d.log);
    save();
}
uint16_t &voltsMv() { return d.mv; }
uint16_t &limitMa() { return d.limit; }

}  // namespace saved
