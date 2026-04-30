#include "devices/AVreceiver/device_marantzAmp/gui_marantzAmp.h"
#include "applicationInternal/commandHandler.h"
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "devices/AVreceiver/device_marantzAmp/device_marantzAmp.h"
#include <lvgl.h>

static void
button_clicked_event_cb(lv_event_t* e) {
    int user_data = (intptr_t) (e->user_data);

    if (user_data == 0) { executeCommand(MARANTZ_SDIRECT); }
    if (user_data == 1) { executeCommand(MARANTZ_POWER_ON); }
    if (user_data == 2) { executeCommand(MARANTZ_POWER_OFF); }
}

lv_obj_t* ui_Image1;
LV_IMG_DECLARE(marantz);

void
create_tab_content_marantzAmp(lv_obj_t* tab) {

    // Add content to the sceneSelection tab

    lv_obj_set_layout(tab, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_ACTIVE);

    ui_Image1 = lv_img_create(tab);
    lv_img_set_src(ui_Image1, &marantz);
    lv_obj_set_width(ui_Image1, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_Image1, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_Image1, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_Image1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_Image1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_Image1, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    // -- create a button for "S. Direct" ----------------------------------------
    lv_obj_t* button = lv_btn_create(ui_Image1);
    lv_obj_set_size(button, 80, 40);
    lv_obj_set_style_radius(button, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(button, button_clicked_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) 0);

    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, "S. Direct");
    lv_obj_center(label);

    // -- create a button for "OFF" ----------------------------------------
    lv_obj_t* button3 = lv_btn_create(ui_Image1);
    lv_obj_set_size(button3, 80, 40);
    lv_obj_set_style_radius(button3, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button3, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(button3, button_clicked_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) 2);

    lv_obj_t* label3 = lv_label_create(button3);
    lv_label_set_text(label3, "OFF");
    lv_obj_center(label3);

    // -- create a button for "ON" ----------------------------------------
    lv_obj_t* button5 = lv_btn_create(ui_Image1);
    lv_obj_set_size(button5, 80, 40);
    lv_obj_set_style_radius(button5, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button5, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(button5, button_clicked_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) 1);

    lv_obj_t* label5 = lv_label_create(button5);
    lv_label_set_text(label5, "ON");
    lv_obj_center(label5);
}

void
notify_tab_before_delete_marantzAmp(void) {
    // remember to set all pointers to lvgl objects to NULL if they might be accessed from outside.
    // They must check if object is NULL and must not use it if so
}

void
register_gui_marantzAmp(void) {
    register_gui(std::string(tabName_marantzAmp), &create_tab_content_marantzAmp, &notify_tab_before_delete_marantzAmp);
}
