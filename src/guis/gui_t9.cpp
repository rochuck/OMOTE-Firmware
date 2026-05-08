#include "guis/gui_t9.h"
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/omote_log.h"
#include <cstring>
#include <lvgl.h>
#include <string>

#include "applicationInternal/commandHandler.h"
#include "applicationInternal/scenes/sceneHandler.h"
#if (ENABLE_KODI == 1)
#include "devices/mediaPlayer/device_kodi/device_kodi.h"
#endif

// Multi-tap (classic phone T9) state. After COMMIT_MS of inactivity, or when a
// different key is pressed, the in-progress letter is committed and a new tap
// starts a fresh letter. Tapping the same key cycles through its alphabet,
// replacing the last char in the textarea.
static const uint32_t COMMIT_MS = 800;

static const char* const T9_LETTERS[9] = {
    // index 0 = digit 1 (no letters), 1 = digit 2 ABC, ..., 8 = digit 9 WXYZ
    "1", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz",
};

static lv_obj_t*   s_ta          = nullptr;
static lv_timer_t* s_commitTimer = nullptr;
static int8_t      s_lastKey     = -1; // 0..8 keys with letters; -1 = none pending
static uint8_t     s_cycleIdx    = 0;

static void
cancel_commit_timer(void) {
    if (s_commitTimer != nullptr) {
        lv_timer_del(s_commitTimer);
        s_commitTimer = nullptr;
    }
}

static void
commit_pending(void) {
    cancel_commit_timer();
    s_lastKey  = -1;
    s_cycleIdx = 0;
}

static void
commit_timer_cb(lv_timer_t* t) {
    s_commitTimer = nullptr; // LVGL deletes the timer when repeat_count hits 0
    s_lastKey     = -1;
    s_cycleIdx    = 0;
    (void) t;
}

static void
start_commit_timer(void) {
    cancel_commit_timer();
    s_commitTimer = lv_timer_create(commit_timer_cb, COMMIT_MS, NULL);
    lv_timer_set_repeat_count(s_commitTimer, 1);
}

static void
replace_last_char(char c) {
    if (s_ta == nullptr) return;
    lv_textarea_del_char(s_ta);
    lv_textarea_add_char(s_ta, c);
}

#if (ENABLE_KODI == 1)
static std::string
json_escape(const char* s) {
    std::string out;
    for (const char* p = s; *p; ++p) {
        unsigned char c = (unsigned char) *p;
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char) c;
                }
        }
    }
    return out;
}
#endif

// Virtual T9 Keypad event handler. user_data on each button is its index 0..11:
//   0..8 -> digits "1".."9" (with letters on 1..8)
//   9    -> BACKSPACE
//   10   -> "0"
//   11   -> OK (send buffer to Kodi via Input.SendText)
static void
virtualT9_event_cb(lv_event_t* e) {
    lv_obj_t* target = lv_event_get_target(e);
    lv_obj_t* cont   = lv_event_get_current_target(e);
    if (target == cont) return; // container itself was clicked
    if (s_ta == nullptr) return;

    int idx = (intptr_t) (target->user_data);

    // Letter keys (and digit 1, which has no letters)
    if (idx >= 0 && idx <= 8) {
        if (idx == 0) {
            commit_pending();
            lv_textarea_add_char(s_ta, '1');
            return;
        }
        if (s_lastKey == idx) {
            s_cycleIdx = (s_cycleIdx + 1) % strlen(T9_LETTERS[idx]);
            replace_last_char(T9_LETTERS[idx][s_cycleIdx]);
        } else {
            commit_pending();
            s_lastKey  = idx;
            s_cycleIdx = 0;
            lv_textarea_add_char(s_ta, T9_LETTERS[idx][0]);
        }
        start_commit_timer();
        return;
    }

    if (idx == 10) {
        commit_pending();
        lv_textarea_add_char(s_ta, '0');
        return;
    }

    if (idx == 9) { // BACKSPACE
        commit_pending();
        lv_textarea_del_char(s_ta);
        return;
    }

    if (idx == 11) { // OK
        commit_pending();
        const char* text = lv_textarea_get_text(s_ta);
        if (text != nullptr && text[0] != '\0') {
#if (ENABLE_KODI == 1)
            std::string payload = std::string("{\"text\":\"") + json_escape(text) + "\",\"done\":true}";
            executeCommand(KODI_SEND_TEXT, payload);
#else
            omote_log_w("gui_t9: OK pressed but ENABLE_KODI is off; nothing to send\r\n");
#endif
        }
        lv_textarea_set_text(s_ta, "");
        return;
    }
}

void
create_tab_content_t9(lv_obj_t* tab) {

    static const char* kb_map[] = {"1",
                                   "2\nABC",
                                   "3\nDEF",
                                   "4\nGHI",
                                   "5\nJKL",
                                   "6\nMNO",
                                   "7\nPQRS",
                                   "8\nTUV",
                                   "9\nWXYZ",
                                   LV_SYMBOL_BACKSPACE,
                                   "0",
                                   LV_SYMBOL_OK,
                                   NULL};
    static lv_coord_t  col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t  row_dsc[] = {50, 50, 50, 50, LV_GRID_TEMPLATE_LAST};

    lv_obj_t* ta = lv_textarea_create(tab);
    lv_obj_set_size(ta, 200, 10);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 0);
    lv_textarea_set_one_line(ta, true);
    s_ta = ta;

    lv_obj_set_style_pad_all(tab, 0, LV_PART_MAIN);
    lv_obj_t* cont = lv_obj_create(tab);
    lv_obj_set_style_shadow_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cont, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);
    lv_obj_set_size(cont, SCR_WIDTH, 270);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_radius(cont, 0, LV_PART_MAIN);

    lv_obj_t* buttonLabel;
    lv_obj_t* obj;

    for (int i = 0; i < 12; i++) {
        uint8_t col = i % 3;
        uint8_t row = i / 3;
        obj         = lv_btn_create(cont);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
        lv_obj_set_style_bg_color(obj, color_primary, LV_PART_MAIN);
        lv_obj_set_style_radius(obj, 14, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(obj, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
        buttonLabel = lv_label_create(obj);
        lv_label_set_text(buttonLabel, kb_map[i]);
        lv_obj_set_style_text_align(buttonLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_user_data(obj, (void*) (intptr_t) i);
        lv_obj_set_style_text_font(buttonLabel, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_center(buttonLabel);
    }
    lv_obj_add_event_cb(cont, virtualT9_event_cb, LV_EVENT_CLICKED, NULL);
}

void
notify_tab_before_delete_t9(void) {
    cancel_commit_timer();
    s_ta       = nullptr;
    s_lastKey  = -1;
    s_cycleIdx = 0;
}

void
register_gui_t9(void) {
    register_gui(std::string(tabName_t9), &create_tab_content_t9, &notify_tab_before_delete_t9);
}
