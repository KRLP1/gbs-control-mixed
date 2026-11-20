#ifndef PREFERENCES_MANAGER_H
#define PREFERENCES_MANAGER_H

#include <Arduino.h>
#include "../core/State.h"

// Preferences management functions
void saveUserPrefs();
void loadDefaultUserOptions();
void syncReverseRotaryEncoderIsrMirror();

// Deferred preferences write for the high-frequency value steppers: the WebUI
// "+"/"-" buttons (scanline strength, sharpness) can fire several times per
// second, and each saveUserPrefs() is a LittleFS write. LittleFS buffers the
// file in heap and the write blocks the loop for milliseconds, which is exactly
// the churn that pushed the free heap below the 10k guards of the web handlers
// (restart/reset/backup then answered "false" until the heap recovered).
// requestDeferredUserPrefsSave() only arms a timestamp; pumpDeferredUserPrefsSave()
// in loop() writes once the delay after the last change has passed, and
// flushDeferredUserPrefsSave() writes immediately (used before a restart).
void requestDeferredUserPrefsSave();
void pumpDeferredUserPrefsSave();
// writes a pending deferred save immediately (before a restart); no-op when idle
void flushDeferredUserPrefsSave();

#endif // PREFERENCES_MANAGER_H
