
#include "applicationInternal/commandHandler.h"

#include "device_smarthome.h"



// WebSocket command IDs (paired with MQTT IDs for compatibility)
uint16_t SMARTHOME_WS_LIGHT_ON;
uint16_t SMARTHOME_WS_LIGHT_OFF;
uint16_t SMARTHOME_WS_LIGHT_BRIGHTNESS;

void
register_device_smarthome() {
#if (ENABLE_WIFI_AND_MQTT == 1)
  
#if (ENABLE_WEBSOCKET == 1)
    register_command(&SMARTHOME_WS_LIGHT_ON, makeCommandData(WS, {"turn_on"}));   // payload must be set when calling commandHandler
    register_command(&SMARTHOME_WS_LIGHT_OFF, makeCommandData(WS, {"turn_off"})); // payload must be set when calling commandHandler
    register_command(&SMARTHOME_WS_LIGHT_BRIGHTNESS,
                     makeCommandData(WS, {"light_brightness"})); // payload must be set when calling commandHandler

#endif
#endif
}
