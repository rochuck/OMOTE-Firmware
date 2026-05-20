#pragma once

#include <list>
#include <string>

#if (ENABLE_WIFI_AND_MQTT == 1) && defined(ARDUINO)

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

// Push the active scene name to the blaster so its display can show it even
// when no IR is sent (e.g. switching to "Off"). No-op when the blaster isn't
// available. Call from the scene-change chokepoint.
void blaster_notifyScene(const std::string& sceneName);

#else

inline void blaster_init() {}
inline void blaster_loop() {}
inline bool blaster_isEnabled()   { return false; }
inline bool blaster_isAvailable() { return false; }
inline bool blaster_send(int, std::list<std::string>, std::string, std::string = "") { return false; }
inline void blaster_notifyScene(const std::string&) {}

#endif
