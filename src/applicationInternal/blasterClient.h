#pragma once

#include <list>
#include <string>

#if (ENABLE_WIFI_AND_MQTT == 1) && defined(ARDUINO)

#include <Arduino.h>  // for String, used by blaster_getJson

// Call once after WiFi has associated. Tries the cached IP first
// (fast path on every wake), falls back to mDNS browse on miss.
// Safe to call again on WiFi reconnect / periodic re-check.
void blaster_init();

// Periodic re-check (~60 s). Call from the main loop; it's a no-op
// most of the time and only does work on its own schedule.
void blaster_loop();

bool blaster_isEnabled();
bool blaster_isAvailable();

// Forward an IR send to the blaster. Mirrors the call shape of
// sendIRcode_HAL: the first element of payloads is the data string
// (optionally "data:nbits:repeat"); additionalPayload, if non-empty,
// overrides it. Returns true on 2xx; on any failure it flips
// internal state to !available so we re-discover next wake/tick.
bool blaster_send(int protocol,
                  std::list<std::string> payloads,
                  std::string additionalPayload,
                  std::string commandName = "");

// Generic HTTP helpers against the resolved blaster, exposed for the state-sync
// module. Both return the HTTP status code, or <0 on transport failure.
// Caller must keep body alive for the duration of the call.
int  blaster_postJson(const char* path, const char* body);
int  blaster_getJson (const char* path, String& outBody);

#else

inline void blaster_init() {}
inline void blaster_loop() {}
inline bool blaster_isEnabled()   { return false; }
inline bool blaster_isAvailable() { return false; }
inline bool blaster_send(int, std::list<std::string>, std::string, std::string = "") { return false; }
#endif
