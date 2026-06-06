#include "blasterStateSync.h"

#if (ENABLE_WIFI_AND_MQTT == 1) && defined(ARDUINO)

#include "blasterClient.h"
#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/gui/guiMemoryOptimizer.h"
#include "applicationInternal/hardware/hardwarePresenter.h"
#include "applicationInternal/scenes/sceneHandler.h"
#include "applicationInternal/scenes/sceneRegistry.h"
#include "applicationInternal/omote_log.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cctype>
#include <cstdlib>
#include <string>

namespace {

// Coalesce a burst of setter calls (scene change typically triggers
// guiList + guiName + lastIndex updates in quick succession) into one POST.
constexpr unsigned long kPostCoalesceMs = 250;
// Periodic re-fetch cadence once we're past the boot sync. 2 s is snappy
// enough that a button press on another remote shows up here within a couple
// seconds, while keeping HTTP chatter on the LAN modest.
constexpr unsigned long kPollIntervalMs = 2000;

unsigned long s_next_poll_ms = 0;

// Boot-sync one-shot: true after the first GET /state attempt has been
// kicked off; prevents re-syncing on every WiFi blip thereafter.
bool s_first_sync_started = false;
// True after the first GET has been processed (applied, skipped, or failed).
// Until then we must NOT POST — otherwise the dirty buffer accumulated during
// boot would race the GET and our value could overwrite the blaster's
// authoritative state before we even read it.
bool s_first_sync_done = false;

// Set true when the user changes state locally AFTER the boot-sync GET has
// been spawned. If true when the fetch completes, the apply is skipped —
// the user's choice wins and has already been POSTed.
bool s_local_changed_during_fetch = false;

// Re-entrancy guard: while apply_remote_state() runs it calls setters, which
// would normally re-POST. Suppress that.
bool s_applying_remote = false;

// Dirty buffer for the POST side.
bool          s_post_pending = false;
unsigned long s_post_due_ms  = 0;
// True when the pending POST is an automatic reconcile (boot-time echo of
// NVS-restored state, cold-blaster push, or mid-session reconnect) rather than
// user-driven navigation. Reconcile pushes carry "reconcile":true so the
// blaster updates its state WITHOUT treating it as user activity — otherwise
// every IMU wake (which reboots the remote and re-syncs) would reset the
// blaster's inactivity auto-off timer.
bool          s_post_is_reconcile = false;

// Pending apply from a completed GET. Written on the fetch task, consumed on
// the main thread in blasterStateSync_loop(). Single producer / single
// consumer with a volatile flag — no mutex needed.
volatile bool s_apply_pending = false;
bool          s_apply_valid   = false;
std::string   s_apply_scene;
std::string   s_apply_guiName;
int           s_apply_guiList = 0;
int           s_apply_lastIdx = 0;

volatile bool s_fetch_in_flight = false;

// -- tiny JSON helpers ----------------------------------------------------
// The blaster's responses are small and known-shape, so we lift just two
// values without pulling in a parser. Format: lower-case keys, no extra
// whitespace expected, but we tolerate it around the colon.

bool find_value_start(const String& body, const char* key, int& out_pos) {
    String pat = String("\"") + key + "\"";
    int p = body.indexOf(pat);
    if (p < 0) return false;
    int c = body.indexOf(':', p);
    if (c < 0) return false;
    int i = c + 1;
    while (i < (int)body.length() && isspace((unsigned char)body[i])) i++;
    out_pos = i;
    return true;
}

bool extract_bool(const String& body, const char* key, bool& out) {
    int i;
    if (!find_value_start(body, key, i)) return false;
    if (body.substring(i, i + 4) == "true")  { out = true;  return true; }
    if (body.substring(i, i + 5) == "false") { out = false; return true; }
    return false;
}

bool extract_string(const String& body, const char* key, std::string& out) {
    int i;
    if (!find_value_start(body, key, i)) return false;
    if (i >= (int)body.length() || body[i] != '"') return false;
    int end = body.indexOf('"', i + 1);
    if (end < 0) return false;
    // No unescape — these fields are user-facing labels with no embedded
    // quotes or backslashes in practice (sender uses json_escape).
    out = body.substring(i + 1, end).c_str();
    return true;
}

bool extract_int(const String& body, const char* key, int& out) {
    int i;
    if (!find_value_start(body, key, i)) return false;
    int start = i;
    if (i < (int)body.length() && body[i] == '-') i++;
    while (i < (int)body.length() && isdigit((unsigned char)body[i])) i++;
    if (i == start) return false;
    out = (int)strtol(body.substring(start, i).c_str(), nullptr, 10);
    return true;
}

// -- JSON build for POST --------------------------------------------------

void escape_into(const std::string& s, String& out) {
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if ((unsigned char)c >= 0x20) out += c;
        }
    }
}

String build_state_body(bool reconcile) {
    String body;
    body.reserve(128);
    body  = "{\"scene\":\"";
    escape_into(get_activeScene(), body);
    body += "\",\"guiName\":\"";
    escape_into(get_activeGUIname(), body);
    body += "\",\"guiList\":";
    body += String(get_activeGUIlist());
    body += ",\"lastIndex\":";
    body += String(get_lastActiveGUIlistIndex());
    // Only emitted for automatic pushes; absent (false) for user navigation so
    // older blasters stay backward-compatible (missing key == user activity).
    if (reconcile) body += ",\"reconcile\":true";
    body += "}";
    return body;
}

// -- fetch task -----------------------------------------------------------
// One-shot. Spawned by onBlasterAvailable() the first time the blaster
// becomes reachable after boot. Runs the synchronous GET off the main task
// (the HTTPClient call can take ~1 s including connect), parses, and stashes
// the result for the main thread to apply.

void fetch_task(void*) {
    String body;
    int code = blaster_getJson("/state", body);
    if (code >= 200 && code < 300) {
        bool valid = false;
        bool ok = extract_bool(body, "valid", valid);
        if (ok && valid) {
            std::string scene, guiName;
            int guiList = 0, lastIdx = 0;
            if (extract_string(body, "scene",     scene)   &&
                extract_string(body, "guiName",   guiName) &&
                extract_int   (body, "guiList",   guiList) &&
                extract_int   (body, "lastIndex", lastIdx)) {
                s_apply_scene   = scene;
                s_apply_guiName = guiName;
                s_apply_guiList = guiList;
                s_apply_lastIdx = lastIdx;
                s_apply_valid   = true;
                omote_log_d("[blasterSync] fetched: scene=\"%s\" gui=\"%s\" list=%d idx=%d\r\n",
                            scene.c_str(), guiName.c_str(), guiList, lastIdx);
            } else {
                omote_log_w("[blasterSync] /state response missing fields: %s\r\n", body.c_str());
                s_apply_valid = false;
            }
        } else {
            // Blaster booted with no state yet — we'll POST ours instead.
            omote_log_i("[blasterSync] blaster cold (valid=false); will push local state\r\n");
            s_apply_valid = false;
        }
        s_apply_pending = true;
    } else {
        omote_log_w("[blasterSync] GET /state failed (%d); will push local state\r\n", code);
        // Treat as "blaster cold" — main loop will POST our NVS state so the
        // blaster (if it comes back) has authoritative state from us. This
        // also closes out the boot-sync gate so future POSTs can flow.
        s_apply_valid   = false;
        s_apply_pending = true;
    }
    s_fetch_in_flight = false;
    vTaskDelete(nullptr);
}

// -- apply on main thread -------------------------------------------------

// Apply the most recent fetch result. Returns true if anything actually
// changed on this remote (so the caller can choose whether to log or drop
// any pending POST). A no-op apply (state already matches) returns false —
// the common case for the periodic poll.
bool apply_remote_state() {
    GUIlists targetList = (GUIlists)s_apply_guiList;
    s_applying_remote = true;

    // Set the scene name FIRST so that get_gui_list_withFallback(SCENE_GUI_LIST)
    // resolves to the TARGET scene's tab list rather than whichever scene this
    // remote happened to be on. Without this, a remote sitting at scene
    // selection (or another scene) would look up "Lyrion-NowPlaying" in the
    // wrong list, fail, and silently bail.
    bool scene_changed = (s_apply_scene != get_activeScene());
    if (scene_changed) {
        gui_memoryOptimizer_setActiveSceneName(s_apply_scene);
        setLabelActiveScene();
    }

    // Now look up the GUI in the (target scene's) list. If the remote sent a
    // GUI name this firmware doesn't know (mid-upgrade mismatch), bail —
    // we've already adopted the scene name, which is harmless on its own.
    gui_list lst = get_gui_list_withFallback(targetList);
    int idx = -1;
    for (size_t i = 0; i < lst->size(); i++) {
        if (lst->at(i) == s_apply_guiName) { idx = (int)i; break; }
    }
    if (idx < 0) {
        omote_log_w("[blasterSync] apply: GUI \"%s\" not in list %d on this remote; "
                    "keeping local state\r\n", s_apply_guiName.c_str(), s_apply_guiList);
        s_applying_remote = false;
        return scene_changed;
    }

    // Note: we deliberately do NOT run the scene's IR start sequence — the A/V
    // hardware is already in this state (that's why the blaster reported it).
    // We're only catching the GUI up.

    // Navigate to the GUI tab. navigateToGUI auto-rewrites lastActiveIndex
    // (from gui_state.activeTabID) any time it crosses gui_lists — so do this
    // BEFORE restoring lastIndex below.
    bool nav_changed = (scene_changed ||
                        s_apply_guiName != get_activeGUIname() ||
                        s_apply_guiList != get_activeGUIlist());
    if (nav_changed) {
        guis_doTabCreationForSpecificGUI(targetList, idx);
    }

    // Restore the cross-list "back to" index last, in case the navigate above
    // clobbered it.
    bool idx_changed = (s_apply_lastIdx != get_lastActiveGUIlistIndex());
    if (idx_changed) {
        gui_memoryOptimizer_setLastActiveGUIlistIndex(s_apply_lastIdx);
    }

    s_applying_remote = false;

    bool any = scene_changed || nav_changed || idx_changed;
    if (any) {
        omote_log_i("[blasterSync] applied remote state: scene=\"%s\" gui=\"%s\" list=%d idx=%d\r\n",
                    s_apply_scene.c_str(), s_apply_guiName.c_str(),
                    s_apply_guiList, s_apply_lastIdx);
    }
    return any;
}

void push_state_now(bool reconcile) {
    if (!blaster_isAvailable()) return;  // try again next time it's up
    String body = build_state_body(reconcile);
    int code = blaster_postJson("/state", body.c_str());
    if (code >= 200 && code < 300) {
        omote_log_d("[blasterSync] POST /state -> %d ok\r\n", code);
    } else {
        omote_log_w("[blasterSync] POST /state -> %d\r\n", code);
        // Don't retry — blasterClient will mark itself unavailable on the
        // next failed /send anyway; we'll re-POST when it recovers.
    }
}

// Spawn the fetch_task with the right bookkeeping. Returns true on success.
// Clears s_local_changed_during_fetch so the consumer can use it as the
// "user touched after fetch started" gate.
bool start_fetch_async() {
    if (s_fetch_in_flight) return false;
    s_fetch_in_flight = true;
    s_local_changed_during_fetch = false;
    BaseType_t ok = xTaskCreate(fetch_task, "blaster_sync", 4096,
                                nullptr, 1, nullptr);
    if (ok != pdPASS) {
        s_fetch_in_flight = false;
        omote_log_w("[blasterSync] could not spawn fetch task\r\n");
        return false;
    }
    return true;
}

}  // namespace

void blasterStateSync_postCurrent() {
    if (s_applying_remote) return;     // re-entry from apply_remote_state()
    if (s_first_sync_started) {
        // Once the boot-fetch is in flight, any subsequent user change must
        // veto the reconcile — their action wins.
        s_local_changed_during_fetch = true;
    }
    // Classify the push. Setter calls before the boot reconcile completes are
    // boot bring-up (GUI tab creation replaying NVS state), not user
    // navigation, so their echo must not reset the blaster's timer. A genuine
    // user change (after boot-sync) upgrades any pending push to user-driven.
    if (s_first_sync_done) {
        s_post_is_reconcile = false;
    } else if (!s_post_pending) {
        s_post_is_reconcile = true;
    }
    s_post_pending = true;
    s_post_due_ms  = millis() + kPostCoalesceMs;
}

void blasterStateSync_onBlasterAvailable() {
    // First connect after boot: kick off a GET /state and reconcile. Marking
    // s_first_sync_started here also arms the "user-wins" guard inside
    // postCurrent(), so any setter call between now and the apply will block
    // the apply.
    if (!s_first_sync_started) {
        s_first_sync_started = true;
        if (!start_fetch_async()) {
            // Without a fetch, open the gate and let the next push establish
            // blaster state from ours. Automatic, so don't reset its timer.
            s_first_sync_done   = true;
            s_post_pending      = true;
            s_post_is_reconcile = true;
            s_post_due_ms       = millis();
        }
        return;
    }

    // Reconnect mid-session (WiFi blip, blaster reboot). Push our current
    // state so the blaster catches up — never overwrite the user's screen.
    // Automatic catch-up, not user activity, so don't reset the auto-off timer.
    s_post_pending      = true;
    s_post_is_reconcile = true;
    s_post_due_ms       = millis();
}

void blasterStateSync_loop() {
    if (s_apply_pending) {
        // Consume the flag first so a parallel fetch result (shouldn't
        // happen with the in_flight guard, but be defensive) doesn't get
        // dropped silently.
        s_apply_pending = false;
        bool is_boot_sync = !s_first_sync_done;
        if (s_local_changed_during_fetch) {
            // User touched the device while the fetch was in flight; their
            // change wins and has already been queued for POST below.
            omote_log_d("[blasterSync] local change during fetch — skip apply\r\n");
        } else if (s_apply_valid) {
            // apply_remote_state is a no-op (returns false, no log) when the
            // blaster's snapshot already matches local — the common case for
            // the periodic poll.
            if (apply_remote_state()) {
                // Our state was just rewritten from the blaster; suppress
                // any POST that would just echo it back.
                s_post_pending = false;
            }
        } else if (is_boot_sync) {
            // Blaster was cold on boot. Push our local NVS-restored state up
            // so it becomes the source of truth from now on. Automatic, so
            // flag it as reconcile — don't reset the blaster's auto-off timer.
            s_post_pending      = true;
            s_post_is_reconcile = true;
            s_post_due_ms       = millis();
        }
        // Boot reconcile is over — POSTs may resume.
        s_first_sync_done = true;
        s_next_poll_ms    = millis() + kPollIntervalMs;
    }

    if (s_post_pending && (long)(millis() - s_post_due_ms) >= 0) {
        // Suppress POSTs until boot reconcile finishes; otherwise the dirty
        // values from before the GET could race the GET and stomp on the
        // blaster's authoritative state.
        if (s_first_sync_started && !s_first_sync_done) return;
        if (blaster_isAvailable()) {
            push_state_now(s_post_is_reconcile);
            s_post_pending      = false;
            s_post_is_reconcile = false;  // next push is user-driven unless re-flagged
            // Reset the poll clock so we don't immediately re-fetch what we
            // just pushed; the blaster needs a beat to settle and the next
            // poll would just echo our own state back.
            s_next_poll_ms = millis() + kPollIntervalMs;
        }
        // If !available, leave s_post_pending set; we'll retry as soon as
        // blasterClient recovers (its loop re-polls every 60 s).
    }

    // Periodic poll once boot sync is done. Skipped while a POST is pending
    // (don't fetch stale state mid-write) or a fetch is already in flight.
    if (s_first_sync_done
        && !s_post_pending
        && !s_fetch_in_flight
        && blaster_isAvailable()
        && (long)(millis() - s_next_poll_ms) >= 0) {
        s_next_poll_ms = millis() + kPollIntervalMs;
        start_fetch_async();
    }
}

#endif  // ENABLE_WIFI_AND_MQTT && ARDUINO
