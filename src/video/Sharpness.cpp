#include "Sharpness.h"
#include "../web/SerialMirror.h"
#include "../storage/PreferencesManager.h"
#include "../gbs/tv5725.h"

typedef TV5725<GBS_ADDR> GBS;

// Last non-zero sharpness: the peaking on/off toggle restores it. RAM only,
// starts at Normal. Updated by syncPeakingFromSharpness().
static uint8_t lastOnSharpness = 22;

static uint8_t levelFromSharpness(uint8_t value)
{
    if (value == 0) {
        return 0; // off
    }
    if (value <= 22) {
        return 1; // normal
    }
    if (value <= 31) {
        return 2; // strong
    }
    return 3; // max
}

uint8_t sharpnessFromPeakingLevel(uint8_t level)
{
    static const uint8_t levelToSharpness[4] = {0, 22, 31, SHARPNESS_MAX_STEP};
    return levelToSharpness[level & 3];
}

void syncPeakingFromSharpness()
{
    const uint8_t value = uopt->picSharpness;
    if (value > 0) {
        lastOnSharpness = value;
    }
    uopt->wantPeaking = (value > 0) ? 1 : 0;
    uopt->peakingLevel = levelFromSharpness(value);
}

void adoptWantPeaking()
{
    if (uopt->wantPeaking == 0) {
        uopt->picSharpness = 0; // lastOnSharpness keeps the previous value
    } else if (uopt->picSharpness == 0) {
        uopt->picSharpness = lastOnSharpness;
    }
    syncPeakingFromSharpness();
}

void applyPictureSharpness()
{
    const uint8_t value = uopt->picSharpness;
    if (value == 0) {
        GBS::VDS_PK_Y_H_BYPS::write(1);
        return;
    }

    // 0x05/0x15 are the 1080p presets: the stock LH gain there is 0x0A
    const bool preset1080 = (rto->presetID == 0x05 || rto->presetID == 0x15);
    uint8_t lb;
    uint8_t lh;
    uint8_t step;
    if (value >= SHARPNESS_MAX_STEP) {
        // fork maximum, full 6-bit register width
        lb = 0x3f;
        lh = 0x3f;
        step = 8;
    } else {
        // brisma formula (picture/picture-color.cpp pictureSharpness())
        const int base = preset1080 ? 0x0A : 0x18;
        int h = (value * base + 11) / 22;
        if (h > 31) {
            h = 31;
        }
        lb = value;
        lh = (uint8_t)h;
        step = 1;
    }

    GBS::VDS_PK_Y_H_BYPS::write(0);
    GBS::VDS_PK_LB_GAIN::write(lb);
    GBS::VDS_PK_LH_GAIN::write(lh);
    if (!preset1080) {
        GBS::VDS_STEP_GAIN::write(step);
    }
}

void setPictureSharpness(uint8_t value, bool announce)
{
    if (value > SHARPNESS_MAX_STEP) {
        value = SHARPNESS_MAX_STEP;
    }
    uopt->picSharpness = value;
    syncPeakingFromSharpness();
    // Deferred save: the sharpness stepper can fire several times per second;
    // an immediate LittleFS write per step is what pushed the heap below the
    // 10k guards of the web handlers (restart/reset/backup went mute). The
    // loop() pump writes the final value once, after the last change settles.
    requestDeferredUserPrefsSave();
    applyPictureSharpness();

    if (announce) {
        SerialM.print(F("sharpness: "));
        if (value == SHARPNESS_MAX_STEP) {
            SerialM.println(F("max"));
        } else {
            SerialM.println(value);
        }
    }
}

void cyclePeakingLevel()
{
    const uint8_t next = (uopt->peakingLevel + 1) & 3;
    SerialM.print(F("peaking level: "));
    SerialM.println(next);
    setPictureSharpness(sharpnessFromPeakingLevel(next), false);
}

void togglePeaking()
{
    SerialM.print(F("peaking "));
    if (uopt->picSharpness == 0) {
        setPictureSharpness(lastOnSharpness, false);
        SerialM.println("on");
    } else {
        setPictureSharpness(0, false);
        SerialM.println("off");
    }
}
