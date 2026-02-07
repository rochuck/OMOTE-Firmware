#include <string>
#include "applicationInternal/commandHandler.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "device_sharpTV.h"

// Only activate the commands that are used. Every command takes 100 bytes, wether used or not.
// uint16_t SHARP_POWER_TOGGLE    ; //"Sharp_power_toggle";
// uint16_t SHARP_SOURCE          ; //"Sharp_source";
// uint16_t SHARP_HDMI            ; //"Sharp_hdmi";
uint16_t SHARP_NUM_1           ; //"Sharp_num_1";
uint16_t SHARP_NUM_2           ; //"Sharp_num_2";
uint16_t SHARP_NUM_3           ; //"Sharp_num_3";
uint16_t SHARP_NUM_4           ; //"Sharp_num_4";
uint16_t SHARP_NUM_5           ; //"Sharp_num_5";
uint16_t SHARP_NUM_6           ; //"Sharp_num_6";
uint16_t SHARP_NUM_7           ; //"Sharp_num_7";
uint16_t SHARP_NUM_8           ; //"Sharp_num_8";
uint16_t SHARP_NUM_9           ; //"Sharp_num_9";
uint16_t SHARP_NUM_0           ; //"Sharp_num_0";
// uint16_t SHARP_TTXMIX          ; //"Sharp_ttxmix";
// uint16_t SHARP_PRECH           ; //"Sharp_prech";
// uint16_t SHARP_VOL_MINUS       ; //"Sharp_vol_minus";
// uint16_t SHARP_VOL_PLUS        ; //"Sharp_vol_plus";
// uint16_t SHARP_MUTE_TOGGLE     ; //"Sharp_mute_toggle";
// uint16_t SHARP_CHLIST          ; //"Sharp_chlist";
uint16_t SHARP_CHANNEL_UP      ; //"Sharp_channel_up";
uint16_t SHARP_CHANNEL_DOWN    ; //"Sharp_channel_down";
uint16_t SHARP_MENU            ; //"Sharp_menu";
// uint16_t SHARP_APPS            ; //"Sharp_apps";
uint16_t SHARP_GUIDE           ; //"Sharp_guide";
// uint16_t SHARP_TOOLS           ; //"Sharp_tools";
// uint16_t SHARP_INFO            ; //"Sharp_info";
uint16_t SHARP_UP              ; //"Sharp_up";
uint16_t SHARP_DOWN            ; //"Sharp_down";
uint16_t SHARP_LEFT            ; //"Sharp_left";
uint16_t SHARP_RIGHT           ; //"Sharp_right";
uint16_t SHARP_SELECT          ; //"Sharp_select";
// uint16_t SHARP_RETURN          ; //"Sharp_return";
uint16_t SHARP_EXIT            ; //"Sharp_exit";
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
uint16_t SHARP_REWIND          ; //"Sharp_rewind";
uint16_t SHARP_PAUSE           ; //"Sharp_pause";
uint16_t SHARP_FASTFORWARD     ; //"Sharp_fastforward";
// uint16_t SHARP_RECORD          ; //"Sharp_record";
uint16_t SHARP_PLAY            ; //"Sharp_play";
// uint16_t SHARP_STOP            ; //"Sharp_stop";
uint16_t SHARP_POWER_OFF       ; //"Sharp_power_off";
uint16_t SHARP_POWER_ON        ; //"Sharp_power_on";
uint16_t SHARP_INPUT_HDMI_1    ; //"Sharp_input_hdmi_1";
uint16_t SHARP_INPUT_HDMI_2    ; //"Sharp_input_hdmi_2";
uint16_t SHARP_INPUT_HDMI_3    ; //"Sharp_input_hdmi_3";
uint16_t SHARP_INPUT_HDMI_4    ; //"Sharp_input_hdmi_4";
uint16_t SHARP_INPUT_HDMI_5    ; //"Sharp_input_hdmi_5";

// uint16_t SHARP_INPUT_COMPONENT ; //"Sharp_input_component";
uint16_t SHARP_INPUT_TV        ; //"Sharp_input_tv";

void register_device_sharpTV() {
  // tested with Sharp UE32EH5300, works also with others
  // both GC and SHARP work well

  // https://github.com/natcl/studioimaginaire/blob/master/arduino_remote/ircodes.py
  // Only activate the commands that are used. Every command takes 100 bytes, wether used or not.
  // register_command(&SHARP_POWER_TOGGLE      , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E040BF"}));
  // register_command(&SHARP_SOURCE            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0807F"}));
  // register_command(&SHARP_HDMI              , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0D12E"}));
  register_command(&SHARP_NUM_1             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E020DF"}));
  register_command(&SHARP_NUM_2             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0A05F"}));
  register_command(&SHARP_NUM_3             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0609F"}));
  register_command(&SHARP_NUM_4             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E010EF"}));
  register_command(&SHARP_NUM_5             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0906F"}));
  register_command(&SHARP_NUM_6             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E050AF"}));
  register_command(&SHARP_NUM_7             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E030CF"}));
  register_command(&SHARP_NUM_8             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0B04F"}));
  register_command(&SHARP_NUM_9             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0708F"}));
  register_command(&SHARP_NUM_0             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E08877"}));
  // register_command(&SHARP_TTXMIX            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E034CB"}));
  // register_command(&SHARP_PRECH             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0C837"}));
  // register_command(&SHARP_VOL_MINUS         , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0D02F"}));
  // register_command(&SHARP_VOL_PLUS          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0E01F"}));
  // register_command(&SHARP_MUTE_TOGGLE       , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0F00F"}));
  // register_command(&SHARP_CHLIST            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0D629"}));
  register_command(&SHARP_CHANNEL_UP        , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E048B7"}));
  register_command(&SHARP_CHANNEL_DOWN      , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E008F7"}));
  register_command(&SHARP_MENU              , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E058A7"}));
  // register_command(&SHARP_APPS              , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E09E61"}));
  register_command(&SHARP_GUIDE             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0F20D"}));
  // register_command(&SHARP_TOOLS             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0D22D"}));
  // register_command(&SHARP_INFO              , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0F807"}));
  register_command(&SHARP_UP                , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E006F9"}));
  register_command(&SHARP_DOWN              , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E08679"}));
  register_command(&SHARP_LEFT              , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0A659"}));
  register_command(&SHARP_RIGHT             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E046B9"}));
  register_command(&SHARP_SELECT            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E016E9"}));
  // register_command(&SHARP_RETURN            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E01AE5"}));
  register_command(&SHARP_EXIT              , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0B44B"}));
  // register_command(&SHARP_KEY_A             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E036C9"}));
  // register_command(&SHARP_KEY_B             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E028D7"}));
  // register_command(&SHARP_KEY_C             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0A857"}));
  // register_command(&SHARP_KEY_D             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E06897"}));
  // register_command(&SHARP_FAMILYSTORY       , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0639C"}));
  // register_command(&SHARP_SEARCH            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0CE31"}));
  // register_command(&SHARP_DUALI_II          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E000FF"}));
  // register_command(&SHARP_SUPPORT           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0FC03"}));
  // register_command(&SHARP_PSIZE             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E07C83"}));
  // register_command(&SHARP_ADSUBT            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0A45B"}));
  register_command(&SHARP_REWIND            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0A25D"}));
  register_command(&SHARP_PAUSE             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E052AD"}));
  register_command(&SHARP_FASTFORWARD       , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E012ED"}));
  // register_command(&SHARP_RECORD            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0926D"}));
  register_command(&SHARP_PLAY              , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0E21D"}));
  // register_command(&SHARP_STOP              , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0629D"}));
  register_command(&SHARP_POWER_OFF         , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E019E6"}));
  register_command(&SHARP_POWER_ON          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E09966"}));
  register_command(&SHARP_INPUT_HDMI_1      , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E09768"}));
  register_command(&SHARP_INPUT_HDMI_2      , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E07D82"}));
  register_command(&SHARP_INPUT_HDMI_3      , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E043BC"}));
  // register_command(&SHARP_INPUT_HDMI_4      , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0A35C"}));
  // register_command(&SHARP_INPUT_COMPONENT   , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0619E"}));
  register_command(&SHARP_INPUT_TV          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0D827"}));
  // unknown commands. Not on my remote
  // register_command(&-                         , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E0C43B"}));
  // register_command(&favorite_channel          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_SHARP), "0xE0E022DD"}));

  // GC also works well
  //register_command(&SHARP_POWER_TOGGLE      , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,1,170,170,20,63,20,63,20,63,20,20,20,20,20,20,20,20,20,20,20,63,20,63,20,63,20,20,20,20,20,20,20,20,20,20,20,20,20,63,20,20,20,20,20,20,20,20,20,20,20,20,20,63,20,20,20,63,20,63,20,63,20,63,20,63,20,63,20,1798"}));
  //register_command(&SHARP_POWER_OFF         , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,1,173,173,21,65,21,65,21,65,21,21,21,21,21,21,21,21,21,21,21,65,21,65,21,65,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,65,21,65,21,21,21,21,21,65,21,65,21,65,21,65,21,21,21,21,21,65,21,65,21,21,21,1832"}));
  //register_command(&SHARP_POWER_ON          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,1,172,172,22,64,22,64,22,64,22,21,22,21,22,21,22,21,22,21,22,64,22,64,22,64,22,21,22,21,22,21,22,21,22,21,22,64,22,21,22,21,22,64,22,64,22,21,22,21,22,64,22,21,22,64,22,64,22,21,22,21,22,64,22,64,22,21,22,1820"}));
  //register_command(&SHARP_INPUT_HDMI_1      , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,1,173,173,21,65,21,65,21,65,21,21,21,21,21,21,21,21,21,21,21,65,21,65,21,65,21,21,21,21,21,21,21,21,21,21,21,65,21,21,21,21,21,65,21,21,21,65,21,65,21,65,21,21,21,65,21,65,21,21,21,65,21,21,21,21,21,21,21,1832"}));
  //register_command(&SHARP_INPUT_HDMI_2      , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,1,173,173,21,65,21,65,21,65,21,21,21,21,21,21,21,21,21,21,21,65,21,65,21,65,21,21,21,21,21,21,21,21,21,21,21,21,21,65,21,65,21,65,21,65,21,65,21,21,21,65,21,65,21,21,21,21,21,21,21,21,21,21,21,65,21,21,21,1832"}));
  //register_command(&SHARP_INPUT_TV          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,1,172,172,21,64,21,64,21,64,21,21,21,21,21,21,21,21,21,21,21,64,21,64,21,64,21,21,21,21,21,21,21,21,21,21,21,64,21,64,21,21,21,64,21,64,21,21,21,21,21,21,21,21,21,21,21,64,21,21,21,21,21,64,21,64,21,64,21,1673"}));
}
