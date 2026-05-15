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

struct PlayerEntry {
    std::string id;   // MAC address, e.g. "aa:bb:cc:dd:ee:ff"
    std::string name; // human-readable
};

static bool                     s_inited        = false;
static int                      s_request_id    = 1;
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
    String body = String("{\"id\":") + String(s_request_id++) +
                  ",\"method\":\"slim.request\",\"params\":[" +
                  player_id_json + "," + command_array_json + "]}";
    return body;
}

// Replace common UTF-8 punctuation with ASCII equivalents — the Montserrat
// subset built in doesn't include these glyphs, so they'd render as missing-
// glyph boxes. Cheap byte-scan in place.
//   E2 80 18/19  left/right single quote  -> '
//   E2 80 1C/1D  left/right double quote  -> "
//   E2 80 93/94  en-dash / em-dash        -> -
//   E2 80 A6     horizontal ellipsis      -> ...
static std::string
sanitize_text(const char* s) {
    std::string out;
    if (!s) return out;
    out.reserve(strlen(s));
    for (const unsigned char* p = (const unsigned char*) s; *p; ) {
        if (p[0] == 0xE2 && p[1] == 0x80) {
            switch (p[2]) {
                case 0x18: case 0x19: out += '\'';  p += 3; continue;
                case 0x1C: case 0x1D: out += '"';   p += 3; continue;
                case 0x93: case 0x94: out += '-';   p += 3; continue;
                case 0xA6:            out += "..."; p += 3; continue;
                default: break;
            }
        }
        out += (char) *p++;
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
static String
post_rpc(const String& body) {
    if (WiFi.status() != WL_CONNECTED) {
        omote_log_e("lyrion: WiFi not connected, dropping request\r\n");
        return String();
    }
    HTTPClient http;
    http.setConnectTimeout(2000);
    http.setTimeout(2000);
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
    // treat the magnitude as the level). Falls back to -1 when absent.
    JsonVariantConst vol = result["mixer volume"];
    if (!vol.isNull()) {
        int v = vol.as<int>();
        if (v < 0) v = -v;
        if (v > 100) v = 100;
        out->volume = v;
    }

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

#endif // ENABLE_LYRION
