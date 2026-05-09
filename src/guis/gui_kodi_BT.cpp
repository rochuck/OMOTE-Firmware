#include "guis/gui_kodi_BT.h"
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/omote_log.h"
#include <lvgl.h>

LV_IMG_DECLARE(kodiBigIcon);

static void
reboot_event_cb(lv_event_t* e) {
    omote_log_i("gui_kodi_BT: reboot button pressed (BT reboot sequence not yet implemented)\r\n");
}

void
create_tab_content_kodi_BT(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 0, LV_PART_MAIN);

    lv_obj_t* bg = lv_img_create(tab);
    lv_img_set_src(bg, &kodiBigIcon);
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_img_opa(bg, LV_OPA_50, LV_PART_MAIN);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* btn = lv_btn_create(tab);
    lv_obj_set_size(btn, 160, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_radius(btn, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, reboot_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "Reboot");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(label);
}

void
notify_tab_before_delete_kodi_BT(void) {
}

void
register_gui_kodi_BT(void) {
    register_gui(std::string(tabName_kodi_BT), &create_tab_content_kodi_BT, &notify_tab_before_delete_kodi_BT);
}
