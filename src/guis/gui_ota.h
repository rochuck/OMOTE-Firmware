#pragma once

#if (ENABLE_OTA == 1)
// Called when the upload starts — creates the overlay
void ota_gui_start(void);
// Called with progress 0–100, or -1 on error
void ota_gui_set_progress(int pct);
#endif
