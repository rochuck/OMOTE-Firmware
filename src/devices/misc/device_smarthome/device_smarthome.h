#pragma once

extern uint16_t SMARTHOME_MQTT_BULB1_SET;
extern uint16_t SMARTHOME_MQTT_BULB2_SET;
extern uint16_t SMARTHOME_MQTT_BULB1_BRIGHTNESS_SET;
extern uint16_t SMARTHOME_MQTT_BULB2_BRIGHTNESS_SET;

// WebSocket equivalents
extern uint16_t SMARTHOME_WS_LIGHT_ON;
extern uint16_t SMARTHOME_WS_LIGHT_OFF;
extern uint16_t SMARTHOME_WS_LIGHT_BRIGHTNESS;

void
register_device_smarthome();
