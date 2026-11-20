#ifndef SCANLINES_H
#define SCANLINES_H

#include <Arduino.h>
#include "../core/State.h"

// Scanline strength model (shared by WebUI, OSD, presets and serial/web commands):
//   0x00 = strongest (biggest) scanlines ... 0x60 = weakest (finest) scanlines.
// On/off is independent of the value (uopt->wantScanlines); every value in
// 0x00..0x60 is a valid "on" strength. 0x7f is only used on the wire of the
// value command 'y' to say "switch scanlines off" (it is also the register
// value disableScanlines() parks MADPT_Y_MI_OFFSET at).
#define SCANLINES_MAX_STRENGTH_VALUE 0x60 // weakest allowed strength value
#define SCANLINES_OFF_VALUE 0x7f          // value command 'y': scanlines off

// Scanlines functions
void enableScanlines();
void disableScanlines();

// Set only the scanline strength (clamped to 0x00..0x60; larger values become
// 0x60) and keep the on/off state. Writes the mixing registers while the
// scanlines are active, persists and prints.
void setScanlineStrength(uint8_t strength);

// Set on/off and strength in one go (WebUI value command, OSD, preset restore).
// on == false: wantScanlines = 0 and the hardware is switched off right away.
// on == true: wantScanlines = 1; the SyncWatcher switches the hardware on when
// the mode allows it (like the serial toggle '7'); if the scanlines are already
// active the new strength is written to the registers immediately.
// persist: also save the user preferences (false for bulk restores that save
// afterwards on their own).
void applyScanlineState(bool on, uint8_t strength, bool persist);

#endif // SCANLINES_H
