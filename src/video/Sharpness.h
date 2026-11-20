#ifndef SHARPNESS_H
#define SHARPNESS_H

#include <Arduino.h>
#include "../core/Globals.h"

// Picture sharpness (TV5725 peaking), ported from brisma/gbsc-pro 3.0.0-beta2
// (picture/picture-color.cpp pictureSharpness()) into the fork's module layout.
//
// uopt->picSharpness is the single source of truth for the peaking state:
//   0        peaking off (VDS_PK_Y_H_BYPS = 1)
//   1..31    brisma scale: LB = v, LH = clamp((v * base + 11) / 22, 0, 31)
//            with base 0x18, or 0x0A on the 1080p presets (0x05/0x15)
//   32       "Max": the fork's old peaking level 3, LB/LH 0x3f (full 6-bit
//            register width), which the 0..31 scale cannot express. Reachable
//            through the WebUI (+ at 31, the "Stark" preset), the value command
//            and the peaking level command '8'.
// uopt->wantPeaking and uopt->peakingLevel are derived from it (see
// syncPeakingFromSharpness()), so the status ping, the slots and the old
// WebUI toggle keep working without a second state to keep in step.
//
// Peaking level command '8': 0 = off, 1 = normal (22), 2 = strong (31), 3 = max (32).
// (The WebUI no longer has this button; its three presets Weich/Normal/Stark are
// 0/22/32.)
static const uint8_t SHARPNESS_MAX_STEP = 32;

// Level -> sharpness value for the peaking level button and for old preference files.
uint8_t sharpnessFromPeakingLevel(uint8_t level);

// Derive wantPeaking / peakingLevel from picSharpness. Registers are not touched.
void syncPeakingFromSharpness();

// wantPeaking was changed from outside (slot restore): fold it into picSharpness.
// Off -> 0; on while 0 -> the last non-zero value. Registers are not touched.
void adoptWantPeaking();

// Write the registers for the current picSharpness.
void applyPictureSharpness();

// Set an explicit sharpness (0..32), sync the derived fields, apply, and arm the
// deferred preferences save (see PreferencesManager.h: written once, shortly after
// the last change).
// announce = false: no serial line (callers that print their own).
void setPictureSharpness(uint8_t value, bool announce = true);

// Peaking level cycle (user command '8'): off -> normal -> strong -> max -> off.
void cyclePeakingLevel();

// Peaking on/off toggle (serial command 'f'): off remembers the last value, on restores it.
void togglePeaking();

#endif // SHARPNESS_H
