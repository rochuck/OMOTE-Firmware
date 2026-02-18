#pragma once

#include <string>

// WebSocket HAL (ESP32) - skeleton
// TODO: Implement using project's WebSocket client of choice (e.g. WebSocketsClient)

void
init_websocket_HAL(void);
bool
websocket_isConnected_HAL();
void
websocket_loop_HAL();
bool
websocket_send_HAL(const char* topic,const char* payload);
void
websocket_shutdown_HAL();
void
websocket_sub_HAL(const char* entity_list);

typedef void (*tAnnounceWebsocketMessage_cb)(const std::string& message);
void
set_announceWebsocketMessage_cb_HAL(tAnnounceWebsocketMessage_cb cb);
