#include "scenes/scene_appleTV.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/keys.h"
#include "applicationInternal/scenes/sceneRegistry.h"
#include <map>
// devices
#include "applicationInternal/commandHandler.h"
#include "devices/AVreceiver/device_marantzAmp/device_marantzAmp.h"
#include "devices/TV/device_sharpTV/device_sharpTV.h"
#include "devices/mediaPlayer/device_appleTV/device_appleTV.h"
// guis
#include "devices/mediaPlayer/device_appleTV/gui_appleTV.h"
#include "guis/gui_numpad.h"
#if (ENABLE_COMPANION == 1)
#include "devices/mediaPlayer/device_appleTV_companion/device_appleTV_companion.h"
#endif

uint16_t SCENE_APPLETV;       //"Scene_appleTV"
uint16_t SCENE_APPLETV_FORCE; //"Scene_appleTV_force"

std::map<char, repeatModes> key_repeatModes_appleTV;
std::map<char, uint16_t>    key_commands_short_appleTV;
std::map<char, uint16_t>    key_commands_long_appleTV;
/* clang-format off */
void
scene_setKeys_appleTV() {
    key_repeatModes_appleTV = {
        {KEY_STOP,  SHORT_REPEATED   },    {KEY_REWI,  SHORT            },    {KEY_PLAY,  SHORT            },    {KEY_FORW,  SHORT_REPEATED   },
        {KEY_CONF,  SHORT            },                                                                          {KEY_INFO,  SHORT            },
                                                             {KEY_UP,    SHORT_REPEATED   },
                          {KEY_LEFT,  SHORT_REPEATED   },    {KEY_OK,    SHORT            },    {KEY_RIGHT, SHORT_REPEATED   },
                                                             {KEY_DOWN,  SHORT_REPEATED   },
                                                                                                                 {KEY_SRC,   SHORT            },
                                                                                                                 {KEY_CHUP,  SHORT            },
                                                                                                                 {KEY_CHDOW, SHORT            },
    };

    key_commands_short_appleTV = {
        {KEY_STOP,  APPLETV_PAUSE             },    {KEY_REWI,  APPLETV_10_SECOND_BACK    },    {KEY_PLAY,  APPLETV_PLAY              },    {KEY_FORW,  APPLETV_10_SECOND_FOREWARD},
        {KEY_CONF,  SHARP_GUIDE               },                                                                                            {KEY_INFO,  APPLETV_MENU              },
                                                                          {KEY_UP,    APPLETV_UP                },
                              {KEY_LEFT,  APPLETV_LEFT              },    {KEY_OK,    APPLETV_OK                },    {KEY_RIGHT, APPLETV_RIGHT             },
                                                                          {KEY_DOWN,  APPLETV_DOWN              },
                                                                                                                                            {KEY_SRC,   APPLETV_HOME              },
                                                                                                                                            {KEY_CHUP,  SHARP_CHANNEL_UP          },
                                                                                                                                            {KEY_CHDOW, SHARP_CHANNEL_DOWN        },
    };

    key_commands_long_appleTV = {

    };
}
/* clang-format on */

void
scene_start_sequence_appleTV(void) {
    for (int i = 0; i < 4; i++) {
        executeCommand(SHARP_POWER_ON);
        delay(10);
    }
    for (int i = 0; i < 4; i++) {
        executeCommand(MARANTZ_POWER_ON);
        delay(10);
    }
    for (int i = 0; i < 4; i++) {
        executeCommand(APPLETV_POWER_ON);
        delay(10);
    }
    delay(1000);
    executeCommand(MARANTZ_INPUT_BD);
    delay(10);
    executeCommand(SHARP_INPUT_HDMI_5);
    delay(12000);
    executeCommand(APPLETV_OK); // Select account
}

void
scene_end_sequence_appleTV(void) {}

std::string scene_name_appleTV     = "Apple TV";
t_gui_list  scene_appleTV_gui_list = {tabName_appleTV};

void
register_scene_appleTV(void) {
    register_command(&SCENE_APPLETV, makeCommandData(SCENE, {scene_name_appleTV}));
    register_command(&SCENE_APPLETV_FORCE, makeCommandData(SCENE, {scene_name_appleTV, "FORCE"}));

    register_scene(scene_name_appleTV,
                   &scene_setKeys_appleTV,
                   &scene_start_sequence_appleTV,
                   &scene_end_sequence_appleTV,
                   &key_repeatModes_appleTV,
                   &key_commands_short_appleTV,
                   &key_commands_long_appleTV,
                   &scene_appleTV_gui_list,
                   SCENE_APPLETV);
}
