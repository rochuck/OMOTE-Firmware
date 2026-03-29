#include "device_appleTV_companion.h"

#if (ENABLE_COMPANION == 1)

#include "applicationInternal/commandHandler.h"

uint16_t COMPANION_LAUNCH_NETFLIX;
uint16_t COMPANION_LAUNCH_YOUTUBE;
uint16_t COMPANION_LAUNCH_DISNEYPLUS;
uint16_t COMPANION_LAUNCH_APPLETV_PLUS;
uint16_t COMPANION_LAUNCH_PRIMEVIDEO;
uint16_t COMPANION_LAUNCH_HBO_MAX;
uint16_t COMPANION_LAUNCH_HULU;
uint16_t COMPANION_LAUNCH_SPOTIFY;
uint16_t COMPANION_LAUNCH_PLEX;
uint16_t COMPANION_LAUNCH_CUSTOM;

void register_device_appleTV_companion(void) {
    register_command(&COMPANION_LAUNCH_NETFLIX,
        makeCommandData(COMPANION, {"launch", "com.netflix.Netflix"}));

    register_command(&COMPANION_LAUNCH_YOUTUBE,
        makeCommandData(COMPANION, {"launch", "com.google.ios.youtube"}));

    register_command(&COMPANION_LAUNCH_DISNEYPLUS,
        makeCommandData(COMPANION, {"launch", "com.disney.disneyplus"}));

    register_command(&COMPANION_LAUNCH_APPLETV_PLUS,
        makeCommandData(COMPANION, {"launch", "com.apple.TVShows"}));

    register_command(&COMPANION_LAUNCH_PRIMEVIDEO,
        makeCommandData(COMPANION, {"launch", "com.amazon.aiv.AIVApp"}));

    register_command(&COMPANION_LAUNCH_HBO_MAX,
        makeCommandData(COMPANION, {"launch", "com.hbo.hbonow"}));

    register_command(&COMPANION_LAUNCH_HULU,
        makeCommandData(COMPANION, {"launch", "com.hulu.plus"}));

    register_command(&COMPANION_LAUNCH_SPOTIFY,
        makeCommandData(COMPANION, {"launch", "com.spotify.client"}));

    register_command(&COMPANION_LAUNCH_PLEX,
        makeCommandData(COMPANION, {"launch", "com.plexapp.plex"}));

    // Custom: pass bundle ID via additionalPayload
    // e.g.: executeCommand(COMPANION_LAUNCH_CUSTOM, "com.example.myapp")
    register_command(&COMPANION_LAUNCH_CUSTOM,
        makeCommandData(COMPANION, {"launch", ""}));
}

#endif // ENABLE_COMPANION
