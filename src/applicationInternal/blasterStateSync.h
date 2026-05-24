#pragma once

// State-of-truth sync with the OMOTE-Blaster. The blaster keeps an in-RAM
// snapshot of (scene, guiName, guiList, lastIndex) so a second remote picking
// up the same blaster lands on the same screen.
//
//  - Every local state change → POST /state to the blaster (throttled).
//  - First time the blaster becomes available after boot → GET /state, and
//    (if the user hasn't touched the device yet) reconcile the GUI to match.
//  - Subsequent reconnects only re-POST current state — never yank the user
//    away from their in-progress screen.

#if (ENABLE_WIFI_AND_MQTT == 1) && defined(ARDUINO)

// Called from each gui_memoryOptimizer_set* setter. Marks the snapshot dirty;
// the actual POST happens from blasterStateSync_loop() on the main thread
// after a short coalescing delay.
void blasterStateSync_postCurrent();

// Drive the sync state machine. Call from the main loop, next to blaster_loop().
// Cheap when there's no work — does a synchronous POST only when dirty and the
// blaster is available; applies a pending fetch result on the main thread.
void blasterStateSync_loop();

// Called by blasterClient when the blaster transitions to available. The first
// such call after boot triggers a /state GET; later calls just re-POST.
void blasterStateSync_onBlasterAvailable();

#else

inline void blasterStateSync_postCurrent() {}
inline void blasterStateSync_loop() {}
inline void blasterStateSync_onBlasterAvailable() {}

#endif
