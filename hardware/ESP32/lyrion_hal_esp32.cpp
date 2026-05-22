/*
 * Lyrion Music Server (LMS) HAL for ESP32-S3
 *
 * Talks to an LMS server over HTTP using its JSON-RPC endpoint
 * (`/jsonrpc.js`). Supports player discovery, command send, status poll,
 * and album-art fetch. State (cached player list, currently-selected player,
 * cached art buffer) lives here so the GUI and command handler stay simple.
 *
 * Configure host/port in secrets_override.h (LYRION_HOST / LYRION_PORT /
 * optional LYRION_PLAYER_NAME).
 *
 * Reference: https://lyrion.org/reference/cli/
 */

#include "applicationInternal/hardware/hardwarePresenter.h"

#if (ENABLE_LYRION == 1)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp32-hal-psram.h>
#include <vector>

#include "applicationInternal/omote_log.h"
#include "secrets.h"

// Album art display size (square). Matches the LCD layout: 240x320 with the
// player name on top and text overlaid on the bottom of the art leaves room
// for a 200px square.
static const int LYRION_ART_PX = 200;

// Soft cap on PNG download size. LMS-resized covers at 200x200 land in the
// 20-60 KB range; 160 KB is a generous ceiling that still leaves plenty of
// PSRAM.
static const size_t LYRION_ART_MAX_BYTES = 160 * 1024;

// Read timeout for app/radio plugin browses. These make LMS fetch menus from
// the internet before replying, so they need far longer than a local query.
static const int LYRION_APP_BROWSE_TIMEOUT_MS = 10000;

struct PlayerEntry {
    std::string id;   // MAC address, e.g. "aa:bb:cc:dd:ee:ff"
    std::string name; // human-readable
};

static bool                     s_inited        = false;
static unsigned                 s_request_id    = 1; // unsigned: wraps cleanly, no signed-overflow UB
static std::vector<PlayerEntry> s_players;
static int                      s_current_index = -1;

static uint8_t*      s_art_buf       = nullptr;
static size_t        s_art_len       = 0;
static std::string   s_art_track_id; // track id whose art is currently in the buffer
static lv_img_dsc_t  s_art_dsc;      // points into s_art_buf

static const char* LYRION_NVS_NAMESPACE = "lyrion";
static const char* LYRION_NVS_KEY_PLAYER = "player";

static std::string
load_saved_player_name(void) {
    Preferences p;
    if (!p.begin(LYRION_NVS_NAMESPACE, true)) return std::string();
    String s = p.getString(LYRION_NVS_KEY_PLAYER, "");
    p.end();
    return std::string(s.c_str());
}

static void
save_player_name(const std::string& name) {
    Preferences p;
    if (!p.begin(LYRION_NVS_NAMESPACE, false)) return;
    p.putString(LYRION_NVS_KEY_PLAYER, name.c_str());
    p.end();
}

static String
build_url(const char* path) {
    return String("http://") + LYRION_HOST + ":" + String((int) LYRION_PORT) + path;
}

// Build a slim.request JSON-RPC body. `player_id_json` is either "\"\"" (empty
// quoted string for server-level commands like player discovery) or a quoted
// MAC string like "\"aa:bb:cc:dd:ee:ff\"". `command_array_json` is the inner
// command array verbatim, e.g. "[\"pause\"]".
static String
build_rpc_body(const String& player_id_json, const String& command_array_json) {
    String body = String("{\"id\":") + String((unsigned long) s_request_id++) +
                  ",\"method\":\"slim.request\",\"params\":[" +
                  player_id_json + "," + command_array_json + "]}";
    return body;
}

// Map a single Unicode code point to a printable ASCII equivalent. The
// Montserrat subset built in only covers ASCII, so anything else would render
// as a missing-glyph box. Known punctuation and accented Latin letters are
// transliterated; everything else falls back to '?' so a box never appears.
static void
append_ascii(std::string& out, uint32_t cp) {
    if (cp < 0x80) { out += (char) cp; return; }
    switch (cp) {
        // Quotes / primes / accents that read as an apostrophe.
        case 0x2018: case 0x2019: case 0x201A: case 0x201B:
        case 0x02BC: case 0x2032: case 0x00B4: case 0x0060:
            out += '\''; return;
        case 0x201C: case 0x201D: case 0x201E: case 0x201F:
        case 0x2033:
            out += '"'; return;
        // Hyphens / dashes / minus.
        case 0x2010: case 0x2011: case 0x2012: case 0x2013:
        case 0x2014: case 0x2015: case 0x2212:
            out += '-'; return;
        case 0x2026: out += "..."; return;            // ellipsis
        case 0x00A0: case 0x2007: case 0x2009: case 0x202F:
            out += ' '; return;                       // assorted spaces
    }
    // Latin-1 Supplement accented letters -> base ASCII letter.
    static const char* const latin1 =  // U+00C0 .. U+00FF
        "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTs"   // C0-DF (xD7 mult sign, DE Th->T, DF ss->s)
        "aaaaaaaceeeeiiiidnooooo/ouuuuyty";  // E0-FF (xF7 div sign)
    if (cp >= 0x00C0 && cp <= 0x00FF) { out += latin1[cp - 0x00C0]; return; }
    out += '?';                                       // unknown -> printable
}

// Transliterate a UTF-8 string to printable ASCII (see append_ascii). Decodes
// each code point, then maps it; malformed bytes are dropped.
static std::string
sanitize_text(const char* s) {
    std::string out;
    if (!s) return out;
    out.reserve(strlen(s));
    for (const unsigned char* p = (const unsigned char*) s; *p; ) {
        unsigned char c = *p;
        uint32_t cp; int len;
        if      (c < 0x80)          { cp = c;        len = 1; }
        else if ((c & 0xE0) == 0xC0){ cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0){ cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0){ cp = c & 0x07; len = 4; }
        else                        { p += 1; continue; }   // stray continuation
        bool ok = true;
        for (int i = 1; i < len; i++) {
            if ((p[i] & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (p[i] & 0x3F);
        }
        if (!ok) { p += 1; continue; }
        append_ascii(out, cp);
        p += len;
    }
    return out;
}

static String
quote_json_string(const std::string& s) {
    // No escaping needed for player MACs / known callers, but keep it tidy.
    String out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    out += "\"";
    return out;
}

// POST a JSON-RPC body and return the response body (empty on failure).
// `timeout_ms` is the read timeout: keep it short (2 s) for local queries so a
// down server can't freeze the UI, but allow more for app/plugin browses, which
// make LMS fetch menus from the internet (Radio Paradise et al.) before it
// replies — a 2 s read timeout drops those mid-response (HTTP error -5).
static String
post_rpc(const String& body, int timeout_ms = 2000) {
    if (WiFi.status() != WL_CONNECTED) {
        omote_log_e("lyrion: WiFi not connected, dropping request\r\n");
        return String();
    }
    HTTPClient http;
    http.setConnectTimeout(2000);
    http.setTimeout(timeout_ms);
    http.begin(build_url("/jsonrpc.js"));
    http.addHeader("Content-Type", "application/json");
    omote_log_v("lyrion: POST body=%s\r\n", body.c_str());
    int    code = http.POST((uint8_t*) body.c_str(), body.length());
    String resp;
    if (code >= 200 && code < 300) {
        resp = http.getString();
        omote_log_v("lyrion: HTTP %d, %u bytes\r\n", code, (unsigned) resp.length());
    } else {
        omote_log_e("lyrion: HTTP %d\r\n", code);
    }
    http.end();
    return resp;
}

void
init_lyrion_HAL(void) {
    s_inited = true;
    memset(&s_art_dsc, 0, sizeof(s_art_dsc));
    omote_log_i("lyrion: configured for http://%s:%d/jsonrpc.js\r\n", LYRION_HOST, (int) LYRION_PORT);
}

bool
lyrion_discoverPlayers_HAL(void) {
    if (!s_inited) return false;
    String body = build_rpc_body(F("\"\""), F("[\"players\",\"0\",\"99\"]"));
    String resp = post_rpc(body);
    if (resp.isEmpty()) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        omote_log_e("lyrion: discover JSON parse failed: %s\r\n", err.c_str());
        return false;
    }
    JsonArrayConst loop = doc["result"]["players_loop"].as<JsonArrayConst>();
    if (loop.isNull()) {
        omote_log_e("lyrion: discover response missing players_loop\r\n");
        return false;
    }
    s_players.clear();
    for (JsonVariantConst p : loop) {
        PlayerEntry e;
        e.id   = p["playerid"].as<const char*>() ? p["playerid"].as<const char*>() : "";
        e.name = p["name"].as<const char*>() ? p["name"].as<const char*>() : "";
        if (!e.id.empty()) s_players.push_back(e);
    }
    if (s_players.empty()) {
        omote_log_w("lyrion: no players discovered\r\n");
        s_current_index = -1;
        return false;
    }

    omote_log_i("lyrion: discovered %u players\r\n", (unsigned) s_players.size());
    for (size_t i = 0; i < s_players.size(); ++i) {
        omote_log_i("lyrion:   [%u] %s  (%s)\r\n", (unsigned) i, s_players[i].name.c_str(), s_players[i].id.c_str());
    }

    // Pick the player to use. Priority:
    //   1. Last-used player saved in NVS (so manual CHUP/CHDOW selections stick
    //      across restarts).
    //   2. LYRION_PLAYER_NAME from secrets (build-time default).
    //   3. First discovered player.
    s_current_index = 0;
    bool matched = false;
    std::string saved = load_saved_player_name();
    if (!saved.empty()) {
        for (size_t i = 0; i < s_players.size(); ++i) {
            if (s_players[i].name == saved) {
                s_current_index = (int) i;
                matched = true;
                break;
            }
        }
    }
    if (!matched) {
        const char* preferred = LYRION_PLAYER_NAME;
        if (preferred && preferred[0] != '\0') {
            for (size_t i = 0; i < s_players.size(); ++i) {
                if (s_players[i].name == preferred) {
                    s_current_index = (int) i;
                    break;
                }
            }
        }
    }
    omote_log_i("lyrion: selected player [%d] %s\r\n", s_current_index, s_players[s_current_index].name.c_str());
    return true;
}

bool
lyrion_cyclePlayer_HAL(int direction) {
    if (s_players.size() < 2) return false;
    int n = (int) s_players.size();
    s_current_index = ((s_current_index + direction) % n + n) % n;
    omote_log_i("lyrion: cycled to player [%d] %s\r\n", s_current_index, s_players[s_current_index].name.c_str());
    save_player_name(s_players[s_current_index].name);
    // Invalidate cached art so the GUI refetches for the new player's current track
    lyrion_releaseArt_HAL();
    return true;
}

bool
lyrion_sendCommand_HAL(const std::string& command_array_json) {
    if (!s_inited) return false;
    if (s_current_index < 0 || s_current_index >= (int) s_players.size()) {
        omote_log_e("lyrion: no player selected, dropping command\r\n");
        return false;
    }
    String pid  = quote_json_string(s_players[s_current_index].id);
    String body = build_rpc_body(pid, String(command_array_json.c_str()));
    String resp = post_rpc(body);
    return !resp.isEmpty();
}

bool
lyrion_powerToggle_HAL(void) {
    return lyrion_sendCommand_HAL("[\"power\"]");
}

bool
lyrion_powerOffAll_HAL(void) {
    if (!s_inited || s_players.empty()) return false;
    bool ok = true;
    for (const auto& p : s_players) {
        String pid  = quote_json_string(p.id);
        String body = build_rpc_body(pid, F("[\"power\",\"0\"]"));
        if (post_rpc(body).isEmpty()) ok = false;
    }
    return ok;
}

bool
lyrion_pollStatus_HAL(LyrionStatus* out) {
    if (!out) return false;
    out->valid = false;
    if (!s_inited || s_current_index < 0 || s_current_index >= (int) s_players.size()) {
        return false;
    }

    out->player_name = s_players[s_current_index].name;

    // tags: a=artist, A=albumartist, l=album, c=coverid (canonical art id, works
    // for remote tracks too — plain track "id" is often negative for streams and
    // doesn't resolve under /music/<id>/cover.png).
    String pid  = quote_json_string(s_players[s_current_index].id);
    String body = build_rpc_body(pid, F("[\"status\",\"-\",\"1\",\"tags:aAlc\"]"));
    String resp = post_rpc(body);
    if (resp.isEmpty()) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        omote_log_e("lyrion: status JSON parse failed: %s\r\n", err.c_str());
        return false;
    }
    JsonVariantConst result = doc["result"];
    const char* mode        = result["mode"].as<const char*>();
    out->is_playing         = (mode && strcmp(mode, "play") == 0);
    out->is_powered         = (result["power"].as<int>() != 0);

    // elapsed: at top level of result (called "time")
    // duration: may live at top level OR inside playlist_loop[0]. For streams
    // with track metadata (Radio Paradise et al.) it's usually top-level under
    // "duration"; for local tracks it's both in playlist_loop[0].duration and
    // sometimes top-level. Try playlist_loop first, then fall back.
    out->elapsed_s  = result["time"].as<float>();
    out->duration_s = result["duration"].as<float>(); // may be overwritten below

    // "mixer volume" is 0-100 (or negative when muted in some LMS versions —
    // treat the magnitude as the level and the sign as the mute flag). Falls
    // back to -1 when absent. Some LMS builds also expose an explicit
    // "mixer muting" flag; honor that too when present.
    JsonVariantConst vol = result["mixer volume"];
    if (!vol.isNull()) {
        int v = vol.as<int>();
        if (v < 0) { out->is_muted = true; v = -v; }
        if (v > 100) v = 100;
        out->volume = v;
    }
    JsonVariantConst muting = result["mixer muting"];
    if (!muting.isNull() && muting.as<int>() != 0) out->is_muted = true;

    JsonArrayConst pl = result["playlist_loop"].as<JsonArrayConst>();
    if (!pl.isNull() && pl.size() > 0) {
        JsonVariantConst t = pl[0];
        const char*      s;
        if ((s = t["title"].as<const char*>()))  out->title  = sanitize_text(s);
        if ((s = t["artist"].as<const char*>())) out->artist = sanitize_text(s);
        if ((s = t["album"].as<const char*>()))  out->album  = sanitize_text(s);
        float track_dur = t["duration"].as<float>();
        if (track_dur > 0.5f) out->duration_s = track_dur;

        // For album art we prefer coverid (canonical, works for remote tracks).
        // Fall back to track id if coverid is missing (older LMS or local tracks
        // that haven't been re-scanned). Both fields can be int or string.
        auto pick_id = [](JsonVariantConst v, std::string* out) {
            const char* s2 = v.as<const char*>();
            if (s2 && s2[0]) { *out = s2; return true; }
            long long n = v.as<long long>();
            if (n != 0) {
                char buf[24];
                snprintf(buf, sizeof(buf), "%lld", n);
                *out = buf;
                return true;
            }
            return false;
        };
        if (!pick_id(t["coverid"], &out->track_id)) {
            pick_id(t["id"], &out->track_id);
        }
    }
    out->valid = true;
    return true;
}

const lv_img_dsc_t*
lyrion_fetchArt_HAL(const std::string& track_id) {
    if (!s_inited || track_id.empty()) return nullptr;
    if (WiFi.status() != WL_CONNECTED) return nullptr;

    if (track_id == s_art_track_id && s_art_buf != nullptr) {
        // Already cached
        return &s_art_dsc;
    }

    // Use the universal "current" endpoint with player= param. Unlike
    // /music/<id>/cover.png (which only works for local-library track ids),
    // /music/current/cover.png?player=<mac> resolves whatever the player is
    // playing right now — local, internet radio, Spotify plugin, etc. — and is
    // what jivelite/web UIs use. We still cache by track_id so the fetch only
    // happens on track change, not on every poll.
    if (s_current_index < 0 || s_current_index >= (int) s_players.size()) return nullptr;
    char path[160];
    snprintf(path, sizeof(path), "/music/current/cover_%dx%d.png?player=%s",
             LYRION_ART_PX, LYRION_ART_PX, s_players[s_current_index].id.c_str());

    HTTPClient http;
    http.setConnectTimeout(2000);
    http.setTimeout(5000);
    http.begin(build_url(path));
    int code = http.GET();
    if (code < 200 || code >= 300) {
        omote_log_e("lyrion: art HTTP %d for %s\r\n", code, path);
        http.end();
        return nullptr;
    }
    int contentLen = http.getSize(); // -1 if chunked / unknown
    if (contentLen > 0 && (size_t) contentLen > LYRION_ART_MAX_BYTES) {
        omote_log_e("lyrion: art size %d exceeds cap\r\n", contentLen);
        http.end();
        return nullptr;
    }

    // Allocate either the known length or the cap; we'll shrink the dsc to the
    // actual read count. Reading via Stream::readBytes blocks until count is
    // satisfied or the (now 5s) HTTPClient timeout fires — much more reliable
    // than poll/sleep over WiFiClient::available().
    size_t cap = (contentLen > 0) ? (size_t) contentLen : LYRION_ART_MAX_BYTES;
    uint8_t* buf = (uint8_t*) ps_malloc(cap);
    if (!buf) {
        omote_log_e("lyrion: ps_malloc(%u) failed for art\r\n", (unsigned) cap);
        http.end();
        return nullptr;
    }

    Stream& stream = http.getStream();
    size_t  read   = 0;
    if (contentLen > 0) {
        read = stream.readBytes(buf, (size_t) contentLen);
    } else {
        // chunked / unknown length: read until the stream closes or we hit cap
        while (read < cap) {
            int n = stream.readBytes(buf + read, cap - read);
            if (n <= 0) break;
            read += n;
        }
    }
    http.end();

    if (contentLen > 0 && (int) read != contentLen) {
        omote_log_e("lyrion: art read %u/%d bytes (timeout?)\r\n", (unsigned) read, contentLen);
        free(buf);
        return nullptr;
    }
    if (read < 8) {
        omote_log_e("lyrion: art only %u bytes, no PNG signature possible\r\n", (unsigned) read);
        free(buf);
        return nullptr;
    }

    // Log the first 8 bytes so a wrong format (e.g. JPEG when we asked for PNG)
    // is obvious in the serial log. PNG magic: 89 50 4E 47 0D 0A 1A 0A
    omote_log_i("lyrion: art %u bytes, magic %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                (unsigned) read, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    static const uint8_t png_magic[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(buf, png_magic, 8) != 0) {
        omote_log_e("lyrion: art is not a PNG — LMS likely returned a different format\r\n");
        free(buf);
        return nullptr;
    }

    // Free old buffer, swap in new one
    if (s_art_buf) free(s_art_buf);
    s_art_buf      = buf;
    s_art_len      = read;
    s_art_track_id = track_id;

    // Hand the raw PNG bytes to LVGL. With cf=0 and w/h=0, the LVGL PNG decoder
    // (lv_png.c) reads dimensions from the PNG IHDR and decodes to the system's
    // native TRUE_COLOR_ALPHA format on open(). Setting cf to RAW_ALPHA here is
    // wrong — the widget would treat the decoded data as raw and render "No data".
    // The cache key is the descriptor pointer, which is stable across swaps, so
    // we must explicitly invalidate or LVGL serves the previously-decoded image.
    lv_img_cache_invalidate_src(&s_art_dsc);
    memset(&s_art_dsc, 0, sizeof(s_art_dsc));
    s_art_dsc.data_size = s_art_len;
    s_art_dsc.data      = s_art_buf;

    omote_log_i("lyrion: art %u bytes for track %s (free PSRAM=%u)\r\n", (unsigned) read, track_id.c_str(),
                (unsigned) ESP.getFreePsram());
    return &s_art_dsc;
}

void
lyrion_releaseArt_HAL(void) {
    if (s_art_buf) {
        lv_img_cache_invalidate_src(&s_art_dsc);
        free(s_art_buf);
        s_art_buf = nullptr;
        s_art_len = 0;
        s_art_track_id.clear();
        memset(&s_art_dsc, 0, sizeof(s_art_dsc));
    }
}

// --- Library browse ---------------------------------------------------------

// Read a field that LMS may return as either a JSON string or number into a
// std::string (favorites item ids look like "f1a2.0"; playlist ids are ints).
static std::string
browse_id_to_string(JsonVariantConst v) {
    const char* s = v.as<const char*>();
    if (s && s[0]) return std::string(s);
    if (!v.isNull() && v.is<long long>()) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%lld", v.as<long long>());
        return std::string(buf);
    }
    return std::string();
}

bool
lyrion_browseFavorites_HAL(const std::string& item_id, std::vector<LyrionBrowseItem>* out) {
    if (!out || !s_inited) return false;
    out->clear();
    // Server-level read (empty player id). Append item_id only when drilling in.
    String cmd = F("[\"favorites\",\"items\",\"0\",\"200\",\"want_url:1\"");
    if (!item_id.empty()) {
        String esc = quote_json_string(item_id);            // "..." escaped
        cmd += ",\"item_id:";
        cmd += esc.substring(1, esc.length() - 1);          // inner text, no quotes
        cmd += "\"";
    }
    cmd += "]";
    String body = build_rpc_body(F("\"\""), cmd);
    String resp = post_rpc(body);
    if (resp.isEmpty()) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        omote_log_e("lyrion: favorites JSON parse failed: %s\r\n", err.c_str());
        return false;
    }
    JsonArrayConst loop = doc["result"]["loop_loop"].as<JsonArrayConst>();
    if (loop.isNull()) {
        omote_log_w("lyrion: favorites response missing loop_loop\r\n");
        return true; // empty list, not an error
    }
    for (JsonVariantConst it : loop) {
        LyrionBrowseItem e;
        const char* name = it["name"].as<const char*>();
        e.title    = sanitize_text(name ? name : "");
        e.id       = browse_id_to_string(it["id"]);
        e.hasitems = (it["hasitems"].as<int>() != 0);
        e.isaudio  = (it["isaudio"].as<int>() != 0);
        e.type     = e.hasitems ? LIT_FOLDER : LIT_FAVORITE;
        if (!e.title.empty()) out->push_back(e);
    }
    return true;
}

bool
lyrion_browsePlaylists_HAL(std::vector<LyrionBrowseItem>* out) {
    if (!out || !s_inited) return false;
    out->clear();
    String body = build_rpc_body(F("\"\""), F("[\"playlists\",\"0\",\"200\"]"));
    String resp = post_rpc(body);
    if (resp.isEmpty()) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        omote_log_e("lyrion: playlists JSON parse failed: %s\r\n", err.c_str());
        return false;
    }
    JsonArrayConst loop = doc["result"]["playlists_loop"].as<JsonArrayConst>();
    if (loop.isNull()) {
        omote_log_w("lyrion: playlists response missing playlists_loop\r\n");
        return true;
    }
    for (JsonVariantConst it : loop) {
        LyrionBrowseItem e;
        const char* name = it["playlist"].as<const char*>();
        e.title   = sanitize_text(name ? name : "");
        e.id      = browse_id_to_string(it["id"]);
        e.isaudio = true; // playlists load+play as a leaf
        e.type    = LIT_PLAYLIST;
        if (!e.title.empty()) out->push_back(e);
    }
    return true;
}

bool
lyrion_playFavorite_HAL(const std::string& item_id) {
    String esc = quote_json_string(item_id); // "..." with " and \ escaped
    String cmd = String("[\"favorites\",\"playlist\",\"play\",\"item_id:") +
                 esc.substring(1, esc.length() - 1) + "\"]";
    return lyrion_sendCommand_HAL(std::string(cmd.c_str()));
}

bool
lyrion_playPlaylist_HAL(const std::string& playlist_id) {
    String esc = quote_json_string(playlist_id);
    String cmd = String("[\"playlistcontrol\",\"cmd:load\",\"playlist_id:") +
                 esc.substring(1, esc.length() - 1) + "\"]";
    return lyrion_sendCommand_HAL(std::string(cmd.c_str()));
}

bool
lyrion_playUrl_HAL(const std::string& url, const std::string& title) {
    String cmd = String("[\"playlist\",\"play\",") +
                 quote_json_string(url) + "," + quote_json_string(title) + "]";
    return lyrion_sendCommand_HAL(std::string(cmd.c_str()));
}

// --- Full library browse + search ------------------------------------------

// Append a `,"key:value"` token to a command array when value is non-empty.
// The value is JSON-escaped (quote_json_string) and stripped of its quotes.
static void
append_tag(String& cmd, const char* key, const std::string& value) {
    if (value.empty()) return;
    String esc = quote_json_string(value);
    cmd += ",\"";
    cmd += key;
    cmd += ":";
    cmd += esc.substring(1, esc.length() - 1);
    cmd += "\"";
}

// Shared list fetch: POST `cmd`, parse `result[loop_key]` into `out`, read
// `result.count` into *total. `name_key` is the per-item display field; ids
// come from per-item "id". `type` is assigned to every produced item.
static bool
browse_list_common(const String& cmd, const char* loop_key, const char* name_key,
                   LyrionItemType type, std::vector<LyrionBrowseItem>* out, int* total) {
    if (!out || !s_inited) return false;
    out->clear();
    if (total) *total = 0;
    String body = build_rpc_body(F("\"\""), cmd);
    String resp = post_rpc(body);
    if (resp.isEmpty()) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        omote_log_e("lyrion: %s JSON parse failed: %s\r\n", loop_key, err.c_str());
        return false;
    }
    JsonVariantConst result = doc["result"];
    if (total) *total = result["count"].as<int>();
    JsonArrayConst loop = result[loop_key].as<JsonArrayConst>();
    if (loop.isNull()) return true; // empty list, not an error
    for (JsonVariantConst it : loop) {
        LyrionBrowseItem e;
        const char* name = it[name_key].as<const char*>();
        e.title = sanitize_text(name ? name : "");
        e.id    = browse_id_to_string(it["id"]);
        e.type  = type;
        if (!e.title.empty()) out->push_back(e);
    }
    return true;
}

bool
lyrion_browseArtists_HAL(const std::string& genre_id, const std::string& search,
                         int start, int count, std::vector<LyrionBrowseItem>* out, int* total) {
    String cmd = String("[\"artists\",\"") + start + "\",\"" + count + "\"";
    append_tag(cmd, "genre_id", genre_id);
    append_tag(cmd, "search", search);
    cmd += "]";
    return browse_list_common(cmd, "artists_loop", "artist", LIT_ARTIST, out, total);
}

bool
lyrion_browseAlbums_HAL(const std::string& artist_id, const std::string& genre_id,
                        const std::string& search, int start, int count,
                        std::vector<LyrionBrowseItem>* out, int* total) {
    String cmd = String("[\"albums\",\"") + start + "\",\"" + count + "\",\"tags:l\",\"sort:album\"";
    append_tag(cmd, "artist_id", artist_id);
    append_tag(cmd, "genre_id", genre_id);
    append_tag(cmd, "search", search);
    cmd += "]";
    return browse_list_common(cmd, "albums_loop", "album", LIT_ALBUM, out, total);
}

bool
lyrion_browseGenres_HAL(int start, int count, std::vector<LyrionBrowseItem>* out, int* total) {
    String cmd = String("[\"genres\",\"") + start + "\",\"" + count + "\"]";
    return browse_list_common(cmd, "genres_loop", "genre", LIT_GENRE, out, total);
}

bool
lyrion_browseTracks_HAL(const std::string& album_id, const std::string& artist_id,
                        const std::string& search, int start, int count,
                        std::vector<LyrionBrowseItem>* out, int* total) {
    String cmd = String("[\"titles\",\"") + start + "\",\"" + count + "\",\"tags:t\",\"sort:tracknum\"";
    append_tag(cmd, "album_id", album_id);
    append_tag(cmd, "artist_id", artist_id);
    append_tag(cmd, "search", search);
    cmd += "]";
    return browse_list_common(cmd, "titles_loop", "title", LIT_TRACK, out, total);
}

bool
lyrion_searchCounts_HAL(const std::string& term, int* artists, int* albums, int* tracks) {
    if (!s_inited) return false;
    if (artists) *artists = 0;
    if (albums) *albums = 0;
    if (tracks) *tracks = 0;
    String cmd = F("[\"search\",\"0\",\"1\"");
    append_tag(cmd, "term", term);
    cmd += "]";
    String resp = post_rpc(build_rpc_body(F("\"\""), cmd));
    if (resp.isEmpty()) return false;

    JsonDocument doc;
    if (deserializeJson(doc, resp)) return false;
    JsonVariantConst result = doc["result"];
    if (artists) *artists = result["contributors_count"].as<int>();
    if (albums) *albums = result["albums_count"].as<int>();
    if (tracks) *tracks = result["tracks_count"].as<int>();
    return true;
}

bool
lyrion_playSelector_HAL(const std::string& selector) {
    String esc = quote_json_string(selector);
    String cmd = String("[\"playlistcontrol\",\"cmd:load\",") + esc + "]";
    return lyrion_sendCommand_HAL(std::string(cmd.c_str()));
}

// --- LMS apps / radio plugins (XMLBrowser/OPML interface) -------------------

// Read a plugin's query command from an apps/radios entry. Usually a plain
// string ("radioparadise"); some versions wrap it in a single-element array.
static std::string
read_app_cmd(JsonVariantConst entry) {
    JsonVariantConst cmd = entry["cmd"];
    if (cmd.is<JsonArrayConst>()) {
        JsonArrayConst a = cmd.as<JsonArrayConst>();
        if (a.size() > 0 && a[0].as<const char*>()) return a[0].as<const char*>();
        return std::string();
    }
    const char* s = cmd.as<const char*>();
    return s ? std::string(s) : std::string();
}

// JSON for the current player id, or "" when none is selected. App/radio
// plugins (Radio Paradise et al.) reference the client in their XMLBrowser
// handlers, so their "items"/"playlist play" queries must carry the player id —
// a server-level ("") request makes the plugin handler throw and LMS drops the
// connection (seen as HTTP error -5).
static String
current_player_id_json(void) {
    if (s_current_index >= 0 && s_current_index < (int) s_players.size()) {
        return quote_json_string(s_players[s_current_index].id);
    }
    return String("\"\"");
}

// Append entries from one apps/radios loop into `out`, skipping cmds already
// present (radios and apps overlap on some servers).
static void
collect_apps(JsonVariantConst result, const char* loop_key, std::vector<LyrionBrowseItem>* out) {
    JsonArrayConst loop = result[loop_key].as<JsonArrayConst>();
    if (loop.isNull()) return;
    for (JsonVariantConst it : loop) {
        LyrionBrowseItem e;
        e.id   = read_app_cmd(it);
        if (e.id.empty()) continue;
        bool dup = false;
        for (const auto& x : *out) if (x.id == e.id) { dup = true; break; }
        if (dup) continue;
        const char* nm = it["name"].as<const char*>();
        e.title = sanitize_text(nm ? nm : e.id.c_str());
        e.type  = LIT_APP;
        if (!e.title.empty()) out->push_back(e);
    }
}

bool
lyrion_browseApps_HAL(std::vector<LyrionBrowseItem>* out) {
    if (!out || !s_inited) return false;
    out->clear();
    // "radios" lists internet-radio plugins (Radio Paradise et al.); "apps"
    // lists the rest (Spotify, podcasts, ...). Query both and merge.
    static const char* const queries[] = {"radios", "apps"};
    static const char* const loop_keys[] = {"radioss_loop", "appss_loop"};
    bool any = false;
    for (int i = 0; i < 2; ++i) {
        String cmd  = String("[\"") + queries[i] + "\",\"0\",\"200\"]";
        String resp = post_rpc(build_rpc_body(current_player_id_json(), cmd), LYRION_APP_BROWSE_TIMEOUT_MS);
        if (resp.isEmpty()) continue;
        JsonDocument doc;
        if (deserializeJson(doc, resp)) {
            omote_log_e("lyrion: %s JSON parse failed\r\n", queries[i]);
            continue;
        }
        collect_apps(doc["result"], loop_keys[i], out);
        any = true;
    }
    return any;
}

bool
lyrion_browseAppItems_HAL(const std::string& app_cmd, const std::string& item_id,
                          std::vector<LyrionBrowseItem>* out) {
    if (!out || !s_inited || app_cmd.empty()) return false;
    out->clear();
    String esc = quote_json_string(app_cmd);
    String cmd = String("[") + esc + ",\"items\",\"0\",\"200\"";
    if (!item_id.empty()) {
        String iesc = quote_json_string(item_id);
        cmd += ",\"item_id:";
        cmd += iesc.substring(1, iesc.length() - 1);
        cmd += "\"";
    }
    cmd += "]";
    String resp = post_rpc(build_rpc_body(current_player_id_json(), cmd), LYRION_APP_BROWSE_TIMEOUT_MS);
    if (resp.isEmpty()) return false;

    JsonDocument doc;
    if (deserializeJson(doc, resp)) {
        omote_log_e("lyrion: app items JSON parse failed\r\n");
        return false;
    }
    JsonVariantConst result = doc["result"];
    // XMLBrowser plugins normally answer under "item_loop"; a few mirror the
    // favorites shape ("loop_loop"). Accept either.
    JsonArrayConst loop = result["item_loop"].as<JsonArrayConst>();
    if (loop.isNull()) loop = result["loop_loop"].as<JsonArrayConst>();
    if (loop.isNull()) {
        // Dump the result object's keys so the real loop name is visible in the
        // log (different plugins/LMS versions vary).
        omote_log_w("lyrion: app items missing item_loop (cmd='%s' item_id='%s'); result keys:\r\n",
                    app_cmd.c_str(), item_id.c_str());
        JsonObjectConst obj = result.as<JsonObjectConst>();
        if (!obj.isNull()) {
            for (JsonPairConst kv : obj) omote_log_w("lyrion:   '%s'\r\n", kv.key().c_str());
        } else {
            omote_log_w("lyrion: result is not an object; resp[0..200]=%.200s\r\n", resp.c_str());
        }
        return true; // empty menu, not an error
    }
    for (JsonVariantConst it : loop) {
        LyrionBrowseItem e;
        const char* nm = it["name"].as<const char*>();
        if (!nm) nm = it["title"].as<const char*>();
        e.title    = sanitize_text(nm ? nm : "");
        e.id       = browse_id_to_string(it["id"]);
        e.hasitems = (it["hasitems"].as<int>() != 0);
        e.isaudio  = (it["isaudio"].as<int>() != 0);
        // Drillable nodes and bare links both drill via "items"; only audio
        // leaves play. Default non-audio to a folder so it stays navigable.
        e.type = (e.isaudio && !e.hasitems) ? LIT_APP_AUDIO : LIT_APP_FOLDER;
        if (!e.title.empty() && !e.id.empty()) out->push_back(e);
    }
    return true;
}

bool
lyrion_playAppItem_HAL(const std::string& app_cmd, const std::string& item_id) {
    if (!s_inited || app_cmd.empty() || item_id.empty()) return false;
    if (s_current_index < 0 || s_current_index >= (int) s_players.size()) return false;
    String aesc = quote_json_string(app_cmd);
    String iesc = quote_json_string(item_id);
    String cmd = String("[") + aesc + ",\"playlist\",\"play\",\"item_id:" +
                 iesc.substring(1, iesc.length() - 1) + "\"]";
    // Like the browse calls, the plugin may resolve the stream upstream before
    // replying — use the longer timeout, not lyrion_sendCommand_HAL's 2 s.
    String pid  = quote_json_string(s_players[s_current_index].id);
    String resp = post_rpc(build_rpc_body(pid, cmd), LYRION_APP_BROWSE_TIMEOUT_MS);
    return !resp.isEmpty();
}

#endif // ENABLE_LYRION
