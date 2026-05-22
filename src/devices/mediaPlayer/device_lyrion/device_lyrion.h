#pragma once

#if (ENABLE_LYRION == 1)

#include <cstdint>

// Transport
extern uint16_t LYRION_PLAY_PAUSE;
extern uint16_t LYRION_STOP;
extern uint16_t LYRION_NEXT;
extern uint16_t LYRION_PREV;
extern uint16_t LYRION_POWER_TOGGLE;

// Volume / mute
extern uint16_t LYRION_VOLUME_UP;
extern uint16_t LYRION_VOLUME_DOWN;
extern uint16_t LYRION_MUTE_TOGGLE;

// Player selection (handled directly in commandHandler — no JSON-RPC sent)
extern uint16_t LYRION_PLAYER_NEXT;
extern uint16_t LYRION_PLAYER_PREV;

// Browse-screen navigation (handled directly in commandHandler — drive the
// browse GUI's d-pad navigation, no JSON-RPC sent)
extern uint16_t LYRION_BROWSE_UP;
extern uint16_t LYRION_BROWSE_DOWN;
extern uint16_t LYRION_BROWSE_SELECT;
extern uint16_t LYRION_BROWSE_BACK;

void register_device_lyrion(void);

#endif // ENABLE_LYRION
