#pragma once

#if (ENABLE_COMPANION == 1)

#include <string>

void init_companion_HAL(void);
bool companion_launchApp_HAL(const std::string& bundleID);
bool companion_isConnected_HAL(void);
void companion_shutdown_HAL(void);

#endif // ENABLE_COMPANION
