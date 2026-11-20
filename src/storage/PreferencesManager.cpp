#include "PreferencesManager.h"
#include "../core/Globals.h"
#include "../web/SerialMirror.h"
#include <LittleFS.h>
#include "../core/options.h"
#include "../menu/OLEDMenuConfig.h"
#include "../config/Config.h"

void syncReverseRotaryEncoderIsrMirror()
{
#if USE_NEW_OLED_MENU
    g_reverseRotaryEncoderForOledMenu = uopt->reverseRotaryEncoderForOledMenu ? 1 : 0;
#endif
}

void loadDefaultUserOptions()
{
    uopt->presetPreference = Output960P;    // #1
    uopt->enableFrameTimeLock = 0; // permanently adjust frame timing to avoid glitch vertical bar. does not work on all displays!
    uopt->presetSlot = 'A';          //
    uopt->frameTimeLockMethod = 0; // compatibility with more displays
    uopt->enableAutoGain = 0;
    uopt->wantScanlines = 0;
    uopt->wantOutputComponent = 0;
    uopt->deintMode = 0;
    uopt->wantVdsLineFilter = 0;
    uopt->wantPeaking = 1;
    uopt->peakingLevel = 1; // #21 default: normal (matches old-prefs-file upgrade fallback; keeps wantPeaking/peakingLevel in sync)
    uopt->preferScalingRgbhv = 1;
    uopt->wantTap6 = 1;
    uopt->PalForce60 = 0;
    uopt->matchPresetSource = 1;             // #14
    uopt->wantStepResponse = 1;              // #15
    uopt->wantFullHeight = 1;                // #16
    uopt->enableCalibrationADC = 1;          // #17
    uopt->scanlineStrength = 0x30;           // #18
    uopt->disableExternalClockGenerator = 0; // #19
    uopt->reverseRotaryEncoderForOledMenu =
        REVERSE_ROTARY_ENCODER_FOR_OLED_MENU > 0 ? 1 : 0; // #20 default from build-time macro
    uopt->menuLanguage = 0;                               // #22 default: German
    uopt->picSharpness = 22;                             // #23 default: Normal (matches wantPeaking = 1, peakingLevel = 1)
    syncReverseRotaryEncoderIsrMirror();
}

// Deferred preferences save, see requestDeferredUserPrefsSave() below.
// time (millis) when the pending save is due; 0 = idle, nothing pending
static unsigned long deferredUserPrefsSaveAt = 0;

void saveUserPrefs()
{
    File f = LittleFS.open("/preferencesv2.txt", "w");
    if (!f) {
        SerialM.println(F("saveUserPrefs: open file failed"));
        return;
    }
    // this write carries the current values of everything, so a deferred save that is
    // still pending would only repeat it (one flash write less)
    deferredUserPrefsSaveAt = 0;
    f.write(uopt->presetPreference + '0'); // #1
    f.write(uopt->enableFrameTimeLock + '0');
    f.write(uopt->presetSlot);
    f.write(uopt->frameTimeLockMethod + '0');
    f.write(uopt->enableAutoGain + '0');
    f.write(uopt->wantScanlines + '0');
    f.write(uopt->wantOutputComponent + '0');
    f.write(uopt->deintMode + '0');
    f.write(uopt->wantVdsLineFilter + '0');
    f.write(uopt->wantPeaking + '0');
    f.write(uopt->preferScalingRgbhv + '0');
    f.write(uopt->wantTap6 + '0');
    f.write(uopt->PalForce60 + '0');
    f.write(uopt->matchPresetSource + '0');             // #14
    f.write(uopt->wantStepResponse + '0');              // #15
    f.write(uopt->wantFullHeight + '0');                // #16
    f.write(uopt->enableCalibrationADC + '0');          // #17
    f.write(uopt->scanlineStrength + '0');              // #18
    f.write(uopt->disableExternalClockGenerator + '0'); // #19
    f.write(uopt->reverseRotaryEncoderForOledMenu + '0'); // #20
    f.write(uopt->peakingLevel + '0'); // #21
    f.write(uopt->menuLanguage + '0'); // #22 (0=German, 1=English)
    f.write(uopt->picSharpness); // #23: raw byte 0..32 (32 = max)

    f.close();
    syncReverseRotaryEncoderIsrMirror();
}

// --- deferred preferences save -------------------------------------------------
// One timestamp instead of a flag + timer: 0 = idle. Written from loop() context
// only (the value steppers all run there), so no fencing is needed.

// Delay between the last stepper change and the write
static const unsigned long PREFERENCES_DEFERRED_SAVE_MS = 5000;

void requestDeferredUserPrefsSave()
{
    // (re)arm the write 5 seconds into the future; further calls while a save is
    // pending only push it back, so a burst of stepper clicks ends in exactly
    // one LittleFS write with the final values (saveUserPrefs() always writes
    // the current uopt, never a snapshot, so a later write is never stale)
    deferredUserPrefsSaveAt = millis() + PREFERENCES_DEFERRED_SAVE_MS;
    if (deferredUserPrefsSaveAt == 0) {
        deferredUserPrefsSaveAt = 1; // 0 means idle
    }
}

void pumpDeferredUserPrefsSave()
{
    if (deferredUserPrefsSaveAt == 0) {
        return; // nothing pending
    }
    // millis() wraps every ~49 days; the (now - due) form stays correct across it
    if ((long)(millis() - deferredUserPrefsSaveAt) < 0) {
        return; // not due yet
    }
    deferredUserPrefsSaveAt = 0;
    saveUserPrefs();
}

void flushDeferredUserPrefsSave()
{
    if (deferredUserPrefsSaveAt == 0) {
        return; // nothing pending
    }
    deferredUserPrefsSaveAt = 0;
    saveUserPrefs();
}
