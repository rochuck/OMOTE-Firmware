#include "device_sharpTV.h"
#include "applicationInternal/commandHandler.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include <string>

// Only activate the commands that are used. Every command takes 100 bytes, wether used or not.
uint16_t SHARP_NUM_1; //"Sharp_num_1";
uint16_t SHARP_NUM_2; //"Sharp_num_2";
uint16_t SHARP_NUM_3; //"Sharp_num_3";
uint16_t SHARP_NUM_4; //"Sharp_num_4";
uint16_t SHARP_NUM_5; //"Sharp_num_5";
uint16_t SHARP_NUM_6; //"Sharp_num_6";
uint16_t SHARP_NUM_7; //"Sharp_num_7";
uint16_t SHARP_NUM_8; //"Sharp_num_8";
uint16_t SHARP_NUM_9; //"Sharp_num_9";
uint16_t SHARP_NUM_0; //"Sharp_num_0";
// uint16_t SHARP_TTXMIX          ; //"Sharp_ttxmix";
// uint16_t SHARP_PRECH           ; //"Sharp_prech";
// uint16_t SHARP_VOL_MINUS       ; //"Sharp_vol_minus";
// uint16_t SHARP_VOL_PLUS        ; //"Sharp_vol_plus";
// uint16_t SHARP_MUTE_TOGGLE     ; //"Sharp_mute_toggle";
// uint16_t SHARP_CHLIST          ; //"Sharp_chlist";
uint16_t SHARP_CHANNEL_UP;   //"Sharp_channel_up";
uint16_t SHARP_CHANNEL_DOWN; //"Sharp_channel_down";
uint16_t SHARP_MENU;         //"Sharp_menu";
// uint16_t SHARP_APPS            ; //"Sharp_apps";
uint16_t SHARP_GUIDE; //"Sharp_guide";
// uint16_t SHARP_TOOLS           ; //"Sharp_tools";
// uint16_t SHARP_INFO            ; //"Sharp_info";
uint16_t SHARP_UP;     //"Sharp_up";
uint16_t SHARP_DOWN;   //"Sharp_down";
uint16_t SHARP_LEFT;   //"Sharp_left";
uint16_t SHARP_RIGHT;  //"Sharp_right";
uint16_t SHARP_SELECT; //"Sharp_select";
// uint16_t SHARP_RETURN          ; //"Sharp_return";
uint16_t SHARP_EXIT; //"Sharp_exit";
// uint16_t SHARP_KEY_A           ; //"Sharp_key_a";
// uint16_t SHARP_KEY_B           ; //"Sharp_key_b";
// uint16_t SHARP_KEY_C           ; //"Sharp_key_c";
// uint16_t SHARP_KEY_D           ; //"Sharp_key_d";
// uint16_t SHARP_FAMILYSTORY     ; //"Sharp_familystory";
// uint16_t SHARP_SEARCH          ; //"Sharp_search";
// uint16_t SHARP_DUALI_II        ; //"Sharp_duali-ii";
// uint16_t SHARP_SUPPORT         ; //"Sharp_support";
// uint16_t SHARP_PSIZE           ; //"Sharp_psize";
// uint16_t SHARP_ADSUBT          ; //"Sharp_adsubt";
uint16_t SHARP_REWIND;      //"Sharp_rewind";
uint16_t SHARP_PAUSE;       //"Sharp_pause";
uint16_t SHARP_FASTFORWARD; //"Sharp_fastforward";
// uint16_t SHARP_RECORD          ; //"Sharp_record";
uint16_t SHARP_PLAY; //"Sharp_play";
// uint16_t SHARP_STOP            ; //"Sharp_stop";
uint16_t SHARP_POWER_OFF;    //"Sharp_power_off";
uint16_t SHARP_POWER_ON;     //"Sharp_power_on";
uint16_t SHARP_INPUT_HDMI_1; //"Sharp_input_hdmi_1";
uint16_t SHARP_INPUT_HDMI_2; //"Sharp_input_hdmi_2";
uint16_t SHARP_INPUT_HDMI_3; //"Sharp_input_hdmi_3";
uint16_t SHARP_INPUT_HDMI_4; //"Sharp_input_hdmi_4";
uint16_t SHARP_INPUT_HDMI_5; //"Sharp_input_hdmi_5";

// uint16_t SHARP_INPUT_COMPONENT ; //"Sharp_input_component";
uint16_t SHARP_INPUT_TV; //"Sharp_input_tv";

void
register_device_sharpTV() {
    // IR codes for Sharp LC-52D65U (Sharp Aquos), remote model 845-039-40B0
    // protocol=Sharp (15-bit), encoded as encodeSharp(device, function, expansion=1, check=0, MSBfirst=true)
    //   = (device << 10) | (function << 2) | 0x2
    // All codes use device=16. Note: Sharp protocol only has a single power toggle (no separate on/off).
    // Commands marked PLACEHOLDER need to be captured with the OMOTE IR receiver.

    // Only activate the commands that are used. Every command takes 100 bytes, wether used or not.

    // Digit keys: device=16
    register_command(&SHARP_NUM_1, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4202"})); // device=16, fn=128
    register_command(&SHARP_NUM_2, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4102"})); // device=16, fn=64
    register_command(&SHARP_NUM_3, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4302"})); // device=16, fn=192
    register_command(&SHARP_NUM_4, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4082"})); // device=16, fn=32
    register_command(&SHARP_NUM_5, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4282"})); // device=16, fn=160
    register_command(&SHARP_NUM_6, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4182"})); // device=16, fn=96
    register_command(&SHARP_NUM_7, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4382"})); // device=16, fn=224
    register_command(&SHARP_NUM_8, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4042"})); // device=16, fn=16
    register_command(&SHARP_NUM_9, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4242"})); // device=16, fn=144
    register_command(&SHARP_NUM_0, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4142"})); // device=16, fn=80

    // Channel, input
    register_command(&SHARP_CHANNEL_UP, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4222"}));   // device=16, fn=136
    register_command(&SHARP_CHANNEL_DOWN, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4122"})); // device=16, fn=72
    register_command(&SHARP_INPUT_TV,
                     makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4322"})); // device=16, fn=200 (input toggle)

    // Power (toggle only - Sharp has no separate on/off)
    register_command(&SHARP_POWER_ON,
                     makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x41A2"})); // device=16, fn=104
    register_command(&SHARP_POWER_OFF, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x41A2"})); // toggle (same code)

    // Navigation
    register_command(&SHARP_UP, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x43AA"}));     // device=16, fn=234
    register_command(&SHARP_DOWN, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x406A"}));   // device=16, fn=26
    register_command(&SHARP_LEFT, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x42BE"}));   // device=16, fn=175
    register_command(&SHARP_RIGHT, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x41BE"}));  // device=16, fn=111
    register_command(&SHARP_SELECT, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x43BE"})); // device=16, fn=239
    register_command(&SHARP_EXIT, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x433E"}));   // device=16, fn=207
    register_command(&SHARP_MENU, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4012"}));   // device=16, fn=4

    // No code found for these — capture with OMOTE IR receiver
    register_command(&SHARP_GUIDE, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4202"}));  // PLACEHOLDER - unknown
    register_command(&SHARP_REWIND, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4202"})); // PLACEHOLDER - unknown
    register_command(&SHARP_PAUSE, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4202"}));  // PLACEHOLDER - unknown
    register_command(&SHARP_FASTFORWARD,
                     makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4202"}));              // PLACEHOLDER - unknown
    register_command(&SHARP_PLAY, makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4202"})); // PLACEHOLDER - unknown

    // HDMI inputs — capture with OMOTE IR receiver
    register_command(&SHARP_INPUT_HDMI_1,
                     makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4202"})); // PLACEHOLDER - unknown
    register_command(&SHARP_INPUT_HDMI_2,
                     makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4202"})); // PLACEHOLDER - unknown
    register_command(&SHARP_INPUT_HDMI_3,
                     makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x4202"})); // PLACEHOLDER - unknown
    register_command(&SHARP_INPUT_HDMI_5,
                     makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0x44CA"})); // device=16, fn=50 (tentative)
}
