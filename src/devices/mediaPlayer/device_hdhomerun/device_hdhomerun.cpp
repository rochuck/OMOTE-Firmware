#include <string>
#include "applicationInternal/commandHandler.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "device_hdhomerun.h"

uint16_t HDHOMERUN_UP;
uint16_t HDHOMERUN_DOWN;
uint16_t HDHOMERUN_LEFT;
uint16_t HDHOMERUN_RIGHT;
uint16_t HDHOMERUN_OK;

uint16_t HDHOMERUN_PLAY;
uint16_t HDHOMERUN_PAUSE;

uint16_t HDHOMERUN_10_SECOND_BACK;
uint16_t HDHOMERUN_10_SECOND_FOREWARD;

uint16_t HDHOMERUN_NEXT;
uint16_t HDHOMERUN_PREVIOUS;

uint16_t HDHOMERUN_MENU;
uint16_t HDHOMERUN_HOME;

uint16_t HDHOMERUN_POWER_ON;
uint16_t HDHOMERUN_POWER_OFF;

void register_device_hdhomerun() {
  register_command(&HDHOMERUN_UP                   , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x77E15080"}));
  register_command(&HDHOMERUN_DOWN                 , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x77E13080"}));
  register_command(&HDHOMERUN_LEFT                 , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x77E19080"}));
  register_command(&HDHOMERUN_RIGHT                , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x77E16080"}));
  register_command(&HDHOMERUN_OK                   , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x77E13A80"}));

  register_command(&HDHOMERUN_PLAY                 , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x77E1FA80"})); 
  register_command(&HDHOMERUN_PAUSE                , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0xA7E14C80:32:1"})); // Code + kNECBits + 1 repeat

  register_command(&HDHOMERUN_10_SECOND_BACK       , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0xA7E16480:32:1"})); // Code + kNECBits + 1 repeat
  register_command(&HDHOMERUN_10_SECOND_FOREWARD   , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0xA7E11080:32:1"})); // Code + kNECBits + 1 repeat

  register_command(&HDHOMERUN_NEXT                 , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0xA7E1C480:32:1"})); // Code + kNECBits + 1 repeat
  register_command(&HDHOMERUN_PREVIOUS             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0xA7E1A480:32:1"})); // Code + kNECBits + 1 repeat
  
  register_command(&HDHOMERUN_MENU                 , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x77E1C080"}));
  register_command(&HDHOMERUN_HOME                 , makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0xA7E10280:32:1"})); // Code + kNECBits + 1 repeat

  register_command(&HDHOMERUN_POWER_ON             , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38380,1,69,347,173,22,65,22,22,22,65,22,22,22,22,22,65,22,65,22,65,22,65,22,65,22,65,22,22,22,22,22,22,22,22,22,65,22,22,22,22,22,65,22,65,22,22,22,65,22,22,22,22,22,22,22,65,22,65,22,65,22,65,22,65,22,65,22,65,22,1397,347,87,22,3692"}));
  register_command(&HDHOMERUN_POWER_OFF            , makeCommandData(IR, {std::to_string(IR_PROTOCOL_GLOBALCACHE), "38380,1,69,347,173,22,65,22,22,22,65,22,22,22,22,22,65,22,65,22,65,22,65,22,65,22,65,22,22,22,22,22,22,22,22,22,65,22,22,22,65,22,22,22,65,22,22,22,65,22,22,22,22,22,22,22,65,22,65,22,65,22,65,22,65,22,65,22,65,22,1397,347,87,22,3692"}));
}
