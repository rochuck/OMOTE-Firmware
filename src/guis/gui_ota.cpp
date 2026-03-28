#include "guis/gui_ota.h"

#if (ENABLE_OTA == 1)
#include <lvgl.h>
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/hardware/hardwarePresenter.h"

static lv_obj_t* ota_overlay = NULL;
static lv_obj_t* ota_bar     = NULL;
static lv_obj_t* ota_pct_lbl = NULL;
static lv_obj_t* ota_status  = NULL;

void ota_gui_start(void) {
    if (ota_overlay != NULL) { return; }

    ota_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ota_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(ota_overlay, 0, 0);
    lv_obj_set_style_bg_color(ota_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ota_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ota_overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ota_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(ota_overlay);
    lv_label_set_text(title, "FIRMWARE UPDATE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

    ota_bar = lv_bar_create(ota_overlay);
    lv_obj_set_size(ota_bar, 200, 16);
    lv_bar_set_range(ota_bar, 0, 100);
    lv_bar_set_value(ota_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ota_bar, lv_color_hex(0x505050), LV_PART_MAIN);
    lv_obj_align(ota_bar, LV_ALIGN_CENTER, 0, -20);

    ota_pct_lbl = lv_label_create(ota_overlay);
    lv_label_set_text(ota_pct_lbl, "0%");
    lv_obj_set_style_text_color(ota_pct_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(ota_pct_lbl, LV_ALIGN_CENTER, 0, 10);

    ota_status = lv_label_create(ota_overlay);
    lv_label_set_text(ota_status, "Receiving...");
    lv_obj_set_style_text_color(ota_status, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(ota_status, LV_ALIGN_CENTER, 0, 35);

    // Force LVGL to render the overlay immediately — the upload runs
    // synchronously inside handleClient(), so the main loop never reaches
    // gui_loop() until after the reboot.
    lv_timer_handler();
}

void ota_gui_set_progress(int pct) {
    // Keep device awake during update
    setLastActivityTimestamp();

    if (ota_overlay == NULL) { return; }

    if (pct < 0) {
        lv_label_set_text(ota_status, "Update failed");
        lv_obj_set_style_text_color(ota_status, lv_color_hex(0xFF4040), LV_PART_MAIN);
        return;
    }

    lv_bar_set_value(ota_bar, pct, LV_ANIM_OFF);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(ota_pct_lbl, buf);

    lv_timer_handler();
}

#endif
