#pragma once

#if (ENABLE_OTA == 1)
typedef void (*tOtaStartCallback)(void);
typedef void (*tOtaProgressCallback)(int pct);  // 0-100, or -1 for error

void set_ota_start_cb_HAL(tOtaStartCallback cb);
void set_ota_progress_cb_HAL(tOtaProgressCallback cb);
void init_ota_HAL(void);
void ota_loop_HAL(void);
#endif
