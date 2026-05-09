#include "scenes/scene_kodi_BT.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/keys.h"
#include "applicationInternal/scenes/sceneRegistry.h"
#include <map>
// devices
#include "applicationInternal/commandHandler.h"
#include "devices/AVreceiver/device_marantzAmp/device_marantzAmp.h"
#include "devices/TV/device_sharpTV/device_sharpTV.h"
#if (ENABLE_KEYBOARD_BLE == 1)
#include "devices/keyboard/device_keyboard_ble/device_keyboard_ble.h"
#endif
// guis
#include "guis/gui_kodi_BT.h"

uint16_t SCENE_KODI_BT;       //"Scene_kodi_BT"
uint16_t SCENE_KODI_BT_FORCE; //"Scene_kodi_BT_force"

std::map<char, repeatModes> key_repeatModes_kodi_BT;
std::map<char, uint16_t>    key_commands_short_kodi_BT;
std::map<char, uint16_t>    key_commands_long_kodi_BT;
/* clang-format off */
void
scene_setKeys_kodi_BT() {
    key_repeatModes_kodi_BT = {

        {KEY_STOP,  SHORT            },    {KEY_REWI,  SHORT_REPEATED   },    {KEY_PLAY,  SHORT            },    {KEY_FORW,  SHORT_REPEATED   },
        {KEY_CONF,  SHORT            },                                                                          {KEY_INFO,  SHORT            },
                                                             {KEY_UP,    SHORT_REPEATED   },
                          {KEY_LEFT,  SHORT_REPEATED   },    {KEY_OK,    SHORT            },    {KEY_RIGHT, SHORT_REPEATED   },
                                                             {KEY_DOWN,  SHORT_REPEATED   },
                                                                                                                 {KEY_SRC,   SHORT            },
                                                                                                                 {KEY_CHUP,  SHORT            },
                                                                                                                 {KEY_CHDOW, SHORT            },

    };

#if (ENABLE_KEYBOARD_BLE == 1)
    key_commands_short_kodi_BT = {

        {KEY_STOP,  KEYBOARD_BLE_KEY_X       },    {KEY_REWI,  KEYBOARD_BLE_REWIND      },    {KEY_PLAY,  KEYBOARD_BLE_PLAYPAUSE   },    {KEY_FORW,  KEYBOARD_BLE_FASTFORWARD},
        {KEY_CONF,  KEYBOARD_BLE_KEY_C       },                                                                                            {KEY_INFO,  KEYBOARD_BLE_KEY_I       },
                                                                  {KEY_UP,    KEYBOARD_BLE_UP          },
                            {KEY_LEFT,  KEYBOARD_BLE_LEFT        },    {KEY_OK,    KEYBOARD_BLE_SELECT      },    {KEY_RIGHT, KEYBOARD_BLE_RIGHT       },
                                                                  {KEY_DOWN,  KEYBOARD_BLE_DOWN        },
                                                                                                                                            {KEY_SRC,   KEYBOARD_BLE_KEY_ESC     },
                                                                                                                                            {KEY_CHUP,  KEYBOARD_BLE_VOLUME_INCREMENT },
                                                                                                                                            {KEY_CHDOW, KEYBOARD_BLE_VOLUME_DECREMENT },

    };

    key_commands_long_kodi_BT = {
        {KEY_OK,   KEYBOARD_BLE_KEY_BACKSPACE },
        {KEY_STOP, KEYBOARD_BLE_MUTE          },
    };
#endif
}
/* clang-format on */

void
scene_start_sequence_kodi_BT(void) {
    for (int i = 0; i < 4; i++) {
        executeCommand(SHARP_POWER_ON);
        delay(10);
    }
    for (int i = 0; i < 4; i++) {
        executeCommand(MARANTZ_POWER_ON);
        delay(10);
    }
    delay(1000);
    executeCommand(MARANTZ_INPUT_GAME);
    delay(10);
    executeCommand(SHARP_INPUT_HDMI_1);
}

void
scene_end_sequence_kodi_BT(void) {}

std::string scene_name_kodi_BT    = "Kodi BT";
t_gui_list  scene_kodi_BT_gui_list = {tabName_kodi_BT};

void
register_scene_kodi_BT(void) {
    register_command(&SCENE_KODI_BT, makeCommandData(SCENE, {scene_name_kodi_BT}));
    register_command(&SCENE_KODI_BT_FORCE, makeCommandData(SCENE, {scene_name_kodi_BT, "FORCE"}));

    register_scene(scene_name_kodi_BT,
                   &scene_setKeys_kodi_BT,
                   &scene_start_sequence_kodi_BT,
                   &scene_end_sequence_kodi_BT,
                   &key_repeatModes_kodi_BT,
                   &key_commands_short_kodi_BT,
                   &key_commands_long_kodi_BT,
                   &scene_kodi_BT_gui_list,
                   SCENE_KODI_BT);
}
