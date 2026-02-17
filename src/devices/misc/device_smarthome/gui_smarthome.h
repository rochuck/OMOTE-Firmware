#pragma once

#include <lvgl.h>
#include <string>
const char* const tabName_smarthome = "Smart Home";
extern uint16_t   GUI_SMARTHOME_ACTIVATE;
void
register_gui_smarthome(void);
void
handle_HA_websocket_message(std::string message);
