#pragma once

#if (ENABLE_KODI == 1)

#include <string>

void init_kodi_HAL(void);
bool kodi_sendRpc_HAL(const std::string& method, const std::string& params_json);

#endif // ENABLE_KODI
