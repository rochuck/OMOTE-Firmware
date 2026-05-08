#include "kodi_hal_windows_linux.h"

#if (ENABLE_KODI == 1)

#include <cstdio>

// Desktop simulator stub: log the RPC instead of sending it.

void init_kodi_HAL(void) {}

bool kodi_sendRpc_HAL(const std::string& method, const std::string& params_json) {
    std::printf("kodi (sim): %s %s\n", method.c_str(), params_json.c_str());
    return true;
}

#endif // ENABLE_KODI
