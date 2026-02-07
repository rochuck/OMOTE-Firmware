#include <lvgl.h>
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "applicationInternal/omote_log.h"
#include "devices/mediaPlayer/device_hdhomerun/gui_hdhomerun.h"

#include "applicationInternal/commandHandler.h"
#include "devices/mediaPlayer/device_hdhomerun/device_hdhomerun.h"

// LVGL declarations
LV_IMG_DECLARE(hdhomerunIcon);
LV_IMG_DECLARE(hdhomerunDisplayIcon);
LV_IMG_DECLARE(hdhomerunBackIcon);

// HDHomeRun Key Event handler
static void hdhomerunKey_event_cb(lv_event_t* e) {
  // Send IR command based on the event user data  
  int user_data = *((int*)(&(e->user_data)));

  omote_log_v("hdhomerunKey_event_cb: Event Id: '%d'.\r\n", user_data);

  if (user_data == 2)
  {
    executeCommand(HDHOMERUN_HOME);
  }
  else if (user_data == 1)
  {
    executeCommand(HDHOMERUN_MENU);
  }
}

void create_tab_content_hdhomerun(lv_obj_t* tab) {

  // Add content to the HDHomeRun tab
  // Add a nice hdhomerun logo
  lv_obj_t* hdhomerunImg = lv_img_create(tab);
  lv_img_set_src(hdhomerunImg, &hdhomerunIcon);
  lv_obj_align(hdhomerunImg, LV_ALIGN_CENTER, 0, -60);
  // create two buttons and add their icons accordingly
  lv_obj_t* button = lv_btn_create(tab);
  lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, 10, 0);
  lv_obj_set_size(button, 60, 60);
  lv_obj_set_style_radius(button, 30, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, color_primary, LV_PART_MAIN);
  lv_obj_add_event_cb(button, hdhomerunKey_event_cb, LV_EVENT_CLICKED, (void*)1);

  hdhomerunImg = lv_img_create(button);
  lv_img_set_src(hdhomerunImg, &hdhomerunBackIcon);
  lv_obj_set_style_img_recolor(hdhomerunImg, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_img_recolor_opa(hdhomerunImg, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(hdhomerunImg, LV_ALIGN_CENTER, -3, 0);

  button = lv_btn_create(tab);
  lv_obj_align(button, LV_ALIGN_BOTTOM_RIGHT, -10, 0);
  lv_obj_set_size(button, 60, 60);
  lv_obj_set_style_radius(button, 30, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, color_primary, LV_PART_MAIN);
  lv_obj_add_event_cb(button, hdhomerunKey_event_cb, LV_EVENT_CLICKED, (void*)2);

  hdhomerunImg = lv_img_create(button);
  lv_img_set_src(hdhomerunImg, &hdhomerunDisplayIcon);
  lv_obj_set_style_img_recolor(hdhomerunImg, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_img_recolor_opa(hdhomerunImg, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(hdhomerunImg, LV_ALIGN_CENTER, 0, 0);

}

void notify_tab_before_delete_hdhomerun(void) {
  // remember to set all pointers to lvgl objects to NULL if they might be accessed from outside.
  // They must check if object is NULL and must not use it if so

}

void register_gui_hdhomerun(void){
  register_gui(std::string(tabName_hdhomerun), & create_tab_content_hdhomerun, & notify_tab_before_delete_hdhomerun);
}
