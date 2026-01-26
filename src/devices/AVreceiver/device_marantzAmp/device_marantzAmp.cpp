#include <string>
#include "applicationInternal/commandHandler.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "device_marantzAmp.h"

  // Only activate the commands that are used. Every command takes 100 bytes, wether used or not.
  uint16_t MARANTZ_INPUT_DVD           ; //"Marantz_input_dvd";
  uint16_t MARANTZ_INPUT_DTV           ; //"Marantz_input_dtv";
  // uint16_t MARANTZ_INPUT_VCR           ; //"Marantz_input_vcr";
  // uint16_t MARANTZ_POWER_TOGGLE        ; //"Marantz_power_toggle";
  // uint16_t MARANTZ_INPUT_CD            ; //"Marantz_input_cd";
  // uint16_t MARANTZ_INPUT_MD            ; //"Marantz_input_md";
  // uint16_t MARANTZ_INPUT_VAUX          ; //"Marantz_input_vaux";
  // uint16_t MARANTZ_MULTICHANNEL        ; //"Marantz_multichannel";
  // uint16_t MARANTZ_INPUT_TUNER         ; //"Marantz_input_tuner";
  // uint16_t MARANTZ_PRESETGROUP         ; //"Marantz_presetgroup";
  // uint16_t MARANTZ_PRESETSTATION_MINUS ; //"Marantz_presetstation-";
  // uint16_t MARANTZ_PRESETSTATION_PLUS  ; //"Marantz_presetstation+";
  uint16_t MARANTZ_STANDARD            ; //"Marantz_standard";
  // uint16_t MARANTZ_5CHSTEREO           ; //"Marantz_5chstereo";
  // uint16_t MARANTZ_NIGHT               ; //"Marantz_night";
  // uint16_t MARANTZ_SLEEP               ; //"Marantz_sleep";
  // uint16_t MARANTZ_TEST                ; //"Marantz_test";
  // uint16_t MARANTZ_STRAIGHT            ; //"Marantz_straight";
  uint16_t MARANTZ_VOL_MINUS           ; //"Marantz_vol-";
  uint16_t MARANTZ_VOL_PLUS            ; //"Marantz_vol+";
  // uint16_t MARANTZ_PROG_MINUS          ; //"Marantz_prog-";
  // uint16_t MARANTZ_PROG_PLUS           ; //"Marantz_prog+";
  uint16_t MARANTZ_MUTE_TOGGLE         ; //"Marantz_mute_toggle";
  // uint16_t MARANTZ_LEVEL               ; //"Marantz_level";
  // uint16_t MARANTZ_SETMENU             ; //"Marantz_setmenu";
  // uint16_t MARANTZ_SETMENU_UP          ; //"Marantz_setmenu_up";
  // uint16_t MARANTZ_SETMENU_DOWN        ; //"Marantz_setmenu_down";
  // uint16_t MARANTZ_SETMENU_MINUS       ; //"Marantz_setmenu_-";
  // uint16_t MARANTZ_SETMENU_PLUS        ; //"Marantz_setmenu_+";
  uint16_t MARANTZ_POWER_OFF           ; //"Marantz_power_off";
  uint16_t MARANTZ_POWER_ON            ; //"Marantz_power_on";

void register_device_marantzAmp() {
  // tested with Marantz RX-V359, works also with others

  // Only activate the commands that are used. Every command takes 100 bytes, wether used or not.
  register_command(&MARANTZ_INPUT_DVD           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1837C"}));
  register_command(&MARANTZ_INPUT_DTV           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA12AD5"}));
  // register_command(&MARANTZ_INPUT_VCR           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1F00F"}));
  // register_command(&MARANTZ_POWER_TOGGLE        , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1F807"}));
  // register_command(&MARANTZ_INPUT_CD            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1A857"}));
  // register_command(&MARANTZ_INPUT_MD            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1936C"}));
  // register_command(&MARANTZ_INPUT_VAUX          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1AA55"}));
  // register_command(&MARANTZ_MULTICHANNEL        , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1E11E"}));
  // register_command(&MARANTZ_INPUT_TUNER         , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA16897"}));
  // register_command(&MARANTZ_PRESETGROUP         , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA148B7"}));
  // register_command(&MARANTZ_PRESETSTATION_MINUS , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA18877"}));
  // register_command(&MARANTZ_PRESETSTATION_PLUS  , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA108F7"}));
  register_command(&MARANTZ_STANDARD            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA109F6"}));
  // register_command(&MARANTZ_5CHSTEREO           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1E916"}));
  // register_command(&MARANTZ_NIGHT               , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1A956"}));
  // register_command(&MARANTZ_SLEEP               , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1EA15"}));
  // register_command(&MARANTZ_TEST                , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1A15E"}));
  // register_command(&MARANTZ_STRAIGHT            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA16A95"}));
  register_command(&MARANTZ_VOL_MINUS           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1D827"}));
  register_command(&MARANTZ_VOL_PLUS            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA158A7"}));
  // register_command(&MARANTZ_PROG_MINUS          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA19A65"}));
  // register_command(&MARANTZ_PROG_PLUS           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA11AE5"}));
  register_command(&MARANTZ_MUTE_TOGGLE         , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA138C7"}));
  // register_command(&MARANTZ_LEVEL               , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1619E"}));
  // register_command(&MARANTZ_SETMENU             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA139C6"}));
  // register_command(&MARANTZ_SETMENU_UP          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA119E6"}));
  // register_command(&MARANTZ_SETMENU_DOWN        , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA19966"}));
  // register_command(&MARANTZ_SETMENU_MINUS       , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1CA35"}));
  // register_command(&MARANTZ_SETMENU_PLUS        , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA14AB5"}));
  register_command(&MARANTZ_POWER_OFF           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA17887"}));
  register_command(&MARANTZ_POWER_ON            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1B847"}));

  // GC seems not to work
  //register_command(&MARANTZ_POWER_TOGGLE       , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,69,341,170,21,21,21,64,21,64,21,64,21,64,21,64,21,64,21,21,21,64,21,21,21,21,21,21,21,21,21,21,21,21,21,64,21,21,21,64,21,21,21,64,21,21,21,64,21,21,21,21,21,64,21,21,21,64,21,21,21,64,21,21,21,64,21,64,21,1517,341,85,21,3655"}));
  //register_command(&MARANTZ_POWER_OFF          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,69,341,170,21,21,21,64,21,64,21,64,21,64,21,64,21,64,21,21,21,64,21,21,21,21,21,21,21,21,21,21,21,21,21,64,21,64,21,64,21,64,21,64,21,64,21,64,21,64,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,64,21,1517,341,85,21,3655"}));
  //register_command(&MARANTZ_POWER_ON           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,69,341,170,21,21,21,64,21,64,21,64,21,64,21,64,21,64,21,21,21,64,21,21,21,21,21,21,21,21,21,21,21,21,21,64,21,21,21,64,21,64,21,64,21,64,21,64,21,64,21,21,21,64,21,21,21,21,21,21,21,21,21,21,21,21,21,64,21,1517,341,85,21,3655"}));
  //register_command(&MARANTZ_INPUT_DVD          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,69,341,170,21,21,21,64,21,21,21,64,21,64,21,64,21,64,21,21,21,64,21,21,21,64,21,21,21,21,21,21,21,21,21,64,21,64,21,21,21,21,21,21,21,21,21,21,21,64,21,64,21,21,21,64,21,64,21,64,21,64,21,64,21,21,21,21,21,1517,341,85,21,3655"}));
  //register_command(&MARANTZ_INPUT_DTV          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,69,341,170,21,21,21,64,21,21,21,64,21,64,21,64,21,64,21,21,21,64,21,21,21,64,21,21,21,21,21,21,21,21,21,64,21,21,21,21,21,64,21,21,21,64,21,21,21,64,21,21,21,64,21,64,21,21,21,64,21,21,21,64,21,21,21,64,21,1517,341,85,21,3655"}));
  //register_command(&MARANTZ_STANDARD           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38000,1,69,341,170,21,21,21,64,21,64,21,64,21,64,21,64,21,64,21,21,21,64,21,21,21,21,21,21,21,21,21,21,21,21,21,64,21,21,21,64,21,64,21,64,21,64,21,64,21,64,21,64,21,64,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,1517,341,85,21,3655"}));
}
