#include "scenes/scene_kodi.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/keys.h"
#include "applicationInternal/scenes/sceneRegistry.h"
#include <map>
// devices
#include "applicationInternal/commandHandler.h"
#include "devices/AVreceiver/device_marantzAmp/device_marantzAmp.h"
#include "devices/TV/device_sharpTV/device_sharpTV.h"
#if (ENABLE_KODI == 1)
#include "devices/mediaPlayer/device_kodi/device_kodi.h"
#endif

uint16_t SCENE_KODI;       //"Scene_kodi"
uint16_t SCENE_KODI_FORCE; //"Scene_kodi_force"

std::map<char, repeatModes> key_repeatModes_kodi;
std::map<char, uint16_t>    key_commands_short_kodi;
std::map<char, uint16_t>    key_commands_long_kodi;
/* clang-format off */
void
scene_setKeys_kodi() {
    key_repeatModes_kodi = {

        {KEY_STOP,  SHORT            },    {KEY_REWI,  SHORT_REPEATED   },    {KEY_PLAY,  SHORT            },    {KEY_FORW,  SHORT_REPEATED   },
        {KEY_CONF,  SHORT            },                                                                          {KEY_INFO,  SHORT            },
                                                             {KEY_UP,    SHORT_REPEATED   },
                          {KEY_LEFT,  SHORT_REPEATED   },    {KEY_OK,    SHORT            },    {KEY_RIGHT, SHORT_REPEATED   },
                                                             {KEY_DOWN,  SHORT_REPEATED   },
                                                                                                                 {KEY_SRC,   SHORT            },
                                                                                                                 {KEY_CHUP,  SHORT            },
                                                                                                                 {KEY_CHDOW, SHORT            },

    };

#if (ENABLE_KODI == 1)
    key_commands_short_kodi = {

        {KEY_STOP,  KODI_STOP                },    {KEY_REWI,  KODI_REWIND              },    {KEY_PLAY,  KODI_PLAY_PAUSE          },    {KEY_FORW,  KODI_FAST_FORWARD        },
        {KEY_CONF,  KODI_CONTEXT_MENU        },                                                                                            {KEY_INFO,  KODI_INFO                },
                                                                  {KEY_UP,    KODI_UP                  },
                            {KEY_LEFT,  KODI_LEFT              },    {KEY_OK,    KODI_SELECT              },    {KEY_RIGHT, KODI_RIGHT             },
                                                                  {KEY_DOWN,  KODI_DOWN                },
                                                                                                                                            {KEY_SRC,   KODI_HOME                },
                                                                                                                                            {KEY_CHUP,  KODI_VOLUME_UP           },
                                                                                                                                            {KEY_CHDOW, KODI_VOLUME_DOWN         },

    };

    key_commands_long_kodi = {
        {KEY_OK,   KODI_BACK },
        {KEY_STOP, KODI_MUTE_TOGGLE },
    };
#endif
}
/* clang-format on */

void
scene_start_sequence_kodi(void) {
    executeCommand(SHARP_POWER_ON);
    delay(10);
    executeCommand(MARANTZ_POWER_ON);
    delay(1000);
    executeCommand(MARANTZ_INPUT_DVD);
    delay(10);
    executeCommand(SHARP_INPUT_HDMI_2);
}

void
scene_end_sequence_kodi(void) {}

std::string scene_name_kodi = "Kodi";

void
register_scene_kodi(void) {
    register_command(&SCENE_KODI, makeCommandData(SCENE, {scene_name_kodi}));
    register_command(&SCENE_KODI_FORCE, makeCommandData(SCENE, {scene_name_kodi, "FORCE"}));

    register_scene(scene_name_kodi,
                   &scene_setKeys_kodi,
                   &scene_start_sequence_kodi,
                   &scene_end_sequence_kodi,
                   &key_repeatModes_kodi,
                   &key_commands_short_kodi,
                   &key_commands_long_kodi,
                   NULL,
                   SCENE_KODI);
}
