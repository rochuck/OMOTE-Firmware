/* this has been hacked to used the hdhomerun app in apple tv*/

#include "scenes/scene_HDHR.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/keys.h"
#include "applicationInternal/scenes/sceneRegistry.h"
#include <map>
// devices
#include "applicationInternal/commandHandler.h"
#include "devices/AVreceiver/device_marantzAmp/device_marantzAmp.h"
#include "devices/TV/device_sharpTV/device_sharpTV.h"
// guis
#include "devices/mediaPlayer/device_appleTV/gui_appleTV.h"
#include "devices/mediaPlayer/device_hdhomerun/gui_hdhomerun.h"
#include "guis/gui_numpad.h"
#include "guis/gui_t9.h"
#if (ENABLE_COMPANION == 1)
#include "devices/mediaPlayer/device_appleTV_companion/device_appleTV_companion.h"
#endif

uint16_t SCENE_TV;       //"Scene_tv"
uint16_t SCENE_TV_FORCE; //"Scene_tv_force"

std::map<char, repeatModes> key_repeatModes_TV;
std::map<char, uint16_t>    key_commands_short_TV;
std::map<char, uint16_t>    key_commands_long_TV;
/* clang-format off */
void
scene_setKeys_TV() {
    key_repeatModes_TV = {

        {KEY_STOP, SHORT_REPEATED},
        {KEY_REWI, SHORT},
        {KEY_PLAY, SHORT},
        {KEY_FORW, SHORT_REPEATED},
        {KEY_CONF, SHORT},
        {KEY_INFO, SHORT},
        {KEY_UP, SHORT_REPEATED},
        {KEY_LEFT, SHORT_REPEATED},
        {KEY_OK, SHORT},
        {KEY_RIGHT, SHORT_REPEATED},
        {KEY_DOWN, SHORT_REPEATED},
        {KEY_SRC, SHORT},
        {KEY_CHUP, SHORT},
        {KEY_CHDOW, SHORT},

    };
/* clang-format on */
    key_commands_short_TV = {

        {KEY_STOP, SHARP_PAUSE},
        {KEY_REWI, SHARP_REWIND},
        {KEY_PLAY, SHARP_PLAY},
        {KEY_FORW, SHARP_FASTFORWARD},
        {KEY_CONF, SHARP_GUIDE},
        {KEY_INFO, SHARP_MENU},
        {KEY_UP, SHARP_UP},
        {KEY_LEFT, SHARP_LEFT},
        {KEY_OK, SHARP_SELECT},
        {KEY_RIGHT, SHARP_RIGHT},
        {KEY_DOWN, SHARP_DOWN},
        {KEY_SRC, SHARP_EXIT},
        {KEY_CHUP, SHARP_CHANNEL_UP},
        {KEY_CHDOW, SHARP_CHANNEL_DOWN},

    };

    key_commands_long_TV = {

    };
}

void
scene_start_sequence_TV(void) {
    executeCommand(SHARP_POWER_ON);
    delay(100);
    executeCommand(MARANTZ_POWER_ON);
    delay(1000);
    executeCommand(MARANTZ_INPUT_DVD);
    delay(100);
    executeCommand(SHARP_INPUT_HDMI_5);
    delay(100);
#if (ENABLE_COMPANION == 1)
    executeCommand(COMPANION_LAUNCH_HDHOMERUN);
#endif
}

void
scene_end_sequence_TV(void) {}

std::string scene_name_TV = "TV HDHOMERUN";
// t_gui_list scene_TV_gui_list = {tabName_hdhomerun};
t_gui_list scene_TV_gui_list = {tabName_t9};

void
register_scene_TV(void) {
    register_command(&SCENE_TV, makeCommandData(SCENE, {scene_name_TV}));
    register_command(&SCENE_TV_FORCE, makeCommandData(SCENE, {scene_name_TV, "FORCE"}));

    register_scene(scene_name_TV,
                   &scene_setKeys_TV,
                   &scene_start_sequence_TV,
                   &scene_end_sequence_TV,
                   &key_repeatModes_TV,
                   &key_commands_short_TV,
                   &key_commands_long_TV,
                   &scene_TV_gui_list,
                   SCENE_TV);
}
