#include "devices/mediaPlayer/device_appleTV/gui_appleTV.h"
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/omote_log.h"
#include <lvgl.h>

#include "applicationInternal/commandHandler.h"
#include "devices/mediaPlayer/device_appleTV/device_appleTV.h"
#if (ENABLE_COMPANION == 1)
#include "devices/mediaPlayer/device_appleTV_companion/device_appleTV_companion.h"
#endif

// LVGL declarations
LV_IMG_DECLARE(hdhomerunIcon);
LV_IMG_DECLARE(appleTVBackIcon);
LV_IMG_DECLARE(appleTVDisplayIcon);
LV_IMG_DECLARE(youtubeIcon);

// Launcher cell identifiers
enum launcherCell {
    CELL_YOUTUBE = 0,
    CELL_HDHOMERUN,
    CELL_PRIME,
    CELL_APPLETV_PLUS,
    CELL_IMMICH,
    CELL_TSN,
    CELL_BACK,
    CELL_MENU,
};

static void
launcher_event_cb(lv_event_t* e) {
    int user_data = *((int*) (&(e->user_data)));

    omote_log_v("launcher_event_cb: Event Id: '%d'.\r\n", user_data);

    switch (user_data) {
#if (ENABLE_COMPANION == 1)
        case CELL_YOUTUBE:      executeCommand(COMPANION_LAUNCH_YOUTUBE);      break;
        case CELL_HDHOMERUN:    executeCommand(COMPANION_LAUNCH_HDHOMERUN);    break;
        case CELL_PRIME:        executeCommand(COMPANION_LAUNCH_PRIMEVIDEO);   break;
        case CELL_APPLETV_PLUS: executeCommand(COMPANION_LAUNCH_APPLETV_PLUS); break;
        // TODO: wire to COMPANION_LAUNCH_CUSTOM with bundle id once known
        case CELL_IMMICH:                                                      break;
        case CELL_TSN:                                                         break;
#endif
        case CELL_BACK:         executeCommand(APPLETV_MENU);                  break;
        case CELL_MENU:         executeCommand(APPLETV_HOME);                  break;
    }
}

static lv_obj_t*
make_cell(lv_obj_t* parent, uint8_t col, uint8_t row, int cell_id) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_radius(btn, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, color_primary, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, launcher_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) cell_id);
    return btn;
}

static void
add_label(lv_obj_t* btn, const char* text) {
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}

static void
add_icon(lv_obj_t* btn, const lv_img_dsc_t* src, uint16_t zoom, bool recolor_white) {
    lv_obj_t* img = lv_img_create(btn);
    lv_img_set_src(img, src);
    if (zoom != 256) {
        lv_img_set_zoom(img, zoom);
    }
    if (recolor_white) {
        lv_obj_set_style_img_recolor(img, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN);
    }
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}

void
create_tab_content_appleTV(lv_obj_t* tab) {
    static lv_coord_t col_dsc[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t row_dsc[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };

    lv_obj_t* grid = lv_obj_create(tab);
    lv_obj_set_size(grid, SCR_WIDTH, 270);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(grid, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(grid, row_dsc, 0);

    // Row 0: YouTube, HDHomeRun
    add_icon(make_cell(grid, 0, 0, CELL_YOUTUBE),   &youtubeIcon,   56, false);
    add_icon(make_cell(grid, 1, 0, CELL_HDHOMERUN), &hdhomerunIcon, 56, false);

    // Row 1: Prime Video, Apple TV+
    add_label(make_cell(grid, 0, 1, CELL_PRIME),        "PRIME");
    add_label(make_cell(grid, 1, 1, CELL_APPLETV_PLUS), "TV+");

    // Row 2: Immich, TSN
    add_label(make_cell(grid, 0, 2, CELL_IMMICH), "IMMICH");
    add_label(make_cell(grid, 1, 2, CELL_TSN),    "TSN");

    // Row 3: back, menu
    add_icon(make_cell(grid, 0, 3, CELL_BACK), &appleTVBackIcon,    256, true);
    add_icon(make_cell(grid, 1, 3, CELL_MENU), &appleTVDisplayIcon, 256, true);
}

void
notify_tab_before_delete_appleTV(void) {
    // remember to set all pointers to lvgl objects to NULL if they might be accessed from outside.
    // They must check if object is NULL and must not use it if so
}

void
register_gui_appleTV(void) {
    register_gui(std::string(tabName_appleTV), &create_tab_content_appleTV, &notify_tab_before_delete_appleTV);
}
