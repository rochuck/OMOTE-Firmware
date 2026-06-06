#pragma once

#include <cstddef>
#include <ctime>

// Status-bar wall clock. The IR blaster is the time authority: it serves a UTC
// epoch as part of the /state sync (see blasterStateSync). This module records
// that epoch together with the millis() at receipt and thereafter ticks locally
// using elapsed millis() — no NTP on the remote. The Edmonton timezone is applied
// at render time so DST is handled by the C library.

// Set the timezone (America/Edmonton). Call once from setup().
void clockTime_begin();

// Adopt a fresh UTC epoch from the blaster. Records the millis() baseline and
// marks the clock valid. Called from the main thread on each /state apply.
void clockTime_setBaseEpoch(time_t utcEpoch);

// True once a base epoch has been received at least once.
bool clockTime_valid();

// Write the current local time as "HH:MM" (24-hour) into buf. Returns false
// (leaving buf untouched) when !clockTime_valid().
bool clockTime_formatHHMM(char* buf, size_t n);
