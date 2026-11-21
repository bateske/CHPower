// What is kept in flash: the trigger's settings, and the PD log's last
// events (the CHPower library's plog::tail()) for the run after. One save
// page's worth, under the magic "CHPD".
#pragma once
#include <stdint.h>

namespace saved {

void begin();               // read (defaults if there is nothing); the last run's log to plog::
bool dirty();
void touch();               // a setting changed: save at the next quiet moment
void save();                // after gfx_wait() (the save borrows the panel's chunk scratch)
void keepLog();             // this run's last PD events, then save()

uint16_t &voltsMv();        // the voltage the trigger asks for
uint16_t &limitMa();        // CN1's trip limit

}  // namespace saved
