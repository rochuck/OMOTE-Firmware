#include "device_kodi.h"

#if (ENABLE_KODI == 1)

#include "applicationInternal/commandHandler.h"

uint16_t KODI_UP;
uint16_t KODI_DOWN;
uint16_t KODI_LEFT;
uint16_t KODI_RIGHT;
uint16_t KODI_SELECT;
uint16_t KODI_BACK;
uint16_t KODI_HOME;
uint16_t KODI_INFO;
uint16_t KODI_CONTEXT_MENU;

uint16_t KODI_PLAY_PAUSE;
uint16_t KODI_STOP;
uint16_t KODI_FAST_FORWARD;
uint16_t KODI_REWIND;
uint16_t KODI_SKIP_NEXT;
uint16_t KODI_SKIP_PREVIOUS;
uint16_t KODI_STEP_FORWARD;
uint16_t KODI_STEP_BACK;

uint16_t KODI_VOLUME_UP;
uint16_t KODI_VOLUME_DOWN;
uint16_t KODI_MUTE_TOGGLE;

uint16_t KODI_ACTION_CUSTOM;

// Helper: build {method, params_json} payload pair.
// Note: params_json must be a complete JSON object string.
void
register_device_kodi(void) {
    // Navigation - https://kodi.wiki/view/JSON-RPC_API#Input
    register_command(&KODI_UP,            makeCommandData(KODI, {"Input.Up",            "{}"}));
    register_command(&KODI_DOWN,          makeCommandData(KODI, {"Input.Down",          "{}"}));
    register_command(&KODI_LEFT,          makeCommandData(KODI, {"Input.Left",          "{}"}));
    register_command(&KODI_RIGHT,         makeCommandData(KODI, {"Input.Right",         "{}"}));
    register_command(&KODI_SELECT,        makeCommandData(KODI, {"Input.Select",        "{}"}));
    register_command(&KODI_BACK,          makeCommandData(KODI, {"Input.Back",          "{}"}));
    register_command(&KODI_HOME,          makeCommandData(KODI, {"Input.Home",          "{}"}));
    register_command(&KODI_INFO,          makeCommandData(KODI, {"Input.Info",          "{}"}));
    register_command(&KODI_CONTEXT_MENU,  makeCommandData(KODI, {"Input.ContextMenu",   "{}"}));

    // Transport - acts on Player.GetActivePlayers()[0]. Using "playerid":1 (video) is a sane default
    // for typical Kodi use; for music you may want playerid:0. Player.PlayPause/Stop accept "to":"toggle".
    register_command(&KODI_PLAY_PAUSE,    makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"playpause\"}"}));
    register_command(&KODI_STOP,          makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"stop\"}"}));
    register_command(&KODI_FAST_FORWARD,  makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"fastforward\"}"}));
    register_command(&KODI_REWIND,        makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"rewind\"}"}));
    register_command(&KODI_SKIP_NEXT,     makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"skipnext\"}"}));
    register_command(&KODI_SKIP_PREVIOUS, makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"skipprevious\"}"}));
    register_command(&KODI_STEP_FORWARD,  makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"stepforward\"}"}));
    register_command(&KODI_STEP_BACK,     makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"stepback\"}"}));

    // Volume
    register_command(&KODI_VOLUME_UP,     makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"volumeup\"}"}));
    register_command(&KODI_VOLUME_DOWN,   makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"volumedown\"}"}));
    register_command(&KODI_MUTE_TOGGLE,   makeCommandData(KODI, {"Input.ExecuteAction", "{\"action\":\"mute\"}"}));

    // Custom: caller supplies the params JSON via additionalPayload to executeCommand()
    register_command(&KODI_ACTION_CUSTOM, makeCommandData(KODI, {"Input.ExecuteAction", "{}"}));
}

#endif // ENABLE_KODI
