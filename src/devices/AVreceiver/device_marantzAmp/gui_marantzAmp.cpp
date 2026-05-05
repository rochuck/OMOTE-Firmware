#include "devices/AVreceiver/device_marantzAmp/gui_marantzAmp.h"
#include "applicationInternal/commandHandler.h"
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "devices/AVreceiver/device_marantzAmp/device_marantzAmp.h"
#include "devices/TV/device_sharpTV/device_sharpTV.h"
#include "devices/mediaPlayer/device_appleTV/device_appleTV.h"
#if (ENABLE_COMPANION == 1)
#include "devices/mediaPlayer/device_appleTV_companion/device_appleTV_companion.h"
#endif
#include <lvgl.h>

static void
button_clicked_event_cb(lv_event_t* e) {
    int user_data = (intptr_t) (e->user_data);

    if (user_data == 0) { executeCommand(MARANTZ_SDIRECT); }
    if (user_data == 1) { executeCommand(MARANTZ_POWER_ON); }
    if (user_data == 2) { executeCommand(MARANTZ_POWER_OFF); }
    if (user_data == 3) { executeCommand(SHARP_POWER_ON); }
    if (user_data == 4) { executeCommand(SHARP_POWER_OFF); }
    if (user_data == 5) { executeCommand(APPLETV_POWER_ON); }
    if (user_data == 6) { executeCommand(APPLETV_POWER_OFF); }
#if (ENABLE_COMPANION == 1)
    if (user_data == 7) { executeCommand(COMPANION_LAUNCH_HDHOMERUN); }
#endif
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

    // -- create a button for "Sharp ON" ----------------------------------------
    lv_obj_t* button6 = lv_btn_create(ui_Image1);
    lv_obj_set_size(button6, 80, 40);
    lv_obj_set_style_radius(button6, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button6, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(button6, button_clicked_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) 3);

    lv_obj_t* label6 = lv_label_create(button6);
    lv_label_set_text(label6, "Sharp ON");
    lv_obj_center(label6);

    // -- create a button for "Sharp OFF" ----------------------------------------
    lv_obj_t* button7 = lv_btn_create(ui_Image1);
    lv_obj_set_size(button7, 80, 40);
    lv_obj_set_style_radius(button7, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button7, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(button7, button_clicked_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) 4);

    lv_obj_t* label7 = lv_label_create(button7);
    lv_label_set_text(label7, "Sharp OFF");
    lv_obj_center(label7);

    // -- create a button for "Apple TV ON" on top right ----------------------------------------
    lv_obj_t* button_appletv_on = lv_btn_create(ui_Image1);
    lv_obj_add_flag(button_appletv_on, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(button_appletv_on, 80, 40);
    lv_obj_align(button_appletv_on, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_radius(button_appletv_on, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button_appletv_on, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(button_appletv_on, button_clicked_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) 5);

    lv_obj_t* label_appletv_on = lv_label_create(button_appletv_on);
    lv_label_set_text(label_appletv_on, "ATV ON");
    lv_obj_center(label_appletv_on);

    // -- create a button for "Apple TV OFF" on top right ----------------------------------------
    lv_obj_t* button_appletv_off = lv_btn_create(ui_Image1);
    lv_obj_add_flag(button_appletv_off, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(button_appletv_off, 80, 40);
    lv_obj_align(button_appletv_off, LV_ALIGN_TOP_RIGHT, -5, 50);
    lv_obj_set_style_radius(button_appletv_off, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button_appletv_off, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(button_appletv_off, button_clicked_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) 6);

    lv_obj_t* label_appletv_off = lv_label_create(button_appletv_off);
    lv_label_set_text(label_appletv_off, "ATV OFF");
    lv_obj_center(label_appletv_off);

#if (ENABLE_COMPANION == 1)
    // -- create a button for "HDHR" launching HDHomeRun via companion ----------------------------------------
    lv_obj_t* button_hdhr = lv_btn_create(ui_Image1);
    lv_obj_add_flag(button_hdhr, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(button_hdhr, 80, 40);
    lv_obj_align(button_hdhr, LV_ALIGN_TOP_RIGHT, -5, 95);
    lv_obj_set_style_radius(button_hdhr, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button_hdhr, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(button_hdhr, button_clicked_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) 7);

    lv_obj_t* label_hdhr = lv_label_create(button_hdhr);
    lv_label_set_text(label_hdhr, "HDHR");
    lv_obj_center(label_hdhr);
#endif
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
