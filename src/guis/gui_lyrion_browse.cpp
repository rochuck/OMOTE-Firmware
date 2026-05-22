#include "guis/gui_lyrion_browse.h"

#if (ENABLE_LYRION == 1)

#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiRegistry.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/keys.h"
#include "applicationInternal/omote_log.h"
#include "applicationInternal/scenes/sceneRegistry.h"
#include "devices/mediaPlayer/device_lyrion/device_lyrion.h"
#include <lvgl.h>
#include <string>
#include <utility>
#include <vector>

// A d-pad-navigable library browser for the Lyrion scene. The root menu is
// built in code; Playlists, Favorites and the full local library (Artists /
// Albums / Genres / Tracks) plus Search are fetched from LMS on demand. The
// "Apps" entry browses the LMS app/radio plugins (Radio Paradise, etc.) through
// their plugin menus, so streams launch with full metadata + cover art. See
// gui_lyrion_nowplaying.cpp for the sibling now-playing tab; the HAL browse/
// play/search calls live in lyrion_hal_*.cpp.

// Which kind of level the user is looking at. Determines what load_current_level
// fetches and how folders drill in.
enum LevelKind {
    LVL_ROOT,          // built-in root menu
    LVL_APPS,          // installed LMS app/radio plugins
    LVL_APP_ITEMS,     // one app's menu (params: app_cmd, app_item_id)
    LVL_PLAYLISTS,     // saved playlists
    LVL_FAVORITES,     // favorites tree (param: fav_item_id)
    LVL_LIBRARY,       // built-in Music Library submenu
    LVL_ARTISTS,       // params: genre_id?, search?
    LVL_ALBUMS,        // params: artist_id? / genre_id?, search?
    LVL_GENRES,
    LVL_TRACKS,        // params: album_id? / artist_id?, search?
    LVL_SEARCH_GROUPS, // search results overview (param: search)
};

struct NavLevel {
    LevelKind   kind;
    std::string fav_item_id; // favorites drill-in
    std::string app_cmd;     // app plugin query cmd (XMLBrowser), carried down the app tree
    std::string app_item_id; // current node within an app menu (empty = its top menu)
    std::string artist_id;
    std::string album_id;
    std::string genre_id;
    std::string search;      // search term (empty = none)
    std::string title;       // breadcrumb text
};

// Root- and library-menu folder ids (stored in LyrionBrowseItem.id for the
// LIT_FOLDER entries those built-in menus produce).
static const char* ID_SEARCH    = "search";
static const char* ID_APPS      = "apps";
static const char* ID_LIBRARY   = "library";
static const char* ID_PLAYLISTS = "playlists";
static const char* ID_FAVORITES = "favorites";
static const char* ID_ARTISTS   = "artists";
static const char* ID_ALBUMS    = "albums";
static const char* ID_GENRES    = "genres";
static const char* ID_TRACKS    = "tracks";

// Page size for paginated library lists, and a hard cap on how many items we
// keep in RAM for one level (search is the tool for very large libraries).
static const int PAGE          = 200;
static const int MAX_LOADED    = 2000;

static lv_obj_t* s_header = nullptr;
static lv_obj_t* s_list   = nullptr;

static std::vector<LyrionBrowseItem> s_items;
static std::vector<lv_obj_t*>        s_rows;
static std::vector<NavLevel>         s_nav_stack;
static int                           s_focus  = 0;
static int                           s_loaded = 0; // items fetched from server for this level
static int                           s_total  = 0; // total available (for "More...")
static std::string                   s_pending_search; // applied when the tab gets (re)built

// --- key map (owned by this GUI; only active while the Browse tab is shown) --
std::map<char, repeatModes> key_repeatModes_lyrion_browse;
std::map<char, uint16_t>    key_commands_short_lyrion_browse;
std::map<char, uint16_t>    key_commands_long_lyrion_browse;

void
gui_setKeys_lyrion_browse(void) {
    key_repeatModes_lyrion_browse = {
        {KEY_UP, SHORT_REPEATED}, {KEY_DOWN, SHORT_REPEATED},
        {KEY_OK, SHORT},          {KEY_BACK, SHORT},
        {KEY_LEFT, SHORT},        {KEY_RIGHT, SHORT},
    };
    key_commands_short_lyrion_browse = {
        {KEY_UP, LYRION_BROWSE_UP}, {KEY_DOWN, LYRION_BROWSE_DOWN},
        {KEY_OK, LYRION_BROWSE_SELECT}, {KEY_RIGHT, LYRION_BROWSE_SELECT},
        {KEY_BACK, LYRION_BROWSE_BACK}, {KEY_LEFT, LYRION_BROWSE_BACK},
    };
    key_commands_long_lyrion_browse = {};
}

// --- rendering --------------------------------------------------------------

static void
style_row(int idx, bool focused) {
    if (idx < 0 || idx >= (int) s_rows.size()) return;
    lv_obj_t* row = s_rows[idx];
    if (focused) {
        lv_obj_set_style_bg_color(row, color_primary, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    }
    lv_obj_set_style_text_color(row, lv_color_white(), LV_PART_MAIN);
}

static const char*
icon_for(const LyrionBrowseItem& it) {
    switch (it.type) {
        case LIT_TRACK:     return LV_SYMBOL_AUDIO;
        case LIT_FAVORITE:  return LV_SYMBOL_AUDIO;
        case LIT_URL:       return LV_SYMBOL_AUDIO;
        case LIT_APP_AUDIO: return LV_SYMBOL_AUDIO;
        case LIT_PLAYLIST:  return LV_SYMBOL_LIST;
        case LIT_PLAY_ALL:  return LV_SYMBOL_PLAY;
        default:            return LV_SYMBOL_DIRECTORY; // folders / artist / album / genre / app
    }
}

// Rebuild the list widget from s_items and apply the current focus highlight.
static void
render(void) {
    if (!s_list) return;
    lv_obj_clean(s_list);
    s_rows.clear();

    if (!s_nav_stack.empty() && s_header) {
        lv_label_set_text(s_header, s_nav_stack.back().title.c_str());
    }

    if (s_items.empty()) {
        lv_obj_t* row = lv_list_add_text(s_list, "(empty)");
        lv_obj_set_style_text_color(row, lv_color_hex(0x888888), LV_PART_MAIN);
        return;
    }

    if (s_focus < 0) s_focus = 0;
    if (s_focus >= (int) s_items.size()) s_focus = (int) s_items.size() - 1;

    for (size_t i = 0; i < s_items.size(); ++i) {
        lv_obj_t* row = lv_list_add_btn(s_list, icon_for(s_items[i]), s_items[i].title.c_str());
        s_rows.push_back(row);
        style_row((int) i, (int) i == s_focus);
    }
    if (!s_rows.empty()) lv_obj_scroll_to_view(s_rows[s_focus], LV_ANIM_OFF);
}

// --- level loading ----------------------------------------------------------

static void
build_root_items(void) {
    s_items.clear();
    auto add = [](const char* title, const char* id) {
        LyrionBrowseItem e; e.title = title; e.id = id; e.type = LIT_FOLDER;
        s_items.push_back(e);
    };
    add("Search",         ID_SEARCH);
    add("Apps",           ID_APPS);
    add("Music Library",  ID_LIBRARY);
    add("Playlists",      ID_PLAYLISTS);
    add("Favorites",      ID_FAVORITES);
}

static void
build_library_items(void) {
    s_items.clear();
    auto add = [](const char* title, const char* id) {
        LyrionBrowseItem e; e.title = title; e.id = id; e.type = LIT_FOLDER;
        s_items.push_back(e);
    };
    add("Artists", ID_ARTISTS);
    add("Albums",  ID_ALBUMS);
    add("Genres",  ID_GENRES);
}

static void
build_search_group_items(const std::string& term) {
    s_items.clear();
    int nArtists = 0, nAlbums = 0, nTracks = 0;
    lyrion_searchCounts_HAL(term, &nArtists, &nAlbums, &nTracks);
    auto add = [](const std::string& title, const char* id) {
        LyrionBrowseItem e; e.title = title; e.id = id; e.type = LIT_FOLDER;
        s_items.push_back(e);
    };
    if (nArtists > 0) add("Artists (" + std::to_string(nArtists) + ")", ID_ARTISTS);
    if (nAlbums  > 0) add("Albums ("  + std::to_string(nAlbums)  + ")", ID_ALBUMS);
    if (nTracks  > 0) add("Tracks ("  + std::to_string(nTracks)  + ")", ID_TRACKS);
}

// Prepend a synthetic "Play all" row for a drilled-in container.
static void
prepend_play_all(const char* label) {
    LyrionBrowseItem e; e.title = label; e.type = LIT_PLAY_ALL;
    s_items.insert(s_items.begin(), e);
}

// Fetch one page (start = s_loaded) for the current level into a temp vector,
// returning how many it appended. Server lists only.
static int
fetch_page(const NavLevel& lvl, std::vector<LyrionBrowseItem>* page) {
    int total = 0;
    switch (lvl.kind) {
        case LVL_ARTISTS: lyrion_browseArtists_HAL(lvl.genre_id, lvl.search, s_loaded, PAGE, page, &total); break;
        case LVL_ALBUMS:  lyrion_browseAlbums_HAL(lvl.artist_id, lvl.genre_id, lvl.search, s_loaded, PAGE, page, &total); break;
        case LVL_GENRES:  lyrion_browseGenres_HAL(s_loaded, PAGE, page, &total); break;
        case LVL_TRACKS:  lyrion_browseTracks_HAL(lvl.album_id, lvl.artist_id, lvl.search, s_loaded, PAGE, page, &total); break;
        default: break;
    }
    s_total = total;
    return (int) page->size();
}

// Append a "More..." row when the server has more items than we've loaded.
static void
maybe_add_more_row(void) {
    if (s_loaded < s_total && s_loaded < MAX_LOADED) {
        LyrionBrowseItem e;
        e.title = "More...";
        e.id    = "__more__";
        e.type  = LIT_FOLDER;
        s_items.push_back(e);
    }
}

// Fill s_items for whatever level is on top of the nav stack (first page).
static void
load_current_level(void) {
    if (s_nav_stack.empty()) return;
    const NavLevel& lvl = s_nav_stack.back();
    s_items.clear();
    s_loaded = 0;
    s_total  = 0;

    switch (lvl.kind) {
        case LVL_ROOT:          build_root_items(); return;
        case LVL_LIBRARY:       build_library_items(); return;
        case LVL_APPS:          lyrion_browseApps_HAL(&s_items); return;
        case LVL_APP_ITEMS:     lyrion_browseAppItems_HAL(lvl.app_cmd, lvl.app_item_id, &s_items); return;
        case LVL_PLAYLISTS:     lyrion_browsePlaylists_HAL(&s_items); return;
        case LVL_FAVORITES:     lyrion_browseFavorites_HAL(lvl.fav_item_id, &s_items); return;
        case LVL_SEARCH_GROUPS: build_search_group_items(lvl.search); return;
        default: break; // paginated library lists below
    }

    std::vector<LyrionBrowseItem> page;
    fetch_page(lvl, &page);
    s_loaded += (int) page.size();
    // Play-all affordance at the top of a drilled container.
    // Label is plain ASCII; the row icon (LV_SYMBOL_PLAY) shows the play glyph,
    // since the Montserrat subset can't render a Unicode triangle.
    if (lvl.kind == LVL_ALBUMS && !lvl.artist_id.empty()) prepend_play_all("Play all by artist");
    if (lvl.kind == LVL_TRACKS && !lvl.album_id.empty())  prepend_play_all("Play album");
    for (auto& it : page) s_items.push_back(std::move(it));
    maybe_add_more_row();
}

// Load the next page and append it (keeps the current focus position).
static void
load_more(void) {
    if (s_nav_stack.empty()) return;
    const NavLevel& lvl = s_nav_stack.back();
    std::vector<LyrionBrowseItem> page;
    fetch_page(lvl, &page);
    s_loaded += (int) page.size();
    // Drop the existing "More..." row (last entry) before appending.
    if (!s_items.empty() && s_items.back().id == "__more__") s_items.pop_back();
    for (auto& it : page) s_items.push_back(std::move(it));
    maybe_add_more_row();
}

static void
push_level(const NavLevel& lvl) {
    s_nav_stack.push_back(lvl);
    s_focus = 0;
    load_current_level();
    render();
}

// --- selection helpers ------------------------------------------------------

// Build a fresh NavLevel of the given kind for pushing.
static NavLevel
make_level(LevelKind kind, const std::string& title) {
    NavLevel lvl;
    lvl.kind  = kind;
    lvl.title = title;
    return lvl;
}

// Jump to a sibling tab by its (stable) gui_list index. Routed through the
// memory optimizer so it works regardless of the current sliding-window state
// (the physical tabview id of a given gui shifts as you navigate).
static void
go_to_tab(int gui_list_index) {
    guis_doTabCreationForSpecificGUI(gui_memoryOptimizer_getActiveGUIlist(), gui_list_index);
}

static void
play_and_show_nowplaying(bool ok, const char* what) {
    if (ok) {
        go_to_tab(LYRION_GUI_NOWPLAYING);
    } else {
        omote_log_e("lyrion browse: play '%s' failed\r\n", what);
    }
}

// Drill into a folder. Behaviour depends on the current level kind.
static void
handle_folder(const LyrionBrowseItem& item) {
    const LevelKind cur = s_nav_stack.empty() ? LVL_ROOT : s_nav_stack.back().kind;

    if (item.id == "__more__") { load_more(); render(); return; }

    if (cur == LVL_ROOT) {
        if (item.id == ID_SEARCH)         go_to_tab(LYRION_GUI_T9);
        else if (item.id == ID_APPS)      push_level(make_level(LVL_APPS, "Apps"));
        else if (item.id == ID_LIBRARY)   push_level(make_level(LVL_LIBRARY, "Music Library"));
        else if (item.id == ID_PLAYLISTS) push_level(make_level(LVL_PLAYLISTS, "Playlists"));
        else if (item.id == ID_FAVORITES) push_level(make_level(LVL_FAVORITES, "Favorites"));
        return;
    }
    if (cur == LVL_LIBRARY) {
        if (item.id == ID_ARTISTS)     push_level(make_level(LVL_ARTISTS, "Artists"));
        else if (item.id == ID_ALBUMS) push_level(make_level(LVL_ALBUMS, "Albums"));
        else if (item.id == ID_GENRES) push_level(make_level(LVL_GENRES, "Genres"));
        return;
    }
    if (cur == LVL_FAVORITES) {
        NavLevel lvl = make_level(LVL_FAVORITES, item.title);
        lvl.fav_item_id = item.id;
        push_level(lvl);
        return;
    }
    if (cur == LVL_SEARCH_GROUPS) {
        const std::string term = s_nav_stack.back().search;
        NavLevel lvl;
        lvl.search = term;
        if (item.id == ID_ARTISTS)     { lvl.kind = LVL_ARTISTS; lvl.title = "Artists"; }
        else if (item.id == ID_ALBUMS) { lvl.kind = LVL_ALBUMS;  lvl.title = "Albums"; }
        else if (item.id == ID_TRACKS) { lvl.kind = LVL_TRACKS;  lvl.title = "Tracks"; }
        else return;
        push_level(lvl);
        return;
    }
}

// --- public navigation ------------------------------------------------------

void
gui_lyrion_browse_nav(char dir) {
    if (!s_list || s_items.empty()) return;
    int prev = s_focus;
    if (dir == 'u') s_focus--;
    else if (dir == 'd') s_focus++;
    if (s_focus < 0) s_focus = 0;
    if (s_focus >= (int) s_items.size()) s_focus = (int) s_items.size() - 1;
    if (s_focus == prev) return;
    style_row(prev, false);
    style_row(s_focus, true);
    if (s_focus < (int) s_rows.size()) lv_obj_scroll_to_view(s_rows[s_focus], LV_ANIM_ON);
}

void
gui_lyrion_browse_select(void) {
    if (!s_list || s_items.empty()) return;
    if (s_focus < 0 || s_focus >= (int) s_items.size()) return;
    const LyrionBrowseItem item = s_items[s_focus]; // copy: push_level rebuilds s_items
    const NavLevel         cur  = s_nav_stack.empty() ? make_level(LVL_ROOT, "") : s_nav_stack.back();

    switch (item.type) {
        case LIT_FOLDER:   handle_folder(item); break;
        case LIT_ARTIST: { NavLevel l = make_level(LVL_ALBUMS, item.title); l.artist_id = item.id; push_level(l); } break;
        case LIT_GENRE:  { NavLevel l = make_level(LVL_ARTISTS, item.title); l.genre_id = item.id; push_level(l); } break;
        case LIT_ALBUM:  { NavLevel l = make_level(LVL_TRACKS, item.title); l.album_id = item.id; push_level(l); } break;
        case LIT_TRACK:    play_and_show_nowplaying(lyrion_playSelector_HAL("track_id:" + item.id), item.title.c_str()); break;
        case LIT_PLAYLIST: play_and_show_nowplaying(lyrion_playPlaylist_HAL(item.id), item.title.c_str()); break;
        case LIT_FAVORITE: play_and_show_nowplaying(lyrion_playFavorite_HAL(item.id), item.title.c_str()); break;
        case LIT_URL:      play_and_show_nowplaying(lyrion_playUrl_HAL(item.url, item.title), item.title.c_str()); break;
        case LIT_APP: { // app root: drill into its top menu (id holds the plugin query cmd)
            NavLevel l = make_level(LVL_APP_ITEMS, item.title); l.app_cmd = item.id; push_level(l);
        } break;
        case LIT_APP_FOLDER: { // menu node inside an app: drill, carrying the app's query cmd
            NavLevel l = make_level(LVL_APP_ITEMS, item.title); l.app_cmd = cur.app_cmd; l.app_item_id = item.id; push_level(l);
        } break;
        case LIT_APP_AUDIO: play_and_show_nowplaying(lyrion_playAppItem_HAL(cur.app_cmd, item.id), item.title.c_str()); break;
        case LIT_PLAY_ALL: {
            std::string selector;
            if (!cur.album_id.empty())       selector = "album_id:" + cur.album_id;
            else if (!cur.artist_id.empty()) selector = "artist_id:" + cur.artist_id;
            if (!selector.empty()) play_and_show_nowplaying(lyrion_playSelector_HAL(selector), selector.c_str());
        } break;
    }
}

void
gui_lyrion_browse_back(void) {
    if (!s_list) return;
    if (s_nav_stack.size() > 1) {
        s_nav_stack.pop_back();
        s_focus = 0;
        load_current_level();
        render();
    } else {
        // Already at root -> leave Browse for the now-playing tab.
        go_to_tab(LYRION_GUI_NOWPLAYING);
    }
}

// Reset the nav stack to root -> search groups for `query` and render.
static void
apply_search(const std::string& query) {
    s_nav_stack.clear();
    s_nav_stack.push_back(make_level(LVL_ROOT, "Lyrion"));
    NavLevel groups = make_level(LVL_SEARCH_GROUPS, "Search: " + query);
    groups.search = query;
    push_level(groups);
}

void
gui_lyrion_browse_runSearch(const std::string& query) {
    // Stash the query, then bring the Browse tab to the front. If the tab was
    // resident (the usual case), no rebuild happens and we apply the search
    // here; if the navigation rebuilt it, create_tab_content consumed the
    // pending query already (and cleared it).
    s_pending_search = query;
    go_to_tab(LYRION_GUI_BROWSE);
    if (s_list && !s_pending_search.empty()) {
        std::string q = s_pending_search;
        s_pending_search.clear();
        apply_search(q);
    }
}

// --- tab lifecycle ----------------------------------------------------------

void
create_tab_content_lyrion_browse(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);

    s_header = lv_label_create(tab);
    lv_obj_set_style_text_color(s_header, color_primary, LV_PART_MAIN);
    lv_obj_set_style_pad_left(s_header, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(s_header, 4, LV_PART_MAIN);
    lv_label_set_text(s_header, "Lyrion");

    s_list = lv_list_create(tab);
    lv_obj_set_width(s_list, lv_pct(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_style_bg_color(s_list, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);

    // Discovery may already have run on the now-playing tab; calling again is
    // harmless and ensures a player is selected before any play command.
    lyrion_discoverPlayers_HAL();

    s_nav_stack.clear();
    s_nav_stack.push_back(make_level(LVL_ROOT, "Lyrion"));
    s_focus = 0;
    load_current_level();

    // If a search was requested while this tab was not built, enter it now.
    if (!s_pending_search.empty()) {
        NavLevel groups = make_level(LVL_SEARCH_GROUPS, "Search: " + s_pending_search);
        groups.search = s_pending_search;
        s_pending_search.clear();
        s_nav_stack.push_back(groups);
        load_current_level();
    }
    render();
}

void
notify_tab_before_delete_lyrion_browse(void) {
    s_header = nullptr;
    s_list   = nullptr;
    s_rows.clear();
    s_items.clear();
    s_nav_stack.clear();
    s_focus  = 0;
    s_loaded = 0;
    s_total  = 0;
    // Keep s_pending_search: a search may have been queued just before a rebuild.
}

void
register_gui_lyrion_browse(void) {
    register_gui(std::string(tabName_lyrion_browse),
                 &create_tab_content_lyrion_browse,
                 &notify_tab_before_delete_lyrion_browse,
                 &gui_setKeys_lyrion_browse,
                 &key_repeatModes_lyrion_browse,
                 &key_commands_short_lyrion_browse,
                 &key_commands_long_lyrion_browse);
}

#endif // ENABLE_LYRION
