#include "OSDManager.h"
#include "../core/options.h"
#include "../video/Scanlines.h"
extern userOptions *uopt;
extern void saveUserPrefs();
extern void shiftHorizontalRight();
extern void shiftHorizontalLeft();
extern void shiftVerticalDownIF();
extern void shiftVerticalUpIF();
extern void scaleVertical(uint16_t, bool);
extern void scaleHorizontal(uint16_t, bool);
extern OSDManager osdManager;

bool osdBrightness(OSDMenuConfig &config)
{
    const int16_t STEP = 8;
    int8_t cur =  GBS::VDS_Y_OFST::read();
    if (config.onChange) {
        if (config.inc) {
            cur = MIN(cur + STEP, 127);
        } else {
            cur = MAX(-128, cur - STEP);
        }
        GBS::VDS_Y_OFST::write(cur);
    } 
    config.barLength = 256 / STEP;
    config.barActiveLength = (cur + 128 + 1) / STEP;
    return true;
}
bool osdScanlines(OSDMenuConfig &config)
{
    // Same model and the same steps as the WebUI stepper:
    //   strength 0x00 = strongest (biggest) scanlines ... 0x60 = weakest (finest),
    //   fine 0x04 steps, "+" = stronger (value down), "-" = weaker (value up).
    // "Off" is its own position behind the weak end, independent of the value:
    //   "-" at 0x60 switches the scanlines off (empty bar), "+" while off switches
    //   them on at 0x60 and goes on towards stronger. At the strong end (0x00) the
    //   value just stays. 0x00 is an ordinary strength, never "off".
    // All changes go through applyScanlineState(), the same single function the
    // WebUI value command 'y' and the preset restore use, so registers, the
    // on/off state and range clamping are identical everywhere. persist = false:
    // turning the encoder through several steps must not cost one LittleFS write
    // per step - the preferences are saved once, shortly after the last step.
    const uint8_t STEP = 0x04;
    const uint8_t WEAKEST = SCANLINES_MAX_STRENGTH_VALUE; // 0x60
    const bool on = uopt->wantScanlines != 0;
    uint8_t current = uopt->scanlineStrength;
    if (current > WEAKEST) {
        current = WEAKEST; // safety net, same correction as applyScanlineState
    }
    if (config.onChange) {
        if (config.inc) {
            if (!on) {
                applyScanlineState(true, WEAKEST, false); // off -> weakest visible lines
            } else if (current > 0x00) {
                uint8_t next = (current >= STEP) ? (uint8_t)(current - STEP) : 0x00;
                applyScanlineState(true, next, false);
            } // else: already strongest, stay
        } else if (on) {
            if (current >= WEAKEST) {
                applyScanlineState(false, current, false); // weakest -> off (value kept)
            } else {
                uint8_t next = (uint8_t)(current + STEP);
                if (next > WEAKEST) {
                    next = WEAKEST;
                }
                applyScanlineState(true, next, false);
            }
        } // else: already off, stay
    }
    // Bar like the WebUI bar: off = empty, 0x60 = shortest visible bar, 0x00 = full.
    config.barLength = 128;
    if (uopt->wantScanlines) {
        uint8_t shown = uopt->scanlineStrength;
        if (shown > WEAKEST) {
            shown = WEAKEST;
        }
        config.barActiveLength = 8 + ((uint16_t)(WEAKEST - shown) * 120) / WEAKEST;
    } else {
        config.barActiveLength = 0;
    }
    return true;
}
bool osdLineFilter(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (config.onChange) {
        uopt->wantVdsLineFilter = config.inc ? 1 : 0;
        GBS::VDS_D_RAM_BYPS::write(!uopt->wantVdsLineFilter);
        saveUserPrefs();
    }
    if (uopt->wantVdsLineFilter == 1) {
        config.barActiveLength = 2;
    } else {
        config.barActiveLength = 0;
    }
    return true;
}
bool osdMoveX(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (!config.onChange) {
        config.barActiveLength = 1;
        return true;
    }
    if (config.inc) {
        shiftHorizontalRight();
        config.barActiveLength = 2;
    } else {
        shiftHorizontalLeft();
        config.barActiveLength = 0;
    }
    return false;
}

bool osdMoveY(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (!config.onChange) {
        config.barActiveLength = 1;
        return true;
    }
    if (config.inc) {
        shiftVerticalDownIF();
        config.barActiveLength = 2;
    } else {
        shiftVerticalUpIF();
        config.barActiveLength = 0;
    }
    return false;
}

bool osdScaleY(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (!config.onChange) {
        config.barActiveLength = 1;
        return true;
    }
    if (config.inc) {
        scaleVertical(2, true);
        config.barActiveLength = 2;
    } else {
        scaleVertical(2, false);
        config.barActiveLength = 0;
    }
    return false;
}

bool osdScaleX(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (!config.onChange) {
        config.barActiveLength = 1;
        return true;
    }
    if (config.inc) {
        scaleHorizontal(2, true);
        config.barActiveLength = 2;
    } else {
        scaleHorizontal(2, false);
        config.barActiveLength = 0;
    }
    return false;
}
bool osdContrast(OSDMenuConfig &config)
{
    const uint8_t STEP = 8;
    int16_t cur = GBS::ADC_RGCTRL::read();
    if (config.onChange) {
        if (uopt->enableAutoGain == 1) {
            uopt->enableAutoGain = 0;
            saveUserPrefs();
        } else {
            uopt->enableAutoGain = 0;
        }
        if (config.inc) {
            cur = MAX(0, cur - STEP);
        } else {
            cur = MIN(cur + STEP, 255);
        }
        GBS::ADC_RGCTRL::write(cur);
        GBS::ADC_GGCTRL::write(cur);
        GBS::ADC_BGCTRL::write(cur);
    }
    config.barLength = 256 / STEP;
    config.barActiveLength = (256 - cur) / STEP;
    return true;
}

void initOSD()
{
    osdManager.registerIcon(OSDIcon::BRIGHTNESS, osdBrightness);
    osdManager.registerIcon(OSDIcon::CONTRAST, osdContrast);

    osdManager.registerIcon(OSDIcon::HUE, osdScanlines);
    osdManager.registerIcon(OSDIcon::VOLUME, osdLineFilter);
    osdManager.registerIcon(OSDIcon::MOVE_Y, osdMoveY);
    osdManager.registerIcon(OSDIcon::MOVE_X, osdMoveX);
    osdManager.registerIcon(OSDIcon::SCALE_Y, osdScaleY);
    osdManager.registerIcon(OSDIcon::SCALE_X, osdScaleX);
}