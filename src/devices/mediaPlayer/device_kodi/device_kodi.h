#pragma once

#if (ENABLE_KODI == 1)

#include <cstdint>

// Navigation
extern uint16_t KODI_UP;
extern uint16_t KODI_DOWN;
extern uint16_t KODI_LEFT;
extern uint16_t KODI_RIGHT;
extern uint16_t KODI_SELECT;
extern uint16_t KODI_BACK;
extern uint16_t KODI_HOME;
extern uint16_t KODI_INFO;
extern uint16_t KODI_CONTEXT_MENU;

// Transport
extern uint16_t KODI_PLAY_PAUSE;
extern uint16_t KODI_STOP;
extern uint16_t KODI_FAST_FORWARD;
extern uint16_t KODI_REWIND;
extern uint16_t KODI_SKIP_NEXT;
extern uint16_t KODI_SKIP_PREVIOUS;
extern uint16_t KODI_STEP_FORWARD;     // small (default ~30s) jump forward
extern uint16_t KODI_STEP_BACK;        // small jump back

// Volume
extern uint16_t KODI_VOLUME_UP;
extern uint16_t KODI_VOLUME_DOWN;
extern uint16_t KODI_MUTE_TOGGLE;

// Generic: pass JSON-RPC params via additionalPayload to executeCommand().
// Method is fixed to "Input.ExecuteAction" for KODI_ACTION_CUSTOM.
// e.g. executeCommand(KODI_ACTION_CUSTOM, "{\"action\":\"osd\"}");
extern uint16_t KODI_ACTION_CUSTOM;

// Text input. Caller supplies params via additionalPayload, e.g.
// executeCommand(KODI_SEND_TEXT, "{\"text\":\"hello\",\"done\":true}");
extern uint16_t KODI_SEND_TEXT;

void register_device_kodi(void);

#endif // ENABLE_KODI
