#include "Scanlines.h"
#include "../core/Globals.h"
#include "../web/SerialMirror.h"
#include "../storage/PreferencesManager.h"
#include "../gbs/tv5725.h"

void setScanlineStrength(uint8_t strength)
{
    // Value only: the on/off state (wantScanlines) is not touched here.
    // Deferred save: this is the "+"/"-" stepper raster, several calls per
    // second are normal and every one of them used to cost a LittleFS write.
    applyScanlineState(uopt->wantScanlines != 0, strength, false);
}

void applyScanlineState(bool on, uint8_t strength, bool persist)
{
    typedef TV5725<GBS_ADDR> GBS;
    if (strength > SCANLINES_MAX_STRENGTH_VALUE) {
        strength = SCANLINES_MAX_STRENGTH_VALUE; // out of range: weakest visible lines
    }
    uopt->scanlineStrength = strength;
    uopt->wantScanlines = on ? 1 : 0;
    if (!on) {
        // wantScanlines is only ever cleared together with this call: the SyncWatcher
        // switches scanlines on, but only switches them off in bob mode.
        disableScanlines();
    } else if (rto->scanlinesEnabled) {
        // already active: the new strength reaches the hardware right away. While they
        // are off the registers stay untouched (disableScanlines() parks
        // MADPT_Y_MI_OFFSET at 0x7f); enableScanlines() applies uopt->scanlineStrength.
        GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength);
        GBS::MADPT_UV_MI_OFFSET::write(uopt->scanlineStrength);
    }
    if (persist) {
        saveUserPrefs();
    } else {
        // persist = false: the high-frequency callers (WebUI stepper, OSD encoder) and the
        // bulk restores (preset load, slot restore). A deferred save is armed: it writes
        // once, 5 s after the last change. A caller that saves on his own right away
        // (e.g. the preset load) makes it redundant: saveUserPrefs() clears the request.
        requestDeferredUserPrefsSave();
    }
    SerialM.print(F("Scanlines: "));
    SerialM.print(on ? F("on") : F("off"));
    SerialM.print(F(", strength 0x"));
    SerialM.println(uopt->scanlineStrength, HEX);
}

void enableScanlines()
{
    typedef TV5725<GBS_ADDR> GBS;
    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 0) {
        // following lines set up UV deinterlacer (on top of normal Y)
        GBS::MADPT_UVDLY_PD_SP::write(0);     // 2_39 0..3
        GBS::MADPT_UVDLY_PD_ST::write(0);     // 2_39 4..7
        GBS::MADPT_EN_UV_DEINT::write(1);     // 2_3a 0
        GBS::MADPT_UV_MI_DET_BYPS::write(1);  // 2_3a 7 enables 2_3b adjust
        GBS::MADPT_UV_MI_OFFSET::write(uopt->scanlineStrength); // 2_3b offset (mixing factor here)
        GBS::MADPT_MO_ADP_UV_EN::write(1);    // 2_16 5 (try to do this some other way?)

        GBS::DIAG_BOB_PLDY_RAM_BYPS::write(0); // 2_00 7 enabled, looks better
        GBS::MADPT_PD_RAM_BYPS::write(0);      // 2_24 2
        GBS::RFF_YUV_DEINTERLACE::write(1);    // scanline fix 2
        GBS::MADPT_Y_MI_DET_BYPS::write(1);    // make sure, so that mixing works
        GBS::VDS_WLEV_GAIN::write(0x08);                       // 3_58
        GBS::VDS_W_LEV_BYPS::write(0);                         // brightness
        GBS::MADPT_VIIR_COEF::write(0x08);                     // 2_27 VIIR filter strength
        GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength); // 2_0b offset (mixing factor here)
        GBS::MADPT_VIIR_BYPS::write(0);                        // 2_26 6 VIIR line fifo // 1 = off
        GBS::RFF_LINE_FLIP::write(1);                          // clears potential garbage in rff buffer

        GBS::MAPDT_VT_SEL_PRGV::write(0);
        GBS::GBS_OPTION_SCANLINES_ENABLED::write(1);
    }
    rto->scanlinesEnabled = 1;
}

void disableScanlines()
{
    typedef TV5725<GBS_ADDR> GBS;
    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
        GBS::MAPDT_VT_SEL_PRGV::write(1);

        // following lines set up UV deinterlacer (on top of normal Y)
        GBS::MADPT_UVDLY_PD_SP::write(4); // 2_39 0..3
        GBS::MADPT_UVDLY_PD_ST::write(4); // 2_39 4..7
        GBS::MADPT_EN_UV_DEINT::write(0);     // 2_3a 0
        GBS::MADPT_UV_MI_DET_BYPS::write(0);  // 2_3a 7 enables 2_3b adjust
        GBS::MADPT_UV_MI_OFFSET::write(4);    // 2_3b
        GBS::MADPT_MO_ADP_UV_EN::write(0);    // 2_16 5

        GBS::DIAG_BOB_PLDY_RAM_BYPS::write(1); // 2_00 7
        GBS::VDS_W_LEV_BYPS::write(1);         // brightness
        GBS::MADPT_Y_MI_OFFSET::write(0x7f); // 2_0b all bits set disables mixing (7 bit register)
        GBS::MADPT_VIIR_BYPS::write(1);      // 2_26 6 disable VIIR
        GBS::MADPT_PD_RAM_BYPS::write(1);
        GBS::RFF_LINE_FLIP::write(0); // back to default

        GBS::GBS_OPTION_SCANLINES_ENABLED::write(0);
    }
    rto->scanlinesEnabled = 0;
}
