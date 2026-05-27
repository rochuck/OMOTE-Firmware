#include <lvgl.h>
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "applicationInternal/scenes/sceneRegistry.h"
#include "applicationInternal/commandHandler.h"
#include "applicationInternal/omote_log.h"
#include "guis/gui_sceneSelection.h"
#include "scenes/scene_appleTV.h"
#include "scenes/scene_kodi.h"
#include "scenes/scene_kodi_BT.h"
#include "scenes/scene_lyrion.h"

LV_IMG_DECLARE(kodiSelectIcon);
LV_IMG_DECLARE(appleSelectIcon);
LV_IMG_DECLARE(lyrionSelectIcon);

static const lv_img_dsc_t* icon_for_scene(const std::string& scene_name) {
  if (scene_name == scene_name_appleTV) return &appleSelectIcon;
  if (scene_name == scene_name_kodi)    return &kodiSelectIcon;
  if (scene_name == scene_name_kodi_BT) return &kodiSelectIcon;
  if (scene_name == scene_name_lyrion)  return &lyrionSelectIcon;
  return NULL;
}

// Tint colors match the colored hotkeys that activate each scene
// (see scene__default.cpp: RED→AppleTV, GREEN→Kodi, YELLOW→Kodi_BT, BLUE→Lyrion)
static bool tint_for_scene(const std::string& scene_name, lv_color_t* out_color) {
  if (scene_name == scene_name_appleTV) { *out_color = lv_color_hex(0xFF3030); return true; }
  if (scene_name == scene_name_kodi)    { *out_color = lv_color_hex(0x30D030); return true; }
  if (scene_name == scene_name_kodi_BT) { *out_color = lv_color_hex(0xFFD030); return true; }
  if (scene_name == scene_name_lyrion)  { *out_color = lv_color_hex(0x4090FF); return true; }
  return false;
}

static uint16_t activate_scene_command;
static bool doForceScene;
//void activate_scene_async(void *command) {
//  executeCommand(activate_scene_command);
//}
void activate_scene_cb(lv_timer_t *timer) {
  uint16_t scene_command_including_force = (uintptr_t)(timer->user_data);
  // get the force flag from the highest bit
  uint16_t activate_scene_command = scene_command_including_force & 0x7FFF;
  bool doForceScene = (scene_command_including_force & 0x8000) == 0x8000;
  if (doForceScene) {
    executeCommand(activate_scene_command, "FORCE");
  } else {
    executeCommand(activate_scene_command);
  }
}

static int lastShortClickedReceived;
static unsigned long int lastShortClickedReceivedTime;

static void sceneSelection_event_cb(lv_event_t* e) {

  int user_data = (intptr_t)(e->user_data);

  // we will receive the following events in that order:
  // LV_EVENT_PRESSED
  // LV_EVENT_RELEASED
  // only on short press: LV_EVENT_SHORT_CLICKED
  // both on short press and long press: LV_EVENT_CLICKED
  // if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
  //   omote_log_v("pressed\r\n");
  // }
  // if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
  //   omote_log_v("released\r\n");
  // }
  if (lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED) {
    lastShortClickedReceived = user_data;
    lastShortClickedReceivedTime = millis(); 
    omote_log_v("short clicked, will see what happens next\r\n");
    return;

  } else if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if ((lastShortClickedReceived == user_data) && (millis() - lastShortClickedReceivedTime < 10)) {
      omote_log_v("clicked, will send short click\r\n");
      doForceScene = false;
    } else {
      omote_log_v("clicked, will send long click\r\n");
      doForceScene = true;
    }
  } else {
    return;
  }

  std::string scene_name = get_scenes_on_sceneSelectionGUI()->at(user_data);

  activate_scene_command = get_activate_scene_command(scene_name);
  if (activate_scene_command != 0) {
    // this line is needed
    if (SceneLabel != NULL) {lv_label_set_text(SceneLabel, "changing...");}
    // Problem: screen will not get updated and show "changing..." if "executeCommand(activate_scene_command);" is called here
    // test 1 (does not work): call lv_timer_handler();
    // lv_timer_handler();
    // test 2 (does not work): async_call
    // lv_async_call(activate_scene_async, &activate_scene_command);
    // test 3: lv_timer_create()
    // needs to run only once, and a very short period of 5 ms to wait until first run is enough
    
    uint16_t scene_command_including_force;
    if (doForceScene) {
      // put the force flag into the highest bit
      scene_command_including_force = activate_scene_command | 0x8000;
      omote_log_d("Scene with index %d and name %s was FORCE selected\r\n", user_data, scene_name.c_str());
    } else {
      scene_command_including_force = activate_scene_command;
      omote_log_d("Scene with index %d and name %s was selected\r\n", user_data, scene_name.c_str());
    }
    lv_timer_t *my_timer = lv_timer_create(activate_scene_cb, 50, (void *)(uintptr_t) scene_command_including_force);
    lv_timer_set_repeat_count(my_timer, 1);

  } else {
    omote_log_w("Cannot activate scene %s, because command was not found\r\n", scene_name.c_str());
  }
}

void create_tab_content_sceneSelection(lv_obj_t* tab) {

  // Add content to the sceneSelection tab

  lv_obj_set_layout(tab, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_ACTIVE);

  // -- create a button for each scene ----------------------------------------
  scene_list scenes = get_scenes_on_sceneSelectionGUI();
  if ((scenes != NULL) && (scenes->size() > 0)) {
    for (int i=0; i<scenes->size(); i++) {
      lv_obj_t* button = lv_btn_create(tab);
      lv_obj_set_size(button, lv_pct(100), 42);
      lv_obj_set_style_radius(button, 30, LV_PART_MAIN);
      lv_obj_set_style_bg_color(button, color_primary, LV_PART_MAIN);
      lv_obj_add_event_cb(button, sceneSelection_event_cb, LV_EVENT_CLICKED,       (void *)(intptr_t)i);
      lv_obj_add_event_cb(button, sceneSelection_event_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);

      const lv_img_dsc_t* icon_src = icon_for_scene(scenes->at(i));
      if (icon_src != NULL) {
        lv_obj_t* icon = lv_img_create(button);
        lv_img_set_src(icon, icon_src);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_add_flag(icon, LV_OBJ_FLAG_FLOATING);
        lv_color_t tint;
        if (tint_for_scene(scenes->at(i), &tint)) {
          lv_obj_set_style_img_recolor(icon, tint, LV_PART_MAIN);
          lv_obj_set_style_img_recolor_opa(icon, LV_OPA_70, LV_PART_MAIN);
        }
      }

      lv_obj_t* label = lv_label_create(button);
      lv_label_set_text(label, scenes->at(i).c_str());
      lv_obj_center(label);
      }
  }
  
}

void notify_tab_before_delete_sceneSelection(void) {
  // remember to set all pointers to lvgl objects to NULL if they might be accessed from outside.
  // They must check if object is NULL and must not use it if so
  
}

void register_gui_sceneSelection(void){
  register_gui(std::string(tabName_sceneSelection), & create_tab_content_sceneSelection, & notify_tab_before_delete_sceneSelection);
}
