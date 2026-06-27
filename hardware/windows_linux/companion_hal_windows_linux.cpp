#include "companion_hal_windows_linux.h"

#if (ENABLE_COMPANION == 1)

// Stub implementations for the desktop simulator.
// The Companion protocol requires a real Apple TV on the network, so these
// are intentional no-ops in the simulator.

void init_companion_HAL(void) {}
bool companion_launchApp_HAL(const std::string& /*bundleID*/) { return false; }
bool companion_killApp_HAL(void) { return false; }
bool companion_isConnected_HAL(void) { return false; }
void companion_shutdown_HAL(void) {}

#endif // ENABLE_COMPANION
