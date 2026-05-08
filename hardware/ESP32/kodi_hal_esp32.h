#pragma once

#if (ENABLE_KODI == 1)

#include <string>

// Initialize the Kodi HAL. Safe to call before WiFi is up; the HAL is stateless
// and only does HTTP requests on demand.
void init_kodi_HAL(void);

// Fire a JSON-RPC request to the configured Kodi host. `method` is the RPC
// method name (e.g. "Input.Up"), `params_json` is a raw JSON object string for
// the params field (e.g. "{}" or "{\"action\":\"home\"}").
// Returns true if the HTTP request returned 2xx, false otherwise.
bool kodi_sendRpc_HAL(const std::string& method, const std::string& params_json);

#endif // ENABLE_KODI
