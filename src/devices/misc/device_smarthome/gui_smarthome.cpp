#include "devices/misc/device_smarthome/gui_smarthome.h"
#include "applicationInternal/commandHandler.h"
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/keys.h"
#include "applicationInternal/omote_log.h"
#include "applicationInternal/scenes/sceneRegistry.h"
#include "cJSON.h"
#include "devices/misc/device_smarthome/device_smarthome.h"
#include "scenes/scene__default.h"
#include <lvgl.h>
#include <string>
#include <unordered_map>

// LVGL declarations
LV_IMG_DECLARE(lightbulb);

uint16_t GUI_SMARTHOME_ACTIVATE;

std::map<char, repeatModes> key_repeatModes_smarthome    = {};
std::map<char, uint16_t>    key_commands_short_smarthome = {};
std::map<char, uint16_t>    key_commands_long_smarthome  = {};

// entity id table
// This table contains the HA entity ids, the associated, lvgl_object for the display,
// the state and the brightness of the light. And a friendly name for me, since
// entity ids suck. It is up to the code to map the names to the switches correctly

struct entity_item {
    bool        state;         // "on"/"off"
    uint8_t     brightness;    // 0-255
    lv_obj_t*   lv_switch;     // LVGL switch pointer
    lv_obj_t*   lv_slider;     // LVGL slider pointer
    const char* friendly_name; // friendly name for me, since entity ids suck
    const char* entity_id;     // HA entity id
    const char* label;         // label on the GUI
};

std::unordered_map<std::string, entity_item*> entity_map;   // entity_id → struct
std::unordered_map<std::string, entity_item*> friendly_map; // friendly → struct
/*
const char* event_id_table[]{
    "dummy",
    "light.signify_netherlands_b_v_lct014_light_2", // computer floor lamp, in living room, hue bulb
    "light.signify_netherlands_b_v_lct014_light",   // piano lamp, in living room, hue bulb
    NULL                                            // always end with NULL, end of table
};
*/
#define ADD_ENTITY(friendly, eid, label)                                                  \
    e                      = new entity_item{false, 0, NULL, NULL, friendly, eid, label}; \
    entity_map[eid]        = e;                                                           \
    friendly_map[friendly] = e;

void
init_entity_maps(void) {
    entity_item* e;
    ADD_ENTITY("lrfl_comp",
               "light.signify_netherlands_b_v_lct014_light_2",
               "Computer Floor\nLamp");                                                   // living room floor lamp-computer
    ADD_ENTITY("lrfl_piano", "light.signify_netherlands_b_v_lct014_light", "Piano Lamp"); // living room floor lamp - piano
}

// Smart Home Toggle Event handler

/*
{
  "id": 1234,
  "type": "call_service",
  "domain": "light",
  "service": "turn_on",
  "target": {
    "entity_id": "light.living_room"
  }
}

*/

static void
smartHomeToggle_event_cb(lv_event_t* e) {
    uint16_t     command;
    std::string  payload;
    const char*  friendly_name = (const char*) (e->user_data);
    entity_item* e_item        = friendly_map[friendly_name];

    omote_log_d("switch callback\r\n");

    if (e_item == NULL) { return; }

    if (lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED)) {
        command = SMARTHOME_WS_LIGHT_ON;
    } else {
        command = SMARTHOME_WS_LIGHT_OFF;
    }

    /* for now we provide the websocket json that is:
    "service": "turn_on",
    "target": {
      "entity_id": "light.living_room"
    }

    we do this on one line, and stuff it in the payload
    in future whis may need to change. if the HA control gets more complex
    the light on/off command does not even matter,  i may consolidate
    them later on
    */
    payload = "\"service\":\"";
    payload += (command == SMARTHOME_WS_LIGHT_ON) ? "turn_on" : "turn_off";
    payload += "\",\"target\":{\"entity_id\":\"";
    payload += e_item->entity_id;
    payload += "\"}";

    executeCommand(command, payload);
}
/*

{
  "id": 1234,
  "type": "call_service",
  "domain": "light",
  "service": "turn_on",
  "target": {
    "entity_id": "light.living_room"
  },
  "service_data": {
    "brightness": 128
  }
}

*/
// Smart Home Slider Event handler
static void
smartHomeSlider_event_cb(lv_event_t* e) {
    uint16_t     command;
    std::string  payload;
    const char*  friendly_name = (const char*) (e->user_data);
    entity_item* e_item        = friendly_map[friendly_name];
    lv_obj_t*    slider        = lv_event_get_target(e);
    if (e_item == NULL) { return; }
    payload = "\"service\":\"turn_on\"";
    payload += ",\"target\":{\"entity_id\":\"";
    payload += e_item->entity_id;
    payload += "\"}";
    payload += ",\"service_data\":{\"brightness\":";
    payload += std::to_string(lv_slider_get_value(slider));
    payload += "}";
    std::string payload_str(payload);

    executeCommand(SMARTHOME_WS_LIGHT_BRIGHTNESS, payload_str);
}

void
create_tab_content_smarthome(lv_obj_t* tab) {

    // Add content to the smart home tab
    lv_obj_set_layout(tab, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_ACTIVE);

    // Add a label, then a box for the light controls
    lv_obj_t* menuLabel = lv_label_create(tab);
    lv_label_set_text(menuLabel, "Living Room");

    lv_obj_t* menuBox = lv_obj_create(tab);
    lv_obj_set_size(menuBox, lv_pct(100), 79);
    lv_obj_set_style_bg_color(menuBox, color_primary, LV_PART_MAIN);
    lv_obj_set_style_border_width(menuBox, 0, LV_PART_MAIN);

    lv_obj_t* bulbIcon = lv_img_create(menuBox);
    lv_img_set_src(bulbIcon, &lightbulb);
    lv_obj_set_style_img_recolor(bulbIcon, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(bulbIcon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(bulbIcon, LV_ALIGN_TOP_LEFT, 0, 0);

    // get the entity for the computer floor lamp, using the friendly name
    entity_item* e = friendly_map["lrfl_comp"];

    menuLabel = lv_label_create(menuBox);
    lv_label_set_text(menuLabel, e->label); //"Computer Floor\nLamp");
    lv_obj_align(menuLabel, LV_ALIGN_TOP_LEFT, 22, 3);
    e->lv_switch = lv_switch_create(menuBox);

    lv_obj_set_size(e->lv_switch, 40, 22);
    lv_obj_align(e->lv_switch, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(e->lv_switch, lv_color_lighten(color_primary, 50), LV_PART_MAIN);
    lv_obj_set_style_bg_color(e->lv_switch, color_primary, LV_PART_INDICATOR);
    if (e->state) { lv_obj_add_state(e->lv_switch, LV_STATE_CHECKED); }
    lv_obj_add_event_cb(e->lv_switch, smartHomeToggle_event_cb, LV_EVENT_VALUE_CHANGED, (void*) e->friendly_name);

    e->lv_slider = lv_slider_create(menuBox);
    lv_slider_set_range(e->lv_slider, 0, 255);
    lv_obj_set_style_bg_color(e->lv_slider, lv_color_lighten(lv_color_black(), 30), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(e->lv_slider, lv_color_lighten(lv_palette_main(LV_PALETTE_AMBER), 180), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(e->lv_slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(e->lv_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(e->lv_slider, 255, LV_PART_MAIN);
    lv_obj_set_style_bg_color(e->lv_slider, lv_color_lighten(color_primary, 50), LV_PART_MAIN);
    lv_slider_set_value(e->lv_slider, e->brightness, LV_ANIM_OFF);
    lv_obj_set_size(e->lv_slider, lv_pct(90), 10);
    lv_obj_align(e->lv_slider, LV_ALIGN_TOP_MID, 0, 37);
    lv_slider_set_value(e->lv_slider, e->brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(e->lv_slider, smartHomeSlider_event_cb, LV_EVENT_VALUE_CHANGED, (void*) e->friendly_name);

    // Add another menu box for a second appliance
    menuBox = lv_obj_create(tab);
    lv_obj_set_size(menuBox, lv_pct(100), 79);
    lv_obj_set_style_bg_color(menuBox, color_primary, LV_PART_MAIN);
    lv_obj_set_style_border_width(menuBox, 0, LV_PART_MAIN);

    bulbIcon = lv_img_create(menuBox);
    lv_img_set_src(bulbIcon, &lightbulb);
    lv_obj_set_style_img_recolor(bulbIcon, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(bulbIcon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(bulbIcon, LV_ALIGN_TOP_LEFT, 0, 0);

    // get the entity for the piano lamp, using the friendly name
    e = friendly_map["lrfl_piano"];

    menuLabel = lv_label_create(menuBox);
    lv_label_set_text(menuLabel, e->label); //"Piano Lamp"
    lv_obj_align(menuLabel, LV_ALIGN_TOP_LEFT, 22, 3);
    e->lv_switch = lv_switch_create(menuBox);

    lv_obj_set_size(e->lv_switch, 40, 22);
    lv_obj_align(e->lv_switch, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(e->lv_switch, lv_color_lighten(color_primary, 50), LV_PART_MAIN);
    lv_obj_set_style_bg_color(e->lv_switch, color_primary, LV_PART_INDICATOR);
    if (e->state) { lv_obj_add_state(e->lv_switch, LV_STATE_CHECKED); }
    lv_obj_add_event_cb(e->lv_switch, smartHomeToggle_event_cb, LV_EVENT_VALUE_CHANGED, (void*) e->friendly_name);

    e->lv_slider = lv_slider_create(menuBox);
    lv_slider_set_range(e->lv_slider, 0, 255);
    lv_obj_set_style_bg_color(e->lv_slider, lv_color_lighten(lv_color_black(), 30), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(e->lv_slider, lv_color_lighten(lv_palette_main(LV_PALETTE_AMBER), 180), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(e->lv_slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(e->lv_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(e->lv_slider, 255, LV_PART_MAIN);
    lv_obj_set_style_bg_color(e->lv_slider, lv_color_lighten(color_primary, 50), LV_PART_MAIN);
    lv_slider_set_value(e->lv_slider, e->brightness, LV_ANIM_OFF);
    lv_obj_set_size(e->lv_slider, lv_pct(90), 10);
    lv_obj_align(e->lv_slider, LV_ALIGN_TOP_MID, 0, 37);
    lv_slider_set_value(e->lv_slider, e->brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(e->lv_slider, smartHomeSlider_event_cb, LV_EVENT_VALUE_CHANGED, (void*) e->friendly_name);

    // Add another room (empty for now)
    menuLabel = lv_label_create(tab);
    lv_label_set_text(menuLabel, "Kitchen");

    menuBox = lv_obj_create(tab);
    lv_obj_set_size(menuBox, lv_pct(100), 79);
    lv_obj_set_style_bg_color(menuBox, color_primary, LV_PART_MAIN);
    lv_obj_set_style_border_width(menuBox, 0, LV_PART_MAIN);
}

void
notify_tab_before_delete_smarthome(void) {
    // remember to set all pointers to lvgl objects to NULL if they might be accessed from outside.
    // They must check if object is NULL and must not use it if so
}

void
gui_setKeys_smarthome() {
    key_commands_short_smarthome = {
        {KEY_STOP, SCENE_SELECTION},
    };
}

void
register_gui_smarthome(void) {

    register_gui(std::string(tabName_smarthome),
                 &create_tab_content_smarthome,
                 &notify_tab_before_delete_smarthome,
                 &gui_setKeys_smarthome,
                 &key_repeatModes_smarthome,
                 &key_commands_short_smarthome,
                 &key_commands_long_smarthome);

    register_command(&GUI_SMARTHOME_ACTIVATE,
                     makeCommandData(GUI, {std::to_string(MAIN_GUI_LIST), std::string(tabName_smarthome)}));

    // Setup our entities, and data that should persist if we navigate away from the screen
    init_entity_maps();
}

void
parseAllEntities(cJSON* root) {

    omote_log_d("Parsing entry\r\n");
    cJSON* event   = cJSON_GetObjectItem(root, "event");
    cJSON* changes = cJSON_GetObjectItem(event, "c");
    if (changes && cJSON_IsObject(changes)) {
        // omote_log_d("Found Change\r\n");
        // Parse ALL changed entities
        cJSON* entity;
        cJSON_ArrayForEach(entity, changes) {
            const char* entity_id = entity->string;
            omote_log_d("Parsing change item %s\r\n", entity_id);
            cJSON* new_state = cJSON_GetObjectItem(entity, "+");
            if (!new_state) continue;

            // Get state
            cJSON*      state_json = cJSON_GetObjectItem(new_state, "s");
            const char* state      = state_json && cJSON_IsString(state_json) ? state_json->valuestring : "unknown";

            // Get brightness (0 if not found)
            int    brightness = -1;
            cJSON* attrs      = cJSON_GetObjectItem(new_state, "a");
            if (attrs) {
                cJSON* brightness_json = cJSON_GetObjectItem(attrs, "brightness");
                if (brightness_json && cJSON_IsNumber(brightness_json)) { brightness = (int) brightness_json->valuedouble; }
                omote_log_d("Parsing brightness %d %f\r\n", brightness, brightness_json->valuedouble);
            }
            // look up lv objects, based on entity id
            entity_item* e = entity_map[entity_id];
            if (!e) continue; // entity not in our table, ignore

            if (strcmp(state, "unknown")) { // if we got a value, update it!
                e->state = (strcmp(state, "on") == 0);
                if (e->lv_switch) {
                    if (e->state) {
                        omote_log_d("Set state on\r\n");
                        lv_obj_add_state(e->lv_switch, LV_STATE_CHECKED);
                    } else {
                        omote_log_d("Set state off\r\n");
                        lv_obj_clear_state(e->lv_switch, LV_STATE_CHECKED);
                        brightness = 0; // if state is off, brightness should be 0, even if HA did not send brightness in the event
                    }
                }
            }

            if (brightness >= 0) {
                e->brightness = brightness;
                if (e->lv_slider) { lv_slider_set_value(e->lv_slider, brightness, LV_ANIM_OFF); }
            }

            omote_log_d("Parsing change item %s (%s): %s (%s) %d\r\n",
                        entity_id,
                        e->friendly_name,
                        state,
                        e->state ? "on" : "off",
                        e->brightness);
        }
    }
    cJSON* attrs = cJSON_GetObjectItem(event, "a");
    if (attrs && cJSON_IsObject(attrs)) {
        // omote_log_d("Found Attrs\r\n");
        // Parse ALL attributes
        cJSON* entity;
        cJSON_ArrayForEach(entity, attrs) {
            const char* entity_id = entity->string;
            omote_log_d("Parsing  attr item %s\r\n", entity_id);

            // Get state
            cJSON*      state_json = cJSON_GetObjectItem(entity, "s");
            const char* state      = state_json && cJSON_IsString(state_json) ? state_json->valuestring : "unknown";

            // Get brightness (0 if not found)
            int    brightness = -1;
            cJSON* attrs      = cJSON_GetObjectItem(entity, "a");
            if (attrs) {
                cJSON* brightness_json = cJSON_GetObjectItem(attrs, "brightness");
                if (brightness_json && cJSON_IsNumber(brightness_json)) { brightness = (int) brightness_json->valuedouble; }
            }

            // look up lv objects, based on entity id
            entity_item* e = entity_map[entity_id];
            if (!e) continue; // entity not in our table, ignore

            e->state = (strcmp(state, "on") == 0);

            if (strcmp(state, "unknown")) {
                if (e->lv_switch) {
                    if (e->state) {
                        lv_obj_add_state(e->lv_switch, LV_STATE_CHECKED);
                    } else {
                        lv_obj_clear_state(e->lv_switch, LV_STATE_CHECKED);
                        brightness = 0; // if state is off, brightness should be 0, even if HA did not send brightness in the event
                    }
                }
            }
            if (brightness >= 0) {
                e->brightness = brightness;

                if (e->lv_slider) { lv_slider_set_value(e->lv_slider, brightness, LV_ANIM_OFF); }
            }
            omote_log_d("Parsing  attr item %s: %s(%d) %d\r\n", entity_id, state, e->state, e->brightness);
        }
    }
}

static uint16_t ws_id = 999;
void
handle_HA_websocket_message(std::string message) {
    uint8_t i = 0;
    omote_log_d("Smart home gui received websocket message: '%s'\r\n", message.c_str());
    cJSON* root = cJSON_Parse(message.c_str());
    if (!root) return; // Invalid JSON
    cJSON* type = cJSON_GetObjectItem(root, "type");
    // if an auth event
    if (type && cJSON_IsString(type) && strcmp(type->valuestring, "auth_ok") == 0) {
        // Subscribe to all events, just build the message here, we will send it in the function "websocket_sub_HAL"
        cJSON* sub = cJSON_CreateObject();
        cJSON_AddNumberToObject(sub, "id", ++ws_id %= 10000);
        cJSON_AddStringToObject(sub, "type", "subscribe_entities");

        cJSON* entityArray = cJSON_AddArrayToObject(sub, "entity_ids");

        for (const auto& pair : entity_map) { cJSON_AddItemToArray(entityArray, cJSON_CreateString(pair.first.c_str())); }

        //       while (event_id_table[++i] != NULL) { cJSON_AddItemToArray(entityArray, cJSON_CreateString(event_id_table[i])); }
        char* msg = cJSON_PrintUnformatted(sub);
        websocket_sub(msg);
        cJSON_Delete(sub);
        free(msg);
    } else {
        parseAllEntities(root);
    }
    cJSON_Delete(root);
}