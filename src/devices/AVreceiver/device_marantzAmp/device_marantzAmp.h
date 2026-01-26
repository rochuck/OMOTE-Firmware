#pragma once

// Only activate the commands that are used. Every command takes 100 bytes, wether used or not.
extern uint16_t MARANTZ_INPUT_DVD;
extern uint16_t MARANTZ_INPUT_DTV;
// extern uint16_t MARANTZ_INPUT_VCR;
// extern uint16_t MARANTZ_POWER_TOGGLE;
// extern uint16_t MARANTZ_INPUT_CD;
// extern uint16_t MARANTZ_INPUT_MD;
// extern uint16_t MARANTZ_INPUT_VAUX;
// extern uint16_t MARANTZ_MULTICHANNEL;
// extern uint16_t MARANTZ_INPUT_TUNER;
// extern uint16_t MARANTZ_PRESETGROUP;
// extern uint16_t MARANTZ_PRESETSTATION_MINUS;
// extern uint16_t MARANTZ_PRESETSTATION_PLUS;
extern uint16_t MARANTZ_STANDARD;
// extern uint16_t MARANTZ_5CHSTEREO;
// extern uint16_t MARANTZ_NIGHT;
// extern uint16_t MARANTZ_SLEEP;
// extern uint16_t MARANTZ_TEST;
// extern uint16_t MARANTZ_STRAIGHT;
extern uint16_t MARANTZ_VOL_MINUS;
extern uint16_t MARANTZ_VOL_PLUS;
// extern uint16_t MARANTZ_PROG_MINUS;
// extern uint16_t MARANTZ_PROG_PLUS;
extern uint16_t MARANTZ_MUTE_TOGGLE;
// extern uint16_t MARANTZ_LEVEL;
// extern uint16_t MARANTZ_SETMENU;
// extern uint16_t MARANTZ_SETMENU_UP;
// extern uint16_t MARANTZ_SETMENU_DOWN;
// extern uint16_t MARANTZ_SETMENU_MINUS;
// extern uint16_t MARANTZ_SETMENU_PLUS;
extern uint16_t MARANTZ_POWER_OFF;
extern uint16_t MARANTZ_POWER_ON;

void register_device_marantzAmp();
