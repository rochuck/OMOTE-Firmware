#pragma once

#if (ENABLE_LYRION == 1)

#include <lvgl.h>
#include <string>

const char* const tabName_lyrion_browse = "Browse";

// gui_list indices in scene_lyrion's gui_list (kept in sync with
// scene_lyrion.cpp). These are stable list positions, NOT physical tabview
// ids — cross-tab jumps use guis_doTabCreationForSpecificGUI(), which maps the
// list index through the memory optimizer's sliding 3-tab window.
enum {
    LYRION_GUI_NOWPLAYING = 0,
    LYRION_GUI_BROWSE     = 1,
    LYRION_GUI_T9         = 2,
};

void register_gui_lyrion_browse(void);

// d-pad navigation entry points, called from the LYRION command handler when
// the Browse tab owns the keypad. All are no-ops if the tab is not built.
//   dir: 'u' = up, 'd' = down (move the highlighted row)
void gui_lyrion_browse_nav(char dir);
void gui_lyrion_browse_select(void); // enter a folder / play the focused item
void gui_lyrion_browse_back(void);   // go up a level (exits to now-playing at root)

// Run a library search for `query` and show grouped results (Artists/Albums/
// Tracks) on the Browse tab. Called by the T9 keypad's OK handler when the
// Lyrion scene is active; switches the active tab to Browse.
void gui_lyrion_browse_runSearch(const std::string& query);

#endif // ENABLE_LYRION
