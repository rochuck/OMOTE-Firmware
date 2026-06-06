#include "clockTime.h"

// ENABLE_WIFI_AND_MQTT comes from a -D build flag (see platformio.ini).
#if (ENABLE_WIFI_AND_MQTT == 1) && defined(ARDUINO)

#include <Arduino.h>
#include <cstdlib>

namespace {
// America/Edmonton (Mountain Time): MST (UTC-7) with MDT DST on the US schedule.
const char* kTZ = "MST7MDT,M3.2.0,M11.1.0";

bool          s_valid     = false;
time_t        s_baseEpoch = 0;       // UTC epoch from the blaster
unsigned long s_baseMs    = 0;       // millis() when s_baseEpoch was received
}  // namespace

void clockTime_begin() {
    setenv("TZ", kTZ, 1);
    tzset();
}

void clockTime_setBaseEpoch(time_t utcEpoch) {
    s_baseEpoch = utcEpoch;
    s_baseMs    = millis();
    s_valid     = true;
}

bool clockTime_valid() {
    return s_valid;
}

bool clockTime_formatHHMM(char* buf, size_t n) {
    if (!s_valid) return false;
    time_t now = s_baseEpoch + (time_t)((millis() - s_baseMs) / 1000UL);
    struct tm lt;
    localtime_r(&now, &lt);
    strftime(buf, n, "%H:%M", &lt);
    return true;
}

#else  // simulator / non-WiFi builds: no clock source, stub out.

void clockTime_begin() {}
void clockTime_setBaseEpoch(time_t) {}
bool clockTime_valid() { return false; }
bool clockTime_formatHHMM(char*, size_t) { return false; }

#endif  // ENABLE_WIFI_AND_MQTT && ARDUINO
