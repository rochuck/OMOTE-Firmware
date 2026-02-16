#include "websocket_hal_esp32.h"
#include <Arduino.h>
#include <cJSON.h>
#include <esp_websocket_client.h>
#include <stdio.h>
#include <string.h>

#include "secrets.h"

static const char*                   TAG = "HA_WS";
static esp_websocket_client_handle_t client;

static tAnnounceWebsocketMessage_cb thisAnnounceWebsocketMessage_cb = NULL;

void
set_announceWebsocketMessage_cb_HAL(tAnnounceWebsocketMessage_cb cb) {
    thisAnnounceWebsocketMessage_cb = cb;
}
void
websocket_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*) event_data;

    // Move declarations to TOP - fixes scope issue
    cJSON* auth     = NULL;
    char*  auth_str = NULL;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to Home Assistant");
        // Send auth
        auth = cJSON_CreateObject();
        cJSON_AddStringToObject(auth, "type", "auth");
        cJSON_AddStringToObject(auth, "access_token", WS_TOKEN);
        auth_str = cJSON_PrintUnformatted(auth);
        esp_websocket_client_send_text(client, auth_str, strlen(auth_str), portMAX_DELAY);
        free(auth_str);
        cJSON_Delete(auth);
        break;

    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "Received: %.*s", data->data_len, (char*) data->data_ptr);
        // Parse JSON response here
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        break;

    default:
        break;
    }
}

void
init_websocket_HAL(void) {
    // TODO: initialize WiFi (if needed), configure websocket client, set URL/port
    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri                           = WS_URL; // Your HA IP

    client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    esp_websocket_client_start(client);
}

bool
websocket_isConnected_HAL() {
    // TODO: return actual connection state
    return false;
}

void
websocket_loop_HAL() {
    // TODO: call client loop/handle to process messages and reconnect if needed
}

bool
websocket_send_HAL(const char* payload) {
    // TODO: send payload over websocket, return true on success
    (void) payload;
    return false;
}

void
websocket_shutdown_HAL() {
    // TODO: gracefully close the websocket and free resources
}
