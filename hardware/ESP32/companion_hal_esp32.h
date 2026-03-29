#pragma once

#if (ENABLE_COMPANION == 1)

#include <string>

// Initialize the Companion HAL and start the background FreeRTOS task.
// Must be called after WiFi is connected (init_mqtt).
void init_companion_HAL(void);

// Enqueue a launch-app command (by bundle ID). Thread-safe.
bool companion_launchApp_HAL(const std::string& bundleID);

// Returns true if currently authenticated and session is alive.
bool companion_isConnected_HAL(void);

#endif // ENABLE_COMPANION
