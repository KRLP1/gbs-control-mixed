#ifndef SLOT_MANAGER_H
#define SLOT_MANAGER_H

#include <Arduino.h>
#include "../core/State.h"
#include "../core/slot.h"

// Slot management functions
const uint8_t *loadPresetFromLittleFS(byte forVideoMode);
void savePresetToLittleFS();

// Preset load with scanlines: the "load custom preset" command sets the pending flag before
// applyPresets(); loadPresetFromLittleFS() consumes it and, if the preset file carries a
// scanline line (see PresetTrailer.h), applies on/off + strength and sets presetScanlinesRestored.
extern bool presetScanlinesRestorePending;
extern bool presetScanlinesRestored;

#endif // SLOT_MANAGER_H

