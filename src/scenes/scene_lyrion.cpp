#include "scenes/scene_lyrion.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/keys.h"
#include "applicationInternal/scenes/sceneRegistry.h"
#include <map>
// devices
#include "applicationInternal/commandHandler.h"
#include "devices/AVreceiver/device_marantzAmp/device_marantzAmp.h"
#if (ENABLE_LYRION == 1)
#include "devices/mediaPlayer/device_lyrion/device_lyrion.h"
#endif
// guis
#include "guis/gui_lyrion_nowplaying.h"

uint16_t SCENE_LYRION;       //"Scene_lyrion"
uint16_t SCENE_LYRION_FORCE; //"Scene_lyrion_force"

std::map<char, repeatModes> key_repeatModes_lyrion;
std::map<char, uint16_t>    key_commands_short_lyrion;
std::map<char, uint16_t>    key_commands_long_lyrion;
/* clang-format off */
void
scene_setKeys_lyrion() {
    key_repeatModes_lyrion = {

        {KEY_STOP,  SHORT            },    {KEY_REWI,  SHORT_REPEATED   },    {KEY_PLAY,  SHORT            },    {KEY_FORW,  SHORT_REPEATED   },
        {KEY_VOLUP, SHORT_REPEATED   },                                                                          {KEY_MUTE,  SHORT            },
        {KEY_VOLDO, SHORT_REPEATED   },
        // d-pad / OK left unbound in v1; reserved for the future browse tab so the scene-level
        // keymap won't need a tab-aware override when browse is added.
                                                                                                                 {KEY_CHUP,  SHORT            },
                                                                                                                 {KEY_CHDOW, SHORT            },

    };

#if (ENABLE_LYRION == 1)
    key_commands_short_lyrion = {

        {KEY_STOP,  LYRION_STOP            },    {KEY_REWI,  LYRION_PREV         },    {KEY_PLAY,  LYRION_PLAY_PAUSE   },    {KEY_FORW,  LYRION_NEXT         },
        {KEY_VOLUP, LYRION_VOLUME_UP       },                                                                                  {KEY_MUTE,  LYRION_MUTE_TOGGLE  },
        {KEY_VOLDO, LYRION_VOLUME_DOWN     },
                                                                                                                                 {KEY_CHUP,  LYRION_PLAYER_NEXT  },
                                                                                                                                 {KEY_CHDOW, LYRION_PLAYER_PREV  },

    };

    key_commands_long_lyrion = {
        {KEY_STOP, LYRION_POWER_TOGGLE },
    };
#endif
}
/* clang-format on */

void
scene_start_sequence_lyrion(void) {
    // nothing here, lyrion is separate for now, we only control the players.
}

void
scene_end_sequence_lyrion(void) {
#if (ENABLE_LYRION == 1)
    lyrion_powerOffAll_HAL();
#endif
}

std::string scene_name_lyrion = "Lyrion";
// Single-tab gui_list today. To add a library-browse tab later, register a new
// GUI (e.g. gui_lyrion_browse) and append its tabName here:
//   t_gui_list scene_lyrion_gui_list = {tabName_lyrion_nowplaying, tabName_lyrion_browse};
t_gui_list scene_lyrion_gui_list = {tabName_lyrion_nowplaying};

void
register_scene_lyrion(void) {
    register_command(&SCENE_LYRION, makeCommandData(SCENE, {scene_name_lyrion}));
    register_command(&SCENE_LYRION_FORCE, makeCommandData(SCENE, {scene_name_lyrion, "FORCE"}));

    register_scene(scene_name_lyrion,
                   &scene_setKeys_lyrion,
                   &scene_start_sequence_lyrion,
                   &scene_end_sequence_lyrion,
                   &key_repeatModes_lyrion,
                   &key_commands_short_lyrion,
                   &key_commands_long_lyrion,
                   &scene_lyrion_gui_list,
                   SCENE_LYRION);
}
