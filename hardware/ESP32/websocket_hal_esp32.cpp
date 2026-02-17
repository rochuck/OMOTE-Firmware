#include "websocket_hal_esp32.h"
#include <Arduino.h>
#include <cJSON.h>
#include <esp_websocket_client.h>
#include <stdio.h>
#include <string.h>

#include "secrets.h"

static const char*                   TAG = "HA_WS";
static esp_websocket_client_handle_t client;
static bool                          is_connected                    = false;
static bool                          is_subscribed                   = false;
static uint16_t                      ws_id                           = 1234;
static tAnnounceWebsocketMessage_cb  thisAnnounceWebsocketMessage_cb = NULL;

// global buffer for fragmented messages
static char ws_buffer[8192]  = {0};
static int  buffer_len       = 0;
static bool message_complete = false;

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
        is_connected = true;
        break;

    case WEBSOCKET_EVENT_DATA:
        if (data->data_len == 0) break; // Ignore empty messages

        // Copy data to buffer
        if (buffer_len + data->data_len < sizeof(ws_buffer)) {
            memcpy(ws_buffer + buffer_len, data->data_ptr, data->data_len);
            buffer_len += data->data_len;

            // Check if JSON complete (ends with })
            if (ws_buffer[buffer_len - 1] == '}') {
                ws_buffer[buffer_len] = '\0'; // Null terminate
                message_complete      = true;

                // ESP_LOGI(TAG, "Received: %.*s", buffer_len, (char*) ws_buffer);
                // Parse JSON response here
                thisAnnounceWebsocketMessage_cb(std::string((char*) ws_buffer, buffer_len));

                // Reset buffer
                buffer_len = 0;
                memset(ws_buffer, 0, sizeof(ws_buffer));
            }
        }
        break;
        // case WEBSOCKET_EVENT_DATA:
        //     ESP_LOGI(TAG, "Received: %.*s", data->data_len, (char*) data->data_ptr);
        //     // Parse JSON response here
        //     thisAnnounceWebsocketMessage_cb(std::string((char*) data->data_ptr, data->data_len));
        //     break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        is_connected = false;
        break;

    default:
        break;
    }
}

void
websocket_sub_HAL(const char* entity_list) {
    if (!is_subscribed) {
        if (is_connected) {
            // Subscribe to all events
            esp_err_t ret = esp_websocket_client_send_text(client, entity_list, strlen(entity_list), portMAX_DELAY);
            if (ret >= 0) { printf("Subscribed to entities: %s\n", entity_list); }
            is_subscribed = true;
        }
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
    return is_connected;
}
// this sends a message to update a switch, or brightness

bool
websocket_send_HAL(const char* topic, const char* payload) {

    printf("Got topic %s and payload %s\n", topic, payload);

    // topic is the command and payload is the device id

    cJSON* root   = cJSON_CreateObject();
    cJSON* target = cJSON_CreateObject();

    // make base object
    cJSON_AddNumberToObject(root, "id", ++ws_id %= 10000);
    cJSON_AddStringToObject(root, "type", "call_service");
    cJSON_AddStringToObject(root, "domain", "light");
    // cJSON_AddStringToObject(root, "service", topic);

    // turn payload into json object
    std::string payload_wrapper = std::string("{") + payload + std::string("}");
    cJSON*      payload_obj     = cJSON_Parse(payload_wrapper.c_str());
    if (!payload_obj || !cJSON_IsObject(payload_obj)) {
        // Handle error
        return false;
    }

    // MERGE: the two objects
    cJSON* service_item = cJSON_DetachItemFromObject(payload_obj, "service");
    if (service_item) cJSON_AddItemToObject(root, service_item->string, service_item);

    cJSON* target_item = cJSON_DetachItemFromObject(payload_obj, "target");
    if (target_item) cJSON_AddItemToObject(root, target_item->string, target_item);

    cJSON* service_data_item = cJSON_DetachItemFromObject(payload_obj, "service_data");
    if (service_data_item) cJSON_AddItemToObject(root, service_data_item->string, service_data_item);

    char* msg = cJSON_PrintUnformatted(root);
    int   ret = esp_websocket_client_send_text(client, msg, strlen(msg), portMAX_DELAY);

    if (ret >= 0) {
        Serial.printf("Sent turn_on: %s\n", msg);
    } else {
        Serial.println("Send failed");
    }
    cJSON_Delete(root);
    free(msg);

    // no one looks at this anyway
    return (ret > 0);
}

void
websocket_shutdown_HAL() {
    // TODO: gracefully close the websocket and free resources
}
