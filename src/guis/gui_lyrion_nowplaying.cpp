#include "guis/gui_lyrion_nowplaying.h"

#if (ENABLE_LYRION == 1)

#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/omote_log.h"
#include <lvgl.h>

// Future browse tab will live next to this one in scene_lyrion's gui_list.
// Keep this GUI focused on the now-playing view; do not embed library
// browsing here.

static lv_obj_t* s_player_label = nullptr;
static lv_obj_t* s_power_icon   = nullptr; // top-left: green=on, gray=off
static lv_obj_t* s_play_icon    = nullptr; // top-right: play / pause glyph
static lv_obj_t* s_art_panel    = nullptr; // gray rounded fallback container
static lv_obj_t* s_art_img      = nullptr; // album art (lv_img); hidden when no art
static lv_obj_t* s_art_glyph    = nullptr; // ♪ shown when no art
static lv_obj_t* s_title_label  = nullptr;
static lv_obj_t* s_artist_label = nullptr;
static lv_obj_t* s_album_label  = nullptr;
static lv_obj_t* s_progress_bar = nullptr;
static lv_obj_t* s_time_elapsed = nullptr;
static lv_obj_t* s_time_remain  = nullptr;
static lv_timer_t* s_poll_timer = nullptr;

static std::string s_displayed_track_id;
static std::string s_displayed_player;
static int         s_displayed_volume = -2; // -2 forces first paint
static int         s_displayed_powered = -1; // -1 forces first paint
static int         s_displayed_playing = -1; // -1 forces first paint

static void
format_mmss(int total_seconds, char* out, size_t out_len, bool negative) {
    if (total_seconds < 0) total_seconds = 0;
    int m = total_seconds / 60;
    int s = total_seconds % 60;
    snprintf(out, out_len, "%s%d:%02d", negative ? "-" : "", m, s);
}

static void
update_art_for_track(const std::string& track_id) {
    if (track_id == s_displayed_track_id) return;
    s_displayed_track_id = track_id;

    const lv_img_dsc_t* art = track_id.empty() ? nullptr : lyrion_fetchArt_HAL(track_id);
    if (art) {
        lv_img_set_src(s_art_img, art);
        lv_obj_clear_flag(s_art_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_art_glyph, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_art_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_art_glyph, LV_OBJ_FLAG_HIDDEN);
    }
}

static void
poll_cb(lv_timer_t* /*t*/) {
    LyrionStatus st;
    bool ok = lyrion_pollStatus_HAL(&st);

    // If we have no player yet (boot-time discovery ran before WiFi was up,
    // or LMS was unreachable), keep retrying discovery so the tab populates
    // on its own once the network catches up.
    if (!ok && st.player_name.empty()) {
        if (lyrion_discoverPlayers_HAL()) {
            ok = lyrion_pollStatus_HAL(&st);
        }
    }

    // player_name is filled even when the HTTP call fails (HAL caches the
    // selected player). Always reflect the current selection so CHUP/CHDOW
    // gives immediate UI feedback. Volume rides along on the same label.
    int vol = (ok && st.valid) ? st.volume : -1;
    if (st.player_name != s_displayed_player || vol != s_displayed_volume) {
        s_displayed_player = st.player_name;
        s_displayed_volume = vol;
        const char* name = st.player_name.empty() ? "" : st.player_name.c_str();
        if (vol >= 0) {
            lv_label_set_text_fmt(s_player_label, "%s - " LV_SYMBOL_VOLUME_MID " %d%%", name, vol);
        } else {
            lv_label_set_text(s_player_label, name);
        }
    }

    // Power + play/pause indicators. When status is invalid, show both as
    // unknown (dim gray + neutral glyph) so the user can tell the screen isn't
    // just stale.
    int powered = (ok && st.valid) ? (st.is_powered ? 1 : 0) : -1;
    int playing = (ok && st.valid) ? (st.is_playing ? 1 : 0) : -1;
    if (powered != s_displayed_powered) {
        s_displayed_powered = powered;
        lv_obj_set_style_text_color(s_power_icon,
            powered == 1 ? lv_color_hex(0x00C000) :
            powered == 0 ? lv_color_hex(0x606060) :
                           lv_color_hex(0x404040),
            LV_PART_MAIN);
    }
    if (playing != s_displayed_playing) {
        s_displayed_playing = playing;
        lv_label_set_text(s_play_icon, playing == 1 ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
        lv_obj_set_style_text_color(s_play_icon,
            playing == 1 ? lv_color_hex(0x00C000) :
            playing == 0 ? lv_color_hex(0xa0a0a0) :
                           lv_color_hex(0x404040),
            LV_PART_MAIN);
    }

    if (!ok || !st.valid) {
        lv_label_set_text(s_title_label,  "");
        lv_label_set_text(s_artist_label, "");
        lv_label_set_text(s_album_label,  "");
        lv_label_set_text(s_time_elapsed, "");
        lv_label_set_text(s_time_remain,  "");
        lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
        update_art_for_track("");
        return;
    }

    lv_label_set_text(s_title_label,  st.title.empty()  ? "" : st.title.c_str());
    lv_label_set_text(s_artist_label, st.artist.c_str());
    lv_label_set_text(s_album_label,  st.album.c_str());
    // Compose an art cache key from coverid + title + artist. coverid alone is
    // stable across tracks within an album (correct: same art) but is *also*
    // often stable across songs in a stream where art does change — so we
    // include title/artist to force a refetch on every track change. The HAL
    // hits /music/current/cover.png which always returns the now-playing art,
    // so refetching is the right behavior; the wasted bandwidth within an
    // album is a few KB and acceptable.
    update_art_for_track(st.track_id + "|" + st.title + "|" + st.artist);

    // Progress bar + time labels. If duration is unknown (stream/radio), show
    // elapsed only and leave the bar empty.
    char buf[16];
    int  elapsed = (int) st.elapsed_s;
    if (st.duration_s > 0.5f) {
        int duration  = (int) st.duration_s;
        int remaining = duration - elapsed;
        if (remaining < 0) remaining = 0;
        lv_bar_set_range(s_progress_bar, 0, duration);
        lv_bar_set_value(s_progress_bar, elapsed, LV_ANIM_OFF);
        format_mmss(elapsed, buf, sizeof(buf), false);
        lv_label_set_text(s_time_elapsed, buf);
        format_mmss(remaining, buf, sizeof(buf), true);
        lv_label_set_text(s_time_remain, buf);
    } else {
        lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
        format_mmss(elapsed, buf, sizeof(buf), false);
        lv_label_set_text(s_time_elapsed, buf);
        lv_label_set_text(s_time_remain, "");
    }
}

void
create_tab_content_lyrion_nowplaying(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 0, LV_PART_MAIN);

    // Top: player name
    s_player_label = lv_label_create(tab);
    lv_label_set_text(s_player_label, "");
    lv_obj_set_style_text_font(s_player_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_player_label, lv_color_hex(0xFFD700), LV_PART_MAIN);
    lv_obj_align(s_player_label, LV_ALIGN_TOP_MID, 0, 6);

    // Power indicator (top-left). Colored by current power state in poll_cb.
    s_power_icon = lv_label_create(tab);
    lv_label_set_text(s_power_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(s_power_icon, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_power_icon, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_align(s_power_icon, LV_ALIGN_TOP_LEFT, 8, 6);

    // Play/pause indicator (top-right). Glyph + color updated in poll_cb.
    s_play_icon = lv_label_create(tab);
    lv_label_set_text(s_play_icon, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_font(s_play_icon, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_play_icon, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_align(s_play_icon, LV_ALIGN_TOP_RIGHT, -8, 6);

    // Album art panel (fallback bg always visible; img on top hides bg when set).
    // Track/artist/album labels are overlaid on the bottom of the art with a
    // semi-transparent dark band so they stay readable against any cover.
    const int art_px = 200;
    s_art_panel = lv_obj_create(tab);
    lv_obj_remove_style_all(s_art_panel);
    lv_obj_set_size(s_art_panel, art_px, art_px);
    lv_obj_align(s_art_panel, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_radius(s_art_panel, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_art_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_art_panel, lv_color_hex(0x303030), LV_PART_MAIN);
    lv_obj_set_style_clip_corner(s_art_panel, true, LV_PART_MAIN);
    lv_obj_clear_flag(s_art_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_art_glyph = lv_label_create(s_art_panel);
    lv_label_set_text(s_art_glyph, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(s_art_glyph, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_art_glyph, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_center(s_art_glyph);

    s_art_img = lv_img_create(s_art_panel);
    lv_obj_align(s_art_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_art_img, LV_OBJ_FLAG_HIDDEN);

    // Dark band at the bottom of the art panel that holds the three text rows.
    // Height covers the three labels with a few pixels of breathing room.
    const int overlay_h = 64;
    lv_obj_t* overlay = lv_obj_create(s_art_panel);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, art_px, overlay_h);
    lv_obj_align(overlay, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    const int label_w = art_px - 8;
    s_title_label = lv_label_create(overlay);
    lv_obj_set_width(s_title_label, label_w);
    lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(s_title_label, "");
    lv_obj_set_style_text_align(s_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 2);

    s_artist_label = lv_label_create(overlay);
    lv_obj_set_width(s_artist_label, label_w);
    lv_label_set_long_mode(s_artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(s_artist_label, "");
    lv_obj_set_style_text_align(s_artist_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_artist_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_artist_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_artist_label, LV_ALIGN_TOP_MID, 0, 24);

    s_album_label = lv_label_create(overlay);
    lv_obj_set_width(s_album_label, label_w);
    lv_label_set_long_mode(s_album_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(s_album_label, "");
    lv_obj_set_style_text_align(s_album_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_album_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_album_label, lv_color_hex(0xc0c0c0), LV_PART_MAIN);
    lv_obj_align(s_album_label, LV_ALIGN_TOP_MID, 0, 42);

    // Progress bar + time labels, bottom-anchored so they stay visible even if
    // the tab content area is shorter than expected.
    s_progress_bar = lv_bar_create(tab);
    lv_obj_set_size(s_progress_bar, 216, 4);
    lv_obj_align(s_progress_bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x303030), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress_bar, color_primary, LV_PART_INDICATOR);
    lv_bar_set_range(s_progress_bar, 0, 100);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);

    s_time_elapsed = lv_label_create(tab);
    lv_label_set_text(s_time_elapsed, "");
    lv_obj_set_style_text_font(s_time_elapsed, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_elapsed, lv_color_hex(0xa0a0a0), LV_PART_MAIN);
    lv_obj_align(s_time_elapsed, LV_ALIGN_BOTTOM_LEFT, 12, -18);

    s_time_remain = lv_label_create(tab);
    lv_label_set_text(s_time_remain, "");
    lv_obj_set_style_text_font(s_time_remain, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_remain, lv_color_hex(0xa0a0a0), LV_PART_MAIN);
    lv_obj_align(s_time_remain, LV_ALIGN_BOTTOM_RIGHT, -12, -18);

    // Discover players + first poll. Discovery is on every tab create so a
    // newly-attached player shows up without rebooting.
    s_displayed_track_id.clear();
    s_displayed_player.clear();
    s_displayed_volume  = -2;
    s_displayed_powered = -1;
    s_displayed_playing = -1;
    lyrion_discoverPlayers_HAL();
    poll_cb(nullptr);

    // 500 ms poll: fast enough that CHUP/CHDOW player switches feel immediate
    // and play/pause state is reflected promptly. LAN traffic is a tiny JSON
    // request every half-second.
    s_poll_timer = lv_timer_create(poll_cb, 500, nullptr);
}

void
notify_tab_before_delete_lyrion_nowplaying(void) {
    if (s_poll_timer) {
        lv_timer_del(s_poll_timer);
        s_poll_timer = nullptr;
    }
    s_player_label = nullptr;
    s_power_icon   = nullptr;
    s_play_icon    = nullptr;
    s_art_panel    = nullptr;
    s_art_img      = nullptr;
    s_art_glyph    = nullptr;
    s_title_label  = nullptr;
    s_artist_label = nullptr;
    s_album_label  = nullptr;
    s_progress_bar = nullptr;
    s_time_elapsed = nullptr;
    s_time_remain  = nullptr;
    s_displayed_track_id.clear();
    s_displayed_player.clear();
    s_displayed_volume  = -2;
    s_displayed_powered = -1;
    s_displayed_playing = -1;
    lyrion_releaseArt_HAL();
}

void
register_gui_lyrion_nowplaying(void) {
    register_gui(std::string(tabName_lyrion_nowplaying),
                 &create_tab_content_lyrion_nowplaying,
                 &notify_tab_before_delete_lyrion_nowplaying);
}

#endif // ENABLE_LYRION
