/*
 * Lyrion Music Server (LMS) HAL — desktop simulator (macOS / Linux)
 *
 * Mirrors the ESP32 implementation but uses POSIX sockets and cJSON
 * instead of HTTPClient / ArduinoJson / Preferences. Player selection
 * is not persisted between runs; the build-time LYRION_PLAYER_NAME (or
 * the first discovered player) is used each time the sim starts.
 */

#include "lyrion_hal_windows_linux.h"

#if (ENABLE_LYRION == 1)

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "cJSON.h"
#include "secrets.h"

static const int    LYRION_ART_PX        = 200;
static const size_t LYRION_ART_MAX_BYTES = 200 * 1024; // sim has plenty of RAM

// Read timeout for app/radio plugin browses — they make LMS fetch menus from
// the internet before replying, so they need far longer than a local query.
static const int LYRION_APP_BROWSE_TIMEOUT_MS = 10000;

struct PlayerEntry {
    std::string id;
    std::string name;
};

static bool                     s_inited        = false;
static unsigned                 s_request_id    = 1; // unsigned: wraps cleanly, no signed-overflow UB
static std::vector<PlayerEntry> s_players;
static int                      s_current_index = -1;

static uint8_t*     s_art_buf = nullptr;
static size_t       s_art_len = 0;
static std::string  s_art_track_id;
static lv_img_dsc_t s_art_dsc;

// Transliterate UTF-8 to printable ASCII — see ESP32 HAL for the rationale.
static void
append_ascii(std::string& out, uint32_t cp) {
    if (cp < 0x80) { out += (char) cp; return; }
    switch (cp) {
        case 0x2018: case 0x2019: case 0x201A: case 0x201B:
        case 0x02BC: case 0x2032: case 0x00B4: case 0x0060:
            out += '\''; return;
        case 0x201C: case 0x201D: case 0x201E: case 0x201F:
        case 0x2033:
            out += '"'; return;
        case 0x2010: case 0x2011: case 0x2012: case 0x2013:
        case 0x2014: case 0x2015: case 0x2212:
            out += '-'; return;
        case 0x2026: out += "..."; return;
        case 0x00A0: case 0x2007: case 0x2009: case 0x202F:
            out += ' '; return;
    }
    static const char* const latin1 =  // U+00C0 .. U+00FF
        "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTs"   // C0-DF (xD7 mult sign, DE Th->T, DF ss->s)
        "aaaaaaaceeeeiiiidnooooo/ouuuuyty";  // E0-FF (xF7 div sign)
    if (cp >= 0x00C0 && cp <= 0x00FF) { out += latin1[cp - 0x00C0]; return; }
    out += '?';
}

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
        else                        { p += 1; continue; }
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

static std::string
quote_json_string(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    out += "\"";
    return out;
}

static std::string
build_rpc_body(const std::string& player_id_json, const std::string& command_array_json) {
    char id_str[24];
    snprintf(id_str, sizeof(id_str), "%u", s_request_id++);
    std::string body = "{\"id\":";
    body += id_str;
    body += ",\"method\":\"slim.request\",\"params\":[";
    body += player_id_json;
    body += ",";
    body += command_array_json;
    body += "]}";
    return body;
}

static bool
tcp_connect(const std::string& host, int port, int& out_sock, int timeout_ms) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host.c_str(), portstr, &hints, &res) != 0) return false;
    int s = -1;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == -1) continue;
        if (connect(s, p->ai_addr, p->ai_addrlen) == 0) break;
        close(s);
        s = -1;
    }
    freeaddrinfo(res);
    if (s == -1) return false;

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    out_sock = s;
    return true;
}

static bool
send_all(int sock, const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*) data;
    while (len > 0) {
        ssize_t n = ::send(sock, p, len, 0);
        if (n <= 0) return false;
        p += n;
        len -= n;
    }
    return true;
}

// Read entire HTTP response into `out`. Returns status code, or -1 on error.
// `out_body_offset` is the offset of the response body within `out`.
static int
http_recv(int sock, std::vector<uint8_t>& out, size_t& out_body_offset, size_t max_body) {
    out.clear();
    out_body_offset = 0;
    char    buf[4096];
    ssize_t n;
    // 1. Read until we see end-of-headers
    while (true) {
        n = ::recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) return -1;
        out.insert(out.end(), buf, buf + n);
        // search for \r\n\r\n
        for (size_t i = 3; i < out.size(); ++i) {
            if (out[i - 3] == '\r' && out[i - 2] == '\n' && out[i - 1] == '\r' && out[i] == '\n') {
                out_body_offset = i + 1;
                goto headers_done;
            }
        }
        if (out.size() > 16 * 1024) return -1; // headers too big
    }
headers_done:;
    // 2. Parse status line + Content-Length / Transfer-Encoding
    std::string headers((const char*) out.data(), out_body_offset);
    int         status = -1;
    {
        // "HTTP/1.1 200 OK\r\n..."
        size_t sp1 = headers.find(' ');
        if (sp1 == std::string::npos) return -1;
        size_t sp2 = headers.find(' ', sp1 + 1);
        if (sp2 == std::string::npos) return -1;
        status = atoi(headers.substr(sp1 + 1, sp2 - sp1 - 1).c_str());
    }
    // Lower-case header search
    auto find_header_value = [&](const char* key) -> std::string {
        std::string lower_h;
        lower_h.reserve(headers.size());
        for (char c : headers) lower_h += (char) tolower((unsigned char) c);
        std::string lower_k;
        for (const char* p = key; *p; ++p) lower_k += (char) tolower((unsigned char) *p);
        size_t pos = lower_h.find("\r\n" + lower_k + ":");
        if (pos == std::string::npos) return "";
        pos += 2 + lower_k.size() + 1;
        size_t end = lower_h.find("\r\n", pos);
        std::string v = headers.substr(pos, end - pos);
        // trim
        size_t a = v.find_first_not_of(" \t");
        size_t b = v.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        return v.substr(a, b - a + 1);
    };
    std::string cl = find_header_value("Content-Length");
    std::string te = find_header_value("Transfer-Encoding");

    if (!cl.empty()) {
        size_t content_len = (size_t) strtoul(cl.c_str(), nullptr, 10);
        if (content_len > max_body) return -1;
        size_t already   = out.size() - out_body_offset;
        size_t remaining = (content_len > already) ? (content_len - already) : 0;
        out.reserve(out_body_offset + content_len);
        while (remaining > 0) {
            n = ::recv(sock, buf, std::min(remaining, sizeof(buf)), 0);
            if (n <= 0) return -1;
            out.insert(out.end(), buf, buf + n);
            remaining -= (size_t) n;
        }
    } else if (te.find("chunked") != std::string::npos) {
        // Drain chunked body. We re-parse from out_body_offset onward.
        std::vector<uint8_t> body;
        body.insert(body.end(), out.begin() + out_body_offset, out.end());
        out.resize(out_body_offset);
        size_t pos = 0;
        while (true) {
            // ensure we have a line for chunk size
            size_t nl = std::string::npos;
            while (true) {
                for (size_t i = pos; i + 1 < body.size(); ++i) {
                    if (body[i] == '\r' && body[i + 1] == '\n') {
                        nl = i;
                        break;
                    }
                }
                if (nl != std::string::npos) break;
                n = ::recv(sock, buf, sizeof(buf), 0);
                if (n <= 0) return -1;
                body.insert(body.end(), buf, buf + n);
            }
            std::string len_line((const char*) &body[pos], nl - pos);
            size_t      chunk_len = (size_t) strtoul(len_line.c_str(), nullptr, 16);
            pos                   = nl + 2;
            if (chunk_len == 0) break;
            while (body.size() < pos + chunk_len + 2) {
                n = ::recv(sock, buf, sizeof(buf), 0);
                if (n <= 0) return -1;
                body.insert(body.end(), buf, buf + n);
            }
            if (out.size() + chunk_len - out_body_offset > max_body) return -1;
            out.insert(out.end(), body.begin() + pos, body.begin() + pos + chunk_len);
            pos += chunk_len + 2; // skip data + trailing \r\n
        }
    }
    return status;
}

// POST a JSON-RPC body and return the response body as a string. `timeout_ms`
// is the socket read/write timeout: short for local queries, longer for
// app/plugin browses that make LMS fetch menus from the internet first.
static std::string
post_rpc(const std::string& body, int timeout_ms = 2000) {
    int s = -1;
    if (!tcp_connect(LYRION_HOST, (int) LYRION_PORT, s, timeout_ms)) {
        std::fprintf(stderr, "lyrion (sim): connect to %s:%d failed\n", LYRION_HOST, (int) LYRION_PORT);
        return std::string();
    }
    char req[512];
    int  hdr_len = snprintf(req,
                            sizeof(req),
                            "POST /jsonrpc.js HTTP/1.1\r\n"
                            "Host: %s:%d\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: %zu\r\n"
                            "Connection: close\r\n"
                            "\r\n",
                            LYRION_HOST,
                            (int) LYRION_PORT,
                            body.size());
    if (!send_all(s, req, hdr_len) || !send_all(s, body.data(), body.size())) {
        close(s);
        return std::string();
    }
    std::vector<uint8_t> resp;
    size_t               body_off = 0;
    int                  status   = http_recv(s, resp, body_off, 1024 * 1024);
    close(s);
    if (status < 200 || status >= 300) {
        std::fprintf(stderr, "lyrion (sim): POST HTTP %d\n", status);
        return std::string();
    }
    return std::string((const char*) resp.data() + body_off, resp.size() - body_off);
}

void
init_lyrion_HAL(void) {
    s_inited = true;
    memset(&s_art_dsc, 0, sizeof(s_art_dsc));
    std::printf("lyrion (sim): configured for http://%s:%d/jsonrpc.js\n", LYRION_HOST, (int) LYRION_PORT);
}

bool
lyrion_discoverPlayers_HAL(void) {
    if (!s_inited) return false;
    std::string resp = post_rpc(build_rpc_body("\"\"", "[\"players\",\"0\",\"99\"]"));
    if (resp.empty()) return false;

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) return false;
    cJSON* result = cJSON_GetObjectItem(root, "result");
    cJSON* loop   = result ? cJSON_GetObjectItem(result, "players_loop") : nullptr;
    if (!loop || !cJSON_IsArray(loop)) {
        cJSON_Delete(root);
        return false;
    }
    s_players.clear();
    int n = cJSON_GetArraySize(loop);
    for (int i = 0; i < n; ++i) {
        cJSON* p    = cJSON_GetArrayItem(loop, i);
        cJSON* pid  = cJSON_GetObjectItem(p, "playerid");
        cJSON* name = cJSON_GetObjectItem(p, "name");
        PlayerEntry e;
        if (pid && cJSON_IsString(pid)) e.id = pid->valuestring;
        if (name && cJSON_IsString(name)) e.name = name->valuestring;
        if (!e.id.empty()) s_players.push_back(e);
    }
    cJSON_Delete(root);

    if (s_players.empty()) {
        s_current_index = -1;
        return false;
    }
    std::printf("lyrion (sim): discovered %zu players\n", s_players.size());
    for (size_t i = 0; i < s_players.size(); ++i) {
        std::printf("lyrion (sim):   [%zu] %s  (%s)\n", i, s_players[i].name.c_str(), s_players[i].id.c_str());
    }

    s_current_index   = 0;
    const char* pref  = LYRION_PLAYER_NAME;
    if (pref && pref[0] != '\0') {
        for (size_t i = 0; i < s_players.size(); ++i) {
            if (s_players[i].name == pref) {
                s_current_index = (int) i;
                break;
            }
        }
    }
    std::printf("lyrion (sim): selected player [%d] %s\n", s_current_index, s_players[s_current_index].name.c_str());
    return true;
}

bool
lyrion_cyclePlayer_HAL(int direction) {
    if (s_players.size() < 2) return false;
    int n           = (int) s_players.size();
    s_current_index = ((s_current_index + direction) % n + n) % n;
    std::printf("lyrion (sim): cycled to player [%d] %s\n", s_current_index, s_players[s_current_index].name.c_str());
    lyrion_releaseArt_HAL();
    return true;
}

bool
lyrion_sendCommand_HAL(const std::string& command_array_json) {
    if (!s_inited) return false;
    if (s_current_index < 0 || s_current_index >= (int) s_players.size()) return false;
    std::string pid  = quote_json_string(s_players[s_current_index].id);
    std::string resp = post_rpc(build_rpc_body(pid, command_array_json));
    return !resp.empty();
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
        std::string pid = quote_json_string(p.id);
        if (post_rpc(build_rpc_body(pid, "[\"power\",\"0\"]")).empty()) ok = false;
    }
    return ok;
}

bool
lyrion_pollStatus_HAL(LyrionStatus* out) {
    if (!out) return false;
    *out = LyrionStatus{};
    if (!s_inited || s_current_index < 0 || s_current_index >= (int) s_players.size()) return false;

    out->player_name = s_players[s_current_index].name;

    std::string pid  = quote_json_string(s_players[s_current_index].id);
    std::string resp = post_rpc(build_rpc_body(pid, "[\"status\",\"-\",\"1\",\"tags:aAlc\"]"));
    if (resp.empty()) return false;

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) return false;
    cJSON* result = cJSON_GetObjectItem(root, "result");
    if (!result) {
        cJSON_Delete(root);
        return false;
    }
    cJSON* mode = cJSON_GetObjectItem(result, "mode");
    out->is_playing = (mode && cJSON_IsString(mode) && strcmp(mode->valuestring, "play") == 0);
    cJSON* power = cJSON_GetObjectItem(result, "power");
    out->is_powered = (power && cJSON_IsNumber(power) && power->valueint != 0);
    cJSON* t_time = cJSON_GetObjectItem(result, "time");
    if (t_time && cJSON_IsNumber(t_time)) out->elapsed_s = (float) t_time->valuedouble;
    cJSON* t_dur = cJSON_GetObjectItem(result, "duration");
    if (t_dur && cJSON_IsNumber(t_dur)) out->duration_s = (float) t_dur->valuedouble;
    cJSON* mv = cJSON_GetObjectItem(result, "mixer volume");
    if (mv && cJSON_IsNumber(mv)) {
        int v = mv->valueint;
        if (v < 0) { out->is_muted = true; v = -v; }
        if (v > 100) v = 100;
        out->volume = v;
    }
    cJSON* muting = cJSON_GetObjectItem(result, "mixer muting");
    if (muting && cJSON_IsNumber(muting) && muting->valueint != 0) out->is_muted = true;

    cJSON* pl = cJSON_GetObjectItem(result, "playlist_loop");
    if (pl && cJSON_IsArray(pl) && cJSON_GetArraySize(pl) > 0) {
        cJSON* t      = cJSON_GetArrayItem(pl, 0);
        cJSON* title  = cJSON_GetObjectItem(t, "title");
        cJSON* artist = cJSON_GetObjectItem(t, "artist");
        cJSON* album  = cJSON_GetObjectItem(t, "album");
        if (title && cJSON_IsString(title)) out->title = sanitize_text(title->valuestring);
        if (artist && cJSON_IsString(artist)) out->artist = sanitize_text(artist->valuestring);
        if (album && cJSON_IsString(album)) out->album = sanitize_text(album->valuestring);
        cJSON* tdur = cJSON_GetObjectItem(t, "duration");
        if (tdur && cJSON_IsNumber(tdur) && tdur->valuedouble > 0.5) {
            out->duration_s = (float) tdur->valuedouble;
        }
        auto pick_id = [](cJSON* v, std::string* out_id) -> bool {
            if (!v) return false;
            if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
                *out_id = v->valuestring;
                return true;
            }
            if (cJSON_IsNumber(v) && v->valuedouble != 0.0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long) v->valuedouble);
                *out_id = buf;
                return true;
            }
            return false;
        };
        if (!pick_id(cJSON_GetObjectItem(t, "coverid"), &out->track_id)) {
            pick_id(cJSON_GetObjectItem(t, "id"), &out->track_id);
        }
    }
    cJSON_Delete(root);
    out->valid = true;
    return true;
}

const lv_img_dsc_t*
lyrion_fetchArt_HAL(const std::string& track_id) {
    if (!s_inited || track_id.empty()) return nullptr;
    if (track_id == s_art_track_id && s_art_buf != nullptr) return &s_art_dsc;
    if (s_current_index < 0 || s_current_index >= (int) s_players.size()) return nullptr;

    char path[160];
    snprintf(path, sizeof(path), "/music/current/cover_%dx%d.png?player=%s",
             LYRION_ART_PX, LYRION_ART_PX, s_players[s_current_index].id.c_str());

    int sock = -1;
    if (!tcp_connect(LYRION_HOST, (int) LYRION_PORT, sock, 5000)) return nullptr;
    char req[512];
    int  req_len = snprintf(req,
                            sizeof(req),
                            "GET %s HTTP/1.1\r\n"
                            "Host: %s:%d\r\n"
                            "Connection: close\r\n"
                            "\r\n",
                            path,
                            LYRION_HOST,
                            (int) LYRION_PORT);
    if (!send_all(sock, req, req_len)) {
        close(sock);
        return nullptr;
    }
    std::vector<uint8_t> resp;
    size_t               body_off = 0;
    int                  status   = http_recv(sock, resp, body_off, LYRION_ART_MAX_BYTES);
    close(sock);
    if (status < 200 || status >= 300) {
        std::fprintf(stderr, "lyrion (sim): art HTTP %d for %s\n", status, path);
        return nullptr;
    }
    size_t art_len = resp.size() - body_off;
    if (art_len < 8) {
        std::fprintf(stderr, "lyrion (sim): art only %zu bytes\n", art_len);
        return nullptr;
    }
    static const uint8_t png_magic[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(resp.data() + body_off, png_magic, 8) != 0) {
        std::fprintf(stderr, "lyrion (sim): art is not a PNG\n");
        return nullptr;
    }
    uint8_t* buf = (uint8_t*) malloc(art_len);
    if (!buf) return nullptr;
    memcpy(buf, resp.data() + body_off, art_len);

    if (s_art_buf) free(s_art_buf);
    s_art_buf      = buf;
    s_art_len      = art_len;
    s_art_track_id = track_id;

    lv_img_cache_invalidate_src(&s_art_dsc);
    memset(&s_art_dsc, 0, sizeof(s_art_dsc));
    s_art_dsc.data_size = s_art_len;
    s_art_dsc.data      = s_art_buf;

    std::printf("lyrion (sim): art %zu bytes for track %s\n", art_len, track_id.c_str());
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

// Read a field that LMS may return as either a JSON string or number.
static std::string
browse_id_to_string(cJSON* v) {
    if (!v) return std::string();
    if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) return v->valuestring;
    if (cJSON_IsNumber(v) && v->valuedouble != 0.0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", (long long) v->valuedouble);
        return std::string(buf);
    }
    return std::string();
}

// Inner text of a JSON-escaped string (drops the surrounding quotes).
static std::string
json_inner(const std::string& s) {
    std::string q = quote_json_string(s);
    return q.substr(1, q.size() - 2);
}

bool
lyrion_browseFavorites_HAL(const std::string& item_id, std::vector<LyrionBrowseItem>* out) {
    if (!out || !s_inited) return false;
    out->clear();
    std::string cmd = "[\"favorites\",\"items\",\"0\",\"200\",\"want_url:1\"";
    if (!item_id.empty()) {
        cmd += ",\"item_id:" + json_inner(item_id) + "\"";
    }
    cmd += "]";
    std::string resp = post_rpc(build_rpc_body("\"\"", cmd));
    if (resp.empty()) return false;

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) return false;
    cJSON* result = cJSON_GetObjectItem(root, "result");
    cJSON* loop   = result ? cJSON_GetObjectItem(result, "loop_loop") : nullptr;
    if (loop && cJSON_IsArray(loop)) {
        int n = cJSON_GetArraySize(loop);
        for (int i = 0; i < n; ++i) {
            cJSON*           it = cJSON_GetArrayItem(loop, i);
            cJSON*           nm = cJSON_GetObjectItem(it, "name");
            cJSON*           ha = cJSON_GetObjectItem(it, "hasitems");
            cJSON*           au = cJSON_GetObjectItem(it, "isaudio");
            LyrionBrowseItem e;
            e.title    = sanitize_text(nm && cJSON_IsString(nm) ? nm->valuestring : "");
            e.id       = browse_id_to_string(cJSON_GetObjectItem(it, "id"));
            e.hasitems = (ha && cJSON_IsNumber(ha) && ha->valueint != 0);
            e.isaudio  = (au && cJSON_IsNumber(au) && au->valueint != 0);
            e.type     = e.hasitems ? LIT_FOLDER : LIT_FAVORITE;
            if (!e.title.empty()) out->push_back(e);
        }
    }
    cJSON_Delete(root);
    return true;
}

bool
lyrion_browsePlaylists_HAL(std::vector<LyrionBrowseItem>* out) {
    if (!out || !s_inited) return false;
    out->clear();
    std::string resp = post_rpc(build_rpc_body("\"\"", "[\"playlists\",\"0\",\"200\"]"));
    if (resp.empty()) return false;

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) return false;
    cJSON* result = cJSON_GetObjectItem(root, "result");
    cJSON* loop   = result ? cJSON_GetObjectItem(result, "playlists_loop") : nullptr;
    if (loop && cJSON_IsArray(loop)) {
        int n = cJSON_GetArraySize(loop);
        for (int i = 0; i < n; ++i) {
            cJSON*           it = cJSON_GetArrayItem(loop, i);
            cJSON*           nm = cJSON_GetObjectItem(it, "playlist");
            LyrionBrowseItem e;
            e.title   = sanitize_text(nm && cJSON_IsString(nm) ? nm->valuestring : "");
            e.id      = browse_id_to_string(cJSON_GetObjectItem(it, "id"));
            e.isaudio = true;
            e.type    = LIT_PLAYLIST;
            if (!e.title.empty()) out->push_back(e);
        }
    }
    cJSON_Delete(root);
    return true;
}

bool
lyrion_playFavorite_HAL(const std::string& item_id) {
    std::string cmd = "[\"favorites\",\"playlist\",\"play\",\"item_id:" + json_inner(item_id) + "\"]";
    return lyrion_sendCommand_HAL(cmd);
}

bool
lyrion_playPlaylist_HAL(const std::string& playlist_id) {
    std::string cmd = "[\"playlistcontrol\",\"cmd:load\",\"playlist_id:" + json_inner(playlist_id) + "\"]";
    return lyrion_sendCommand_HAL(cmd);
}

bool
lyrion_playUrl_HAL(const std::string& url, const std::string& title) {
    std::string cmd = "[\"playlist\",\"play\"," + quote_json_string(url) + "," + quote_json_string(title) + "]";
    return lyrion_sendCommand_HAL(cmd);
}

// --- Full library browse + search ------------------------------------------

// Append a `,"key:value"` token to a command array when value is non-empty.
static void
append_tag(std::string& cmd, const char* key, const std::string& value) {
    if (value.empty()) return;
    cmd += ",\"";
    cmd += key;
    cmd += ":";
    cmd += json_inner(value);
    cmd += "\"";
}

// Shared list fetch: POST `cmd`, parse `result[loop_key]` into `out`, read
// `result.count` into *total. Every produced item is tagged with `type`.
static bool
browse_list_common(const std::string& cmd, const char* loop_key, const char* name_key,
                   LyrionItemType type, std::vector<LyrionBrowseItem>* out, int* total) {
    if (!out || !s_inited) return false;
    out->clear();
    if (total) *total = 0;
    std::string resp = post_rpc(build_rpc_body("\"\"", cmd));
    if (resp.empty()) return false;

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) return false;
    cJSON* result = cJSON_GetObjectItem(root, "result");
    if (!result) {
        cJSON_Delete(root);
        return false;
    }
    cJSON* cnt = cJSON_GetObjectItem(result, "count");
    if (total && cnt && cJSON_IsNumber(cnt)) *total = cnt->valueint;
    cJSON* loop = cJSON_GetObjectItem(result, loop_key);
    if (loop && cJSON_IsArray(loop)) {
        int n = cJSON_GetArraySize(loop);
        for (int i = 0; i < n; ++i) {
            cJSON*           it = cJSON_GetArrayItem(loop, i);
            cJSON*           nm = cJSON_GetObjectItem(it, name_key);
            LyrionBrowseItem e;
            e.title = sanitize_text(nm && cJSON_IsString(nm) ? nm->valuestring : "");
            e.id    = browse_id_to_string(cJSON_GetObjectItem(it, "id"));
            e.type  = type;
            if (!e.title.empty()) out->push_back(e);
        }
    }
    cJSON_Delete(root);
    return true;
}

bool
lyrion_browseArtists_HAL(const std::string& genre_id, const std::string& search,
                         int start, int count, std::vector<LyrionBrowseItem>* out, int* total) {
    std::string cmd = "[\"artists\",\"" + std::to_string(start) + "\",\"" + std::to_string(count) + "\"";
    append_tag(cmd, "genre_id", genre_id);
    append_tag(cmd, "search", search);
    cmd += "]";
    return browse_list_common(cmd, "artists_loop", "artist", LIT_ARTIST, out, total);
}

bool
lyrion_browseAlbums_HAL(const std::string& artist_id, const std::string& genre_id,
                        const std::string& search, int start, int count,
                        std::vector<LyrionBrowseItem>* out, int* total) {
    std::string cmd = "[\"albums\",\"" + std::to_string(start) + "\",\"" + std::to_string(count) +
                      "\",\"tags:l\",\"sort:album\"";
    append_tag(cmd, "artist_id", artist_id);
    append_tag(cmd, "genre_id", genre_id);
    append_tag(cmd, "search", search);
    cmd += "]";
    return browse_list_common(cmd, "albums_loop", "album", LIT_ALBUM, out, total);
}

bool
lyrion_browseGenres_HAL(int start, int count, std::vector<LyrionBrowseItem>* out, int* total) {
    std::string cmd = "[\"genres\",\"" + std::to_string(start) + "\",\"" + std::to_string(count) + "\"]";
    return browse_list_common(cmd, "genres_loop", "genre", LIT_GENRE, out, total);
}

bool
lyrion_browseTracks_HAL(const std::string& album_id, const std::string& artist_id,
                        const std::string& search, int start, int count,
                        std::vector<LyrionBrowseItem>* out, int* total) {
    std::string cmd = "[\"titles\",\"" + std::to_string(start) + "\",\"" + std::to_string(count) +
                      "\",\"tags:t\",\"sort:tracknum\"";
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
    std::string cmd = "[\"search\",\"0\",\"1\"";
    append_tag(cmd, "term", term);
    cmd += "]";
    std::string resp = post_rpc(build_rpc_body("\"\"", cmd));
    if (resp.empty()) return false;

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) return false;
    cJSON* result = cJSON_GetObjectItem(root, "result");
    if (result) {
        cJSON* a = cJSON_GetObjectItem(result, "contributors_count");
        cJSON* b = cJSON_GetObjectItem(result, "albums_count");
        cJSON* t = cJSON_GetObjectItem(result, "tracks_count");
        if (artists && a && cJSON_IsNumber(a)) *artists = a->valueint;
        if (albums && b && cJSON_IsNumber(b)) *albums = b->valueint;
        if (tracks && t && cJSON_IsNumber(t)) *tracks = t->valueint;
    }
    cJSON_Delete(root);
    return true;
}

bool
lyrion_playSelector_HAL(const std::string& selector) {
    std::string cmd = "[\"playlistcontrol\",\"cmd:load\"," + quote_json_string(selector) + "]";
    return lyrion_sendCommand_HAL(cmd);
}

// --- LMS apps / radio plugins (XMLBrowser/OPML interface) -------------------

// Read a plugin's query command from an apps/radios entry. Usually a plain
// string ("radioparadise"); some versions wrap it in a single-element array.
static std::string
read_app_cmd(cJSON* entry) {
    cJSON* cmd = cJSON_GetObjectItem(entry, "cmd");
    if (!cmd) return std::string();
    if (cJSON_IsArray(cmd) && cJSON_GetArraySize(cmd) > 0) {
        cJSON* first = cJSON_GetArrayItem(cmd, 0);
        if (first && cJSON_IsString(first)) return first->valuestring;
        return std::string();
    }
    if (cJSON_IsString(cmd) && cmd->valuestring) return cmd->valuestring;
    return std::string();
}

// JSON for the current player id, or "" when none is selected. App/radio
// plugins reference the client in their XMLBrowser handlers, so their queries
// must carry the player id (a server-level request makes the plugin throw).
static std::string
current_player_id_json(void) {
    if (s_current_index >= 0 && s_current_index < (int) s_players.size()) {
        return quote_json_string(s_players[s_current_index].id);
    }
    return "\"\"";
}

// Append entries from one apps/radios loop into `out`, skipping cmds already
// present (radios and apps overlap on some servers).
static void
collect_apps(cJSON* result, const char* loop_key, std::vector<LyrionBrowseItem>* out) {
    cJSON* loop = result ? cJSON_GetObjectItem(result, loop_key) : nullptr;
    if (!loop || !cJSON_IsArray(loop)) return;
    int n = cJSON_GetArraySize(loop);
    for (int i = 0; i < n; ++i) {
        cJSON*           it = cJSON_GetArrayItem(loop, i);
        LyrionBrowseItem e;
        e.id = read_app_cmd(it);
        if (e.id.empty()) continue;
        bool dup = false;
        for (const auto& x : *out) if (x.id == e.id) { dup = true; break; }
        if (dup) continue;
        cJSON* nm = cJSON_GetObjectItem(it, "name");
        e.title = sanitize_text(nm && cJSON_IsString(nm) ? nm->valuestring : e.id.c_str());
        e.type  = LIT_APP;
        if (!e.title.empty()) out->push_back(e);
    }
}

bool
lyrion_browseApps_HAL(std::vector<LyrionBrowseItem>* out) {
    if (!out || !s_inited) return false;
    out->clear();
    static const char* const queries[]   = {"radios", "apps"};
    static const char* const loop_keys[] = {"radioss_loop", "appss_loop"};
    bool any = false;
    for (int i = 0; i < 2; ++i) {
        std::string cmd  = std::string("[\"") + queries[i] + "\",\"0\",\"200\"]";
        std::string resp = post_rpc(build_rpc_body(current_player_id_json(), cmd), LYRION_APP_BROWSE_TIMEOUT_MS);
        if (resp.empty()) continue;
        cJSON* root = cJSON_Parse(resp.c_str());
        if (!root) continue;
        collect_apps(cJSON_GetObjectItem(root, "result"), loop_keys[i], out);
        cJSON_Delete(root);
        any = true;
    }
    return any;
}

bool
lyrion_browseAppItems_HAL(const std::string& app_cmd, const std::string& item_id,
                          std::vector<LyrionBrowseItem>* out) {
    if (!out || !s_inited || app_cmd.empty()) return false;
    out->clear();
    std::string cmd = "[" + quote_json_string(app_cmd) + ",\"items\",\"0\",\"200\"";
    if (!item_id.empty()) {
        cmd += ",\"item_id:" + json_inner(item_id) + "\"";
    }
    cmd += "]";
    std::string resp = post_rpc(build_rpc_body(current_player_id_json(), cmd), LYRION_APP_BROWSE_TIMEOUT_MS);
    if (resp.empty()) return false;

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) return false;
    cJSON* result = cJSON_GetObjectItem(root, "result");
    cJSON* loop   = result ? cJSON_GetObjectItem(result, "item_loop") : nullptr;
    if (!loop || !cJSON_IsArray(loop)) {
        // A few plugins mirror the favorites shape under "loop_loop".
        loop = result ? cJSON_GetObjectItem(result, "loop_loop") : nullptr;
    }
    if (loop && cJSON_IsArray(loop)) {
        int n = cJSON_GetArraySize(loop);
        for (int i = 0; i < n; ++i) {
            cJSON* it = cJSON_GetArrayItem(loop, i);
            cJSON* nm = cJSON_GetObjectItem(it, "name");
            if (!nm || !cJSON_IsString(nm)) nm = cJSON_GetObjectItem(it, "title");
            cJSON* ha = cJSON_GetObjectItem(it, "hasitems");
            cJSON* au = cJSON_GetObjectItem(it, "isaudio");
            LyrionBrowseItem e;
            e.title    = sanitize_text(nm && cJSON_IsString(nm) ? nm->valuestring : "");
            e.id       = browse_id_to_string(cJSON_GetObjectItem(it, "id"));
            e.hasitems = (ha && cJSON_IsNumber(ha) && ha->valueint != 0);
            e.isaudio  = (au && cJSON_IsNumber(au) && au->valueint != 0);
            e.type     = (e.isaudio && !e.hasitems) ? LIT_APP_AUDIO : LIT_APP_FOLDER;
            if (!e.title.empty() && !e.id.empty()) out->push_back(e);
        }
    }
    cJSON_Delete(root);
    return true;
}

bool
lyrion_playAppItem_HAL(const std::string& app_cmd, const std::string& item_id) {
    if (!s_inited || app_cmd.empty() || item_id.empty()) return false;
    if (s_current_index < 0 || s_current_index >= (int) s_players.size()) return false;
    std::string cmd = "[" + quote_json_string(app_cmd) + ",\"playlist\",\"play\",\"item_id:" +
                      json_inner(item_id) + "\"]";
    // Plugin may resolve the stream upstream before replying — use the longer
    // timeout, not lyrion_sendCommand_HAL's default.
    std::string pid  = quote_json_string(s_players[s_current_index].id);
    std::string resp = post_rpc(build_rpc_body(pid, cmd), LYRION_APP_BROWSE_TIMEOUT_MS);
    return !resp.empty();
}

#endif // ENABLE_LYRION
