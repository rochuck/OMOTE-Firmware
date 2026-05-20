#include "device_marantzAmp.h"
#include "applicationInternal/commandHandler.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include <string>

// Only activate the commands that are used. Every command takes 100 bytes, wether used or not.
uint16_t MARANTZ_INPUT_DVD; //"Marantz_input_dvd";
uint16_t MARANTZ_INPUT_DTV; //"Marantz_input_dtv";
// On the NR1501 the front-panel labels for these codes don't match the rear-panel HDMI
// labels — bench-tested 2026-05-08:
//   BD button   -> selects the AppleTV HDMI input  (used by scene_appleTV)
//   GAME button -> selects the Kodi HDMI input     (used by scene_kodi)
//   DSS button  -> untested (DVD code did nothing on this unit)
uint16_t MARANTZ_INPUT_BD;   // Marantz 2015 IR Chart: System 07 / Cmd 63 / Ext 00 (Blu-ray Code1)
uint16_t MARANTZ_INPUT_GAME; // Marantz 2015 IR Chart: System 16 / Cmd 00 / Ext 62
uint16_t MARANTZ_INPUT_DSS;  // Marantz 2015 IR Chart: System 06 / Cmd 63 (CBL/SAT)
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
uint16_t MARANTZ_SDIRECT; //"Marantz_sdirect";
// uint16_t MARANTZ_5CHSTEREO           ; //"Marantz_5chstereo";
// uint16_t MARANTZ_NIGHT               ; //"Marantz_night";
// uint16_t MARANTZ_SLEEP               ; //"Marantz_sleep";
// uint16_t MARANTZ_TEST                ; //"Marantz_test";
// uint16_t MARANTZ_STRAIGHT            ; //"Marantz_straight";
uint16_t MARANTZ_VOL_MINUS; //"Marantz_vol-";
uint16_t MARANTZ_VOL_PLUS;  //"Marantz_vol+";
// uint16_t MARANTZ_PROG_MINUS          ; //"Marantz_prog-";
// uint16_t MARANTZ_PROG_PLUS           ; //"Marantz_prog+";
uint16_t MARANTZ_MUTE_TOGGLE; //"Marantz_mute_toggle";
// uint16_t MARANTZ_LEVEL               ; //"Marantz_level";
// uint16_t MARANTZ_SETMENU             ; //"Marantz_setmenu";
// uint16_t MARANTZ_SETMENU_UP          ; //"Marantz_setmenu_up";
// uint16_t MARANTZ_SETMENU_DOWN        ; //"Marantz_setmenu_down";
// uint16_t MARANTZ_SETMENU_MINUS       ; //"Marantz_setmenu_-";
// uint16_t MARANTZ_SETMENU_PLUS        ; //"Marantz_setmenu_+";
uint16_t MARANTZ_POWER_OFF; //"Marantz_power_off";
uint16_t MARANTZ_POWER_ON;  //"Marantz_power_on";

/* Chucks note, looking at the back of the amp, from left to right the hdmi inputs are:
blueray game dvd dss and the out */

void
register_device_marantzAmp() {
    // tested with Marantz RX-V359, works also with others

    // Only activate the commands that are used. Every command takes 100 bytes, whether used or not.
    // Original NEC code 0x5EA1837C did nothing on the NR1501 — swapped for the Pronto code
    // from the Marantz 2015 IR Chart (System 16 / Cmd 00 / Ext 10) so it can be retested.
    register_command(&MARANTZ_INPUT_DVD,
                     makeCommandData(IR,
                                     {std::to_string(IR_PROTOCOL_GLOBALCACHE),
                                      "36000,1,1,32,32,64,64,64,32,32,32,32,32,32,161,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,64,"
                                      "64,64,64,2731,32,32,64,64,64,32,32,32,32,32,32,161,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,"
                                      "64,64,64,64,1200"}, "AMP DVD"));
    register_command(&MARANTZ_INPUT_DTV, makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA12AD5"}, "AMP DTV"));

    // HDMI inputs from the official Marantz 2015 NR/SR/AV IR Command Chart, converted from
    // Pronto hex to GlobalCache decimal at 36 kHz (same approach as POWER_ON/OFF below).

    // Blu-ray (BD) Code1 — Pronto: 0000 0071 0000 0024 ... (36 repeat pairs)
    register_command(&MARANTZ_INPUT_BD,
                     makeCommandData(IR,
                                     {std::to_string(IR_PROTOCOL_GLOBALCACHE),
                                      "36000,1,1,32,32,32,32,64,32,32,64,32,32,32,32,32,161,32,32,32,32,32,32,32,32,32,32,64,32,32,32,"
                                      "32,32,32,32,32,32,32,2731,32,32,32,32,64,32,32,64,32,32,32,32,32,161,32,32,32,32,32,32,32,32,32,"
                                      "32,64,32,32,32,32,32,32,32,32,32,32,1200"}, "AMP BD"));

    // GAME — Pronto: 0000 0071 0000 0011 ... (17 pairs, single frame as documented in chart)
    register_command(&MARANTZ_INPUT_GAME,
                     makeCommandData(IR,
                                     {std::to_string(IR_PROTOCOL_GLOBALCACHE),
                                      "36000,1,1,31,31,64,64,64,31,31,31,31,31,31,160,31,31,31,31,31,31,31,31,31,31,31,64,31,31,31,31,"
                                      "31,31,31,31,64,2721"}, "AMP GAME"));

    // DSS / CBL/SAT — Pronto: 0000 0073 0000 0020 ... (32 pairs, three RC5-Ext frames)
    register_command(&MARANTZ_INPUT_DSS,
                     makeCommandData(IR,
                                     {std::to_string(IR_PROTOCOL_GLOBALCACHE),
                                      "36000,1,1,32,32,32,32,64,32,32,64,32,32,64,64,32,32,32,32,32,32,32,32,32,32,32,3163,32,32,32,32,"
                                      "64,32,32,64,32,32,64,64,32,32,32,32,32,32,32,32,32,32,32,3163,32,32,32,32,64,32,32,64,32,32,64,"
                                      "64,32,32,32,1176"}, "AMP DSS"));
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
    register_command(&MARANTZ_SDIRECT, makeCommandData(IR, {std::to_string(IR_PROTOCOL_RC5), "0x422"}, "AMP DIRECT")); // c22 and 422
    // register_command(&MARANTZ_5CHSTEREO           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1E916"}));
    // register_command(&MARANTZ_NIGHT               , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1A956"}));
    // register_command(&MARANTZ_SLEEP               , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1EA15"}));
    // register_command(&MARANTZ_TEST                , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1A15E"}));
    // register_command(&MARANTZ_STRAIGHT            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA16A95"}));
    register_command(&MARANTZ_VOL_MINUS, makeCommandData(IR, {std::to_string(IR_PROTOCOL_RC5), "0x411"}, "AMP VOL-"));
    register_command(&MARANTZ_VOL_PLUS, makeCommandData(IR, {std::to_string(IR_PROTOCOL_RC5), "0x410"}, "AMP VOL+"));
    // register_command(&MARANTZ_PROG_MINUS          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA19A65"}));
    // register_command(&MARANTZ_PROG_PLUS           , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA11AE5"}));
    register_command(&MARANTZ_MUTE_TOGGLE, makeCommandData(IR, {std::to_string(IR_PROTOCOL_RC5), "0x40D"}, "AMP MUTE"));
    // register_command(&MARANTZ_LEVEL               , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1619E"}));
    // register_command(&MARANTZ_SETMENU             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA139C6"}));
    // register_command(&MARANTZ_SETMENU_UP          , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA119E6"}));
    // register_command(&MARANTZ_SETMENU_DOWN        , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA19966"}));
    // register_command(&MARANTZ_SETMENU_MINUS       , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA1CA35"}));
    // register_command(&MARANTZ_SETMENU_PLUS        , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x5EA14AB5"}));

    // these seem to work for power on and power off
    // https://github.com/Arduino-IRremote/Arduino-IRremote/discussions/1038
    //

    // Pronto: 0000 0071 0000 0024 ... (36 repeat pairs, ~36.7kHz → 36000)
    register_command(&MARANTZ_POWER_ON,
                     makeCommandData(IR,
                                     {std::to_string(IR_PROTOCOL_GLOBALCACHE),
                                      "36000,1,1,32,32,32,32,32,32,64,32,32,32,32,32,32,161,32,32,32,64,32,32,64,32,32,32,32,32,32,"
                                      "32,32,32,32,32,32,64,32,2731,32,32,32,32,32,32,64,32,32,32,32,32,32,161,32,32,32,64,32,32,"
                                      "64,32,32,32,32,32,32,32,32,32,32,32,32,64,32,1200"}, "AMP ON"));
    // Discrete power-off from Pronto hex (biphase, two frames) converted to GlobalCache at 36kHz.
    // Pronto: 0000 0071 0000 0022 ... (34 repeat pairs — two pairs shorter than ON)
    register_command(&MARANTZ_POWER_OFF,
                     makeCommandData(IR,
                                     {std::to_string(IR_PROTOCOL_GLOBALCACHE),
                                      "36000,1,1,32,32,32,32,32,32,64,32,32,32,32,32,32,161,32,32,32,64,32,32,64,32,32,32,32,32,32,"
                                      "32,32,32,32,64,64,2731,32,32,32,32,32,32,64,32,32,32,32,32,32,161,32,32,32,64,32,32,64,32,"
                                      "32,32,32,32,32,32,32,32,32,64,64,1200"}, "AMP OFF"));
}