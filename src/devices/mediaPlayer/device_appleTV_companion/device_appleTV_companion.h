#pragma once

#if (ENABLE_COMPANION == 1)

#include <cstdint>

// Launch specific apps on Apple TV via the Companion protocol.
// Bundle IDs can be found by running:
//   atvremote --id <ATV_ID> --protocol companion app_list

extern uint16_t COMPANION_LAUNCH_NETFLIX;
extern uint16_t COMPANION_LAUNCH_YOUTUBE;
extern uint16_t COMPANION_LAUNCH_DISNEYPLUS;
extern uint16_t COMPANION_LAUNCH_APPLETV_PLUS;
extern uint16_t COMPANION_LAUNCH_PRIMEVIDEO;
extern uint16_t COMPANION_LAUNCH_HBO_MAX;
extern uint16_t COMPANION_LAUNCH_HULU;
extern uint16_t COMPANION_LAUNCH_SPOTIFY;
extern uint16_t COMPANION_LAUNCH_PLEX;
// Generic launcher: pass bundle ID via additionalPayload to executeCommand()
extern uint16_t COMPANION_LAUNCH_CUSTOM;

void register_device_appleTV_companion(void);

#endif // ENABLE_COMPANION
