#include "scenes/scene_appleTV.h"
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
#if (ENABLE_COMPANION == 1)
#include "devices/mediaPlayer/device_appleTV_companion/device_appleTV_companion.h"
#endif

uint16_t SCENE_APPLETV;       //"Scene_appleTV"
uint16_t SCENE_APPLETV_FORCE; //"Scene_appleTV_force"

std::map<char, repeatModes> key_repeatModes_appleTV;
std::map<char, uint16_t>    key_commands_short_appleTV;
std::map<char, uint16_t>    key_commands_long_appleTV;

void
scene_setKeys_appleTV() {
    key_repeatModes_appleTV = {

    };

    key_commands_short_appleTV = {

    };

    key_commands_long_appleTV = {

    };
}

void
scene_start_sequence_appleTV(void) {
    executeCommand(SHARP_POWER_ON);
    delay(500);
    executeCommand(MARANTZ_POWER_ON);
    delay(1500);
    executeCommand(MARANTZ_INPUT_DVD);
    delay(3000);
    executeCommand(SHARP_INPUT_HDMI_5);
    delay(2000);
#if (ENABLE_COMPANION == 1)
    executeCommand(COMPANION_LAUNCH_NETFLIX);
#endif
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
