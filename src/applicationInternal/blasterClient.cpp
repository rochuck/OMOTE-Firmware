#include "blasterClient.h"

#if (ENABLE_WIFI_AND_MQTT == 1) && defined(ARDUINO)

#include "applicationInternal/gui/guiBase.h"
#include "applicationInternal/omote_log.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

// NVS — own namespace so we don't tangle with the existing "settings" file.
constexpr const char* kPrefNamespace = "blaster";
constexpr const char* kKeyEnabled    = "enabled";
constexpr const char* kKeyHost       = "host";        // user override; empty = auto
constexpr const char* kKeyLastIp     = "last_ip";     // auto-cached on success

constexpr const char* kMdnsService = "omote-blaster";
constexpr uint16_t    kBlasterPort = 80;
// Discovery runs off the main task, so a generous connect timeout is fine and
// lets the handshake survive cross-VLAN routing latency (gateway hop + firewall
// eval easily exceeds a few hundred ms on the first connect).
constexpr int         kStatusTimeoutMs = 1500;
// Send happens on the main flow (button press), so keep this tighter — but the
// device is already known-reachable here, so the connect should be quick.
constexpr int         kSendTimeoutMs   = 1000;
constexpr unsigned long kRecheckIntervalMs = 60UL * 1000UL;

bool s_enabled    = true;
bool s_available  = false;
String s_host;            // user override (host or IP) — empty if not set
String s_resolved;        // working address (last_ip or mDNS-resolved)
unsigned long s_next_recheck_ms = 0;
volatile bool s_discover_in_flight = false;
bool s_mdns_started = false;       // MDNS.begin() is global; only call it once

// Single sink for state changes so the status-bar icon stays in sync.
// Only notifies the GUI on actual transitions to keep the LVGL flush queue quiet.
void set_available(bool v) {
    bool changed = (v != s_available);
    s_available = v;
    if (changed) showBlasterAvailable(v);
}

void load_prefs() {
    Preferences p;
    p.begin(kPrefNamespace, true);  // read-only
    s_enabled = p.getBool(kKeyEnabled, true);
    s_host    = p.getString(kKeyHost,    "");
    s_resolved = p.getString(kKeyLastIp, "");
    p.end();
}

void save_last_ip(const String& ip) {
    Preferences p;
    p.begin(kPrefNamespace, false);
    p.putString(kKeyLastIp, ip);
    p.end();
}

// One handshake against a candidate address. Returns true if /status responds
// 2xx with a body that identifies as an omote-blaster.
bool handshake(const String& addr) {
    if (addr.length() == 0) return false;

    String url = "http://" + addr + ":" + String(kBlasterPort) + "/status";
    HTTPClient http;
    http.setTimeout(kStatusTimeoutMs);
    http.setConnectTimeout(kStatusTimeoutMs);
    if (!http.begin(url)) {
        return false;
    }
    // Tell the server to close the socket after responding. Without this, the
    // default HTTP/1.1 keep-alive leaves connections lingering on the blaster's
    // small lwip socket pool, which exhausts after a few requests and makes it
    // stop responding.
    http.setReuse(false);
    int code = http.GET();
    bool ok = false;
    if (code >= 200 && code < 300) {
        String body = http.getString();
        // Cheap identity check; no JSON parser needed.
        ok = (body.indexOf("omote-blaster") >= 0);
    }
    http.end();
    if (ok) {
        omote_log_d("[blaster] handshake %s -> %d ok\r\n", url.c_str(), code);
    } else {
        omote_log_d("[blaster] handshake %s -> %d (no)\r\n", url.c_str(), code);
    }
    return ok;
}

// The ESP32 mDNS responder must be started before any queryService(), or it
// silently returns 0. Other modules (OTA) may also start it, but only when
// their feature is compiled in — so the blaster can't rely on that. begin()
// is global and idempotent-unsafe, so we gate it on our own flag.
void ensure_mdns() {
    if (s_mdns_started) return;
    if (MDNS.begin("OMOTE")) {
        s_mdns_started = true;
        omote_log_d("[blaster] mDNS responder started\r\n");
    } else {
        omote_log_w("[blaster] MDNS.begin() failed\r\n");
    }
}

// mDNS browse for _omote-blaster._tcp. Returns the first IPv4 hit as a string,
// or empty on miss.
String mdns_resolve() {
    ensure_mdns();
    int n = MDNS.queryService(kMdnsService, "tcp");
    if (n <= 0) return String();
    IPAddress ip = MDNS.IP(0);
    String s = ip.toString();
    omote_log_d("[blaster] mDNS resolved %s\r\n", s.c_str());
    return s;
}

void try_discover() {
    // 1) User override (highest priority).
    if (s_host.length() > 0) {
        if (handshake(s_host)) {
            s_resolved = s_host;
            set_available(true);
            return;
        }
    }

    // 2) Cached IP from a previous successful handshake.
    if (s_resolved.length() > 0 && s_resolved != s_host) {
        if (handshake(s_resolved)) {
            set_available(true);
            return;
        }
        omote_log_d("[blaster] cached IP %s failed; falling back to mDNS\r\n",
                    s_resolved.c_str());
    } else if (s_resolved.length() == 0) {
        omote_log_d("[blaster] no cached IP, browsing mDNS...\r\n");
    }

    // 3) mDNS browse.
    String found = mdns_resolve();
    if (found.length() > 0 && handshake(found)) {
        s_resolved = found;
        save_last_ip(found);
        set_available(true);
        return;
    }

    omote_log_i("[blaster] not available — falling back to local IR\r\n");
    set_available(false);
}

// Reassemble payloads → "data" + parsed nbits/repeat. Mirrors what
// sendIRcode_HAL does with the colon-separated form. Returns false on
// malformed input (caller should fall back to local IR).
bool extract_data(const std::list<std::string>& payloads,
                  const std::string& additionalPayload,
                  String& out_data,
                  int& out_nbits,
                  int& out_repeat) {
    std::string src;
    if (!additionalPayload.empty()) {
        src = additionalPayload;
    } else if (!payloads.empty()) {
        src = payloads.front();
    } else {
        return false;
    }

    // Look for "data:nbits:repeat".
    size_t c1 = src.find(':');
    if (c1 == std::string::npos) {
        out_data = src.c_str();
        out_nbits = 0;     // 0 → blaster will use its default table
        out_repeat = -1;   // -1 → ditto
        return true;
    }
    size_t c2 = src.find(':', c1 + 1);
    if (c2 == std::string::npos) return false;

    out_data = src.substr(0, c1).c_str();
    out_nbits  = (int)strtol(src.substr(c1 + 1, c2 - c1 - 1).c_str(), nullptr, 0);
    out_repeat = (int)strtol(src.substr(c2 + 1).c_str(),               nullptr, 0);
    return true;
}

// Runs try_discover() off the main task so the synchronous MDNS.queryService
// (~3s timeout) doesn't freeze gui_loop().
void discover_task(void*) {
    try_discover();
    s_next_recheck_ms = millis() + kRecheckIntervalMs;
    s_discover_in_flight = false;
    vTaskDelete(nullptr);
}

void start_discover_async() {
    if (s_discover_in_flight) return;
    s_discover_in_flight = true;
    // 6 KB stack covers mDNS + HTTPClient + TLS-free GET. Priority 1 (low).
    BaseType_t ok = xTaskCreate(discover_task, "blaster_disc", 6144,
                                nullptr, 1, nullptr);
    if (ok != pdPASS) {
        s_discover_in_flight = false;
        omote_log_w("[blaster] could not spawn discovery task\r\n");
    }
}

}  // namespace

void blaster_init() {
    load_prefs();
    if (!s_enabled) {
        omote_log_i("[blaster] disabled in prefs\r\n");
        set_available(false);
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        omote_log_w("[blaster] init called before WiFi up; skipping\r\n");
        set_available(false);
        return;
    }
    start_discover_async();
}

void blaster_loop() {
    if (!s_enabled) return;
    if (s_available) return;                              // only poll when down
    if (s_discover_in_flight) return;                     // already running
    if ((long)(millis() - s_next_recheck_ms) < 0) return; // not yet
    if (WiFi.status() != WL_CONNECTED) {
        s_next_recheck_ms = millis() + kRecheckIntervalMs;
        return;
    }
    start_discover_async();
}

bool blaster_isEnabled()   { return s_enabled; }
bool blaster_isAvailable() { return s_available; }

bool blaster_send(int protocol,
                  std::list<std::string> payloads,
                  std::string additionalPayload) {
    if (!s_enabled || !s_available || s_resolved.length() == 0) return false;

    String data;
    int nbits = 0, repeat = -1;
    if (!extract_data(payloads, additionalPayload, data, nbits, repeat)) {
        omote_log_w("[blaster] could not extract data; falling back\r\n");
        return false;
    }

    // Build JSON manually — payload is tiny and avoids a new dep.
    char body[160];
    int bw;
    if (nbits > 0 && repeat >= 0) {
        bw = snprintf(body, sizeof(body),
                      "{\"protocol\":%d,\"data\":\"%s\",\"nbits\":%d,\"repeat\":%d}",
                      protocol, data.c_str(), nbits, repeat);
    } else {
        bw = snprintf(body, sizeof(body),
                      "{\"protocol\":%d,\"data\":\"%s\"}",
                      protocol, data.c_str());
    }
    if (bw <= 0 || bw >= (int)sizeof(body)) {
        omote_log_w("[blaster] body buffer overflow\r\n");
        return false;
    }

    String url = "http://" + s_resolved + ":" + String(kBlasterPort) + "/send";
    HTTPClient http;
    http.setTimeout(kSendTimeoutMs);
    http.setConnectTimeout(kSendTimeoutMs);
    if (!http.begin(url)) return false;
    http.setReuse(false);  // close socket after response; see handshake() note
    http.addHeader("Content-Type", "application/json");

    int code = http.POST((uint8_t*)body, bw);
    http.end();

    if (code >= 200 && code < 300) {
        omote_log_d("[blaster] send proto=%d data=%s -> %d\r\n",
                    protocol, data.c_str(), code);
        return true;
    }

    omote_log_w("[blaster] send -> %d; marking unavailable\r\n", code);
    set_available(false);
    s_next_recheck_ms = millis() + kRecheckIntervalMs;
    return false;
}

#endif  // ENABLE_WIFI_AND_MQTT
