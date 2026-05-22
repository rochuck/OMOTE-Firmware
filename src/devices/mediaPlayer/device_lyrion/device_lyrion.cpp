#include "device_lyrion.h"

#if (ENABLE_LYRION == 1)

#include "applicationInternal/commandHandler.h"

uint16_t LYRION_PLAY_PAUSE;
uint16_t LYRION_STOP;
uint16_t LYRION_NEXT;
uint16_t LYRION_PREV;
uint16_t LYRION_POWER_TOGGLE;

uint16_t LYRION_VOLUME_UP;
uint16_t LYRION_VOLUME_DOWN;
uint16_t LYRION_MUTE_TOGGLE;

uint16_t LYRION_PLAYER_NEXT;
uint16_t LYRION_PLAYER_PREV;

uint16_t LYRION_BROWSE_UP;
uint16_t LYRION_BROWSE_DOWN;
uint16_t LYRION_BROWSE_SELECT;
uint16_t LYRION_BROWSE_BACK;

// LMS command-array payloads (slim.request format).
// Internal sentinels (__lyrion_*) are intercepted in commandHandler.cpp and
// dispatched to dedicated HAL functions instead of being POSTed.
void
register_device_lyrion(void) {
    register_command(&LYRION_PLAY_PAUSE,    makeCommandData(LYRION, {"[\"pause\"]"}));
    register_command(&LYRION_STOP,          makeCommandData(LYRION, {"[\"stop\"]"}));
    register_command(&LYRION_NEXT,          makeCommandData(LYRION, {"[\"playlist\",\"index\",\"+1\"]"}));
    register_command(&LYRION_PREV,          makeCommandData(LYRION, {"[\"playlist\",\"index\",\"-1\"]"}));
    register_command(&LYRION_POWER_TOGGLE,  makeCommandData(LYRION, {"__lyrion_power_toggle__"}));

    register_command(&LYRION_VOLUME_UP,     makeCommandData(LYRION, {"[\"mixer\",\"volume\",\"+5\"]"}));
    register_command(&LYRION_VOLUME_DOWN,   makeCommandData(LYRION, {"[\"mixer\",\"volume\",\"-5\"]"}));
    register_command(&LYRION_MUTE_TOGGLE,   makeCommandData(LYRION, {"[\"mixer\",\"muting\",\"toggle\"]"}));

    register_command(&LYRION_PLAYER_NEXT,   makeCommandData(LYRION, {"__lyrion_player_next__"}));
    register_command(&LYRION_PLAYER_PREV,   makeCommandData(LYRION, {"__lyrion_player_prev__"}));

    register_command(&LYRION_BROWSE_UP,     makeCommandData(LYRION, {"__lyrion_browse_up__"}));
    register_command(&LYRION_BROWSE_DOWN,   makeCommandData(LYRION, {"__lyrion_browse_down__"}));
    register_command(&LYRION_BROWSE_SELECT, makeCommandData(LYRION, {"__lyrion_browse_select__"}));
    register_command(&LYRION_BROWSE_BACK,   makeCommandData(LYRION, {"__lyrion_browse_back__"}));
}

#endif // ENABLE_LYRION
