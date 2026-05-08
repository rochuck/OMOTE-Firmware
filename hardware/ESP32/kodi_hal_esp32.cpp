/*
 * Kodi JSON-RPC HAL for ESP32-S3
 *
 * Sends fire-and-forget JSON-RPC requests to a Kodi instance over HTTP.
 * Configure host/port/credentials in secrets_override.h
 * (KODI_HOST / KODI_PORT / KODI_USER / KODI_PASS).
 *
 * Kodi must have "Allow remote control via HTTP" enabled in
 * Settings -> Services -> Control.
 *
 * Reference: https://kodi.wiki/view/JSON-RPC_API
 */

#include "kodi_hal_esp32.h"

#if (ENABLE_KODI == 1)

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "applicationInternal/omote_log.h"
#include "secrets.h"

static int  s_request_id = 1;
static bool s_inited     = false;

void
init_kodi_HAL(void) {
    s_inited = true;
    omote_log_i("kodi: configured for http://%s:%d/jsonrpc\r\n", KODI_HOST, (int) KODI_PORT);
}

bool
kodi_sendRpc_HAL(const std::string& method, const std::string& params_json) {
    if (!s_inited) {
        omote_log_e("kodi: not initialised\r\n");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        omote_log_e("kodi: WiFi not connected, dropping '%s'\r\n", method.c_str());
        return false;
    }

    String url = String("http://") + KODI_HOST + ":" + String((int) KODI_PORT) + "/jsonrpc";

    String body = String("{\"jsonrpc\":\"2.0\",\"method\":\"") + method.c_str() +
                  "\",\"params\":" + (params_json.empty() ? "{}" : params_json.c_str()) +
                  ",\"id\":" + String(s_request_id++) + "}";

    HTTPClient http;
    http.setConnectTimeout(2000);
    http.setTimeout(2000);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Optional Basic auth
    const char* user = KODI_USER;
    const char* pass = KODI_PASS;
    if (user && user[0] != '\0') { http.setAuthorization(user, pass ? pass : ""); }

    omote_log_d("kodi: POST %s body=%s\r\n", url.c_str(), body.c_str());
    int code = http.POST((uint8_t*) body.c_str(), body.length());
    bool ok  = (code >= 200 && code < 300);
    if (!ok) {
        omote_log_e("kodi: HTTP %d for '%s'\r\n", code, method.c_str());
    } else {
        omote_log_v("kodi: HTTP %d for '%s'\r\n", code, method.c_str());
    }
    http.end();
    return ok;
}

#endif // ENABLE_KODI
