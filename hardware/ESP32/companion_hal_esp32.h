#pragma once

#if (ENABLE_COMPANION == 1)

#include <string>

// Initialize the Companion HAL and start the background FreeRTOS task.
// Must be called after WiFi is connected (init_mqtt).
void init_companion_HAL(void);

// Enqueue a launch-app command (by bundle ID). Thread-safe.
bool companion_launchApp_HAL(const std::string& bundleID);

// Enqueue a "force quit foreground app" macro: double-press Home to open the
// app switcher, then swipe up on the highlighted card. Thread-safe.
bool companion_killApp_HAL(void);

// Returns true if currently authenticated and session is alive.
bool companion_isConnected_HAL(void);

// Stop the background task. Intended for use just before a reboot (e.g. OTA),
// no resume path. The task is killed via vTaskDelete, which does not run
// C++ destructors — acceptable here because cleanup is moot before reset.
void companion_shutdown_HAL(void);

#endif // ENABLE_COMPANION
