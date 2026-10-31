#ifndef OSD_MANAGER_H_
#define OSD_MANAGER_H_
#include "../gbs/tv5725.h"
// Size of the OSD elements at 1x zoom, in OSD pixels. The programming guide
// (chapter 7) gives 131 x 19 for the icon menu and 13 + 2 + <bar length> for the
// value bar (icon + bar). On the reference display (1920x1080) the icon menu
// measured ~136 and the value bar (icon, gap, bar of length 128) ~162, so these
// two widths are the measured values. They are only used to center the elements.
#define MENU_WIDTH 136
#define BAR_WIDTH 162
#define MENU_HEIGHT 19
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))
typedef TV5725<GBS_ADDR> GBS;
struct OSDMenuConfig
{
    uint8_t barLength;
    uint8_t barActiveLength;
    bool onChange;
    bool inc;
};
typedef bool (*OSDHanlder)(OSDMenuConfig &config);
bool osdBrightness(OSDMenuConfig &config);
bool osdContrast(OSDMenuConfig &config);
bool osdScanlines(OSDMenuConfig &config);
bool osdLineFilter(OSDMenuConfig &config);
bool osdMoveX(OSDMenuConfig &config);
bool osdMoveY(OSDMenuConfig &config);
bool osdScaleY(OSDMenuConfig &config);
bool osdScaleX(OSDMenuConfig &config);
void initOSD();
enum class OSDIcon {
    BRIGHTNESS,
    CONTRAST,
    HUE,
    VOLUME,
    MOVE_Y,
    MOVE_X,
    SCALE_Y,
    SCALE_X,
};

enum OSDColor {
    OSD_BLACk = 0,
    OSD_BLUE,
    OSD_GREEN,
    OSD_CYAN,
    OSD_RED,
    OSD_MAGENTA,
    OSD_YELLOW,
    OSD_WHITE
};
enum class OSDNav {
    IDLE = 0,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    ENTER,
    MENU,
    BACK
};
typedef TV5725<GBS_ADDR> GBS;
class OSDManager
{
private:
    enum class OSDIconValue {
        BRIGHTNESS = 0x1, // 4'b0001
        CONTRAST = 0x2,   // 4'b0010
        HUE = 0x3,        // 4'b0011
        VOLUME = 0x4,      // 4'b0100
        UP_DOWN = 0x8,    // 4'b1000
        LEFT_RIGHT = 0x9, // 4'b1001
        VERTICAL = 0xA,   // 4'b1010
        HORIZONTAL = 0xB  // 4'b1011
    };

    enum class OSDState {
        OFF,
        MAIN,
        SUB,
    };
    uint8_t cursor = 0; // Initializer statt auf BSS-Nullung zu vertrauen
    uint8_t horiZoom = GBS::OSD_ZOOM_5X; // zoom register value in use, set by resetPosition()
    OSDHanlder handlers[8];
    OSDState state = OSDState::OFF;
    void rstOn()
    {
        GBS::OSD_SW_RESET::write(true);
    }
    void rstOff()
    {
        GBS::OSD_SW_RESET::write(false);
    }
    uint8_t iconToRegValue(OSDIcon icon)
    {
        OSDIconValue values[] = {
            OSDIconValue::BRIGHTNESS,
            OSDIconValue::CONTRAST,
            OSDIconValue::HUE,
            OSDIconValue::VOLUME,
            OSDIconValue::UP_DOWN,
            OSDIconValue::LEFT_RIGHT,
            OSDIconValue::VERTICAL,
            OSDIconValue::HORIZONTAL};
        return (uint8_t)values[(uint8_t)icon];
    }
    uint8_t iconToRegValue(uint8_t icon)
    {
        return iconToRegValue((OSDIcon)(icon));
    }

    // Horizontal start register value that centers an element of the given width
    // (in OSD pixels at 1x zoom) in the active display window. The register
    // counts in steps of 8 pixels (programming guide, S0_95).
    uint8_t centeredHoriStart(uint16_t widthAt1x)
    {
        int16_t x_start = GBS::VDS_DIS_HB_SP::read();
        int16_t x_stop = GBS::VDS_DIS_HB_ST::read();
        int16_t center = x_start + (x_stop - x_start) / 2;
        // 960p windowbox (preset ID 0x07): the display window is a one-sided crop
        // mask and not centered on the picture, so use the explicit center.
        if (centerOverride != 0 && GBS::GBS_PRESET_ID::read() == 0x07) {
            center = centerOverride;
        }
        int16_t x = (center - ((horiZoom + 1) * widthAt1x) / 2) >> 3;
        if (x < 0) {
            x = 0; // element wider than the window: stay at the left edge
        } else if (x > 255) {
            x = 255;
        }
        return (uint8_t)x;
    }

public:
    OSDManager()
    {
        memset(&this->handlers, 0, sizeof(this->handlers));
    }
    // Name of the icon the cursor is on (shown on the OLED while the OSD is open)
    const char *selectedName() const
    {
        static const char *const names[8] = {
            "Helligkeit", "Kontrast", "Scanlines", "Zeilenfilter",
            "Position Y", "Position X", "Skalierung Y", "Skalierung X"};
        return names[cursor & 7];
    }
    // Horizontal OSD center in display-window units; 0 = center of VDS_DIS_HB.
    // Set by applyWindowbox960(), only used while the windowbox preset is active.
    int16_t centerOverride = 0;
    uint8_t preset = 0; // wird aktuell nirgends gesetzt -> resetPosition() nimmt immer den default-Zoom
    void registerIcon(OSDIcon icon, OSDHanlder handler)
    {
        this->handlers[(uint8_t)icon] = handler;
    }
    void tick(OSDNav nav)
    {
        if (nav == OSDNav::IDLE) {
            return;
        }
        if (nav == OSDNav::MENU) {
            if (state == OSDState::OFF) {
                menuOn();
            } else {
                menuOff();
            }
            return;
        }
        if (nav == OSDNav::BACK) {
            if (state == OSDState::MAIN) {
                menuOff();
            } else if (state == OSDState::SUB) {
                menuOff();
                menuOn();
            }
            return;
        }
        if (nav == OSDNav::ENTER) {
            if (state == OSDState::MAIN) {
                enter();
            } else if (state == OSDState::SUB) {
                menuOff();
                menuOn();
            } else {
                menuOn();
            }
            return;
        }
        if (nav == OSDNav::RIGHT) {
            if (state == OSDState::MAIN) {
                next();
            } else if (state == OSDState::SUB) {
                submit(true);
            }
        }
        if (nav == OSDNav::LEFT) {
            if (state == OSDState::MAIN) {
                prev();
            } else if (state == OSDState::SUB) {
                submit(false);
            }
        }
    }
    void resetPosition()
    {
        auto y_stop = GBS::VDS_DIS_VB_ST::read();

        auto x_zoom = GBS::OSD_ZOOM_5X;
        auto y_zoom = GBS::OSD_ZOOM_4X;
        switch (this->preset) {
            case 1: // 480p
                x_zoom = GBS::OSD_ZOOM_7X;
                y_zoom = GBS::OSD_ZOOM_2X;
                break;
            case 5: // 1080p
                x_zoom = GBS::OSD_ZOOM_3X;
                y_zoom = GBS::OSD_ZOOM_8X;
                break;
            default:
                break;
        }
        // The zoom registers hold (factor - 1): value 4 means 5x (TV5725 programming
        // guide, chapter 7.4). Use the real factor for the size of the menu.
        this->horiZoom = x_zoom;
        uint8_t x = centeredHoriStart(MENU_WIDTH);
        uint8_t y = (y_stop - 64 - (y_zoom + 1) * MENU_HEIGHT) >> 3;

        GBS::OSD_MENU_DISP_STYLE::write(1);
        GBS::OSD_MENU_HORI_START::write(x);
        GBS::OSD_MENU_VER_START::write(y);
        GBS::OSD_HORIZONTAL_ZOOM::write(x_zoom);
        GBS::OSD_VERTICAL_ZOOM::write(y_zoom);
        GBS::OSD_MENU_BAR_BORD_COR::write(OSD_BLUE);
        GBS::OSD_MENU_BAR_FONT_BGCOR::write(OSD_YELLOW);
        GBS::OSD_MENU_BAR_FONT_FORCOR::write(OSD_RED);
        GBS::OSD_MENU_SEL_BGCOR::write(OSD_GREEN);
        GBS::OSD_MENU_SEL_FORCOR::write(OSD_BLACk);
    }
    void menuOn()
    {
        rstOff();
        GBS::OSD_COMMAND_FINISH::write(false);
        resetPosition();
        // GBS::OSD_YCBCR_RGB_FORMAT::write(false);
        GBS::OSD_MENU_ICON_SEL::write(iconToRegValue(cursor));
        GBS::OSD_DISP_EN::write(true);
        GBS::OSD_MENU_EN::write(true);
        GBS::OSD_COMMAND_FINISH::write(true);
        state = OSDState::MAIN;
    }

    void menuOff()
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        GBS::OSD_MENU_MOD_SEL::write(0);
        GBS::OSD_DISP_EN::write(false);
        GBS::OSD_MENU_EN::write(false);
        state = OSDState::OFF;
        GBS::OSD_COMMAND_FINISH::write(true);
        rstOn();
    }

    void updateCursor()
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        GBS::OSD_MENU_ICON_SEL::write(iconToRegValue(cursor));
        GBS::OSD_COMMAND_FINISH::write(true);
    }
    void next()
    {
        auto old = cursor;
        do {
            cursor = (cursor + 1) % 8;
        } while (handlers[cursor] == nullptr && cursor != old);
        updateCursor();
    }
    void prev()
    {
        auto old = cursor;
        do {
            cursor = cursor == 0 ? 7 : (cursor - 1) % 8;
        } while (handlers[cursor] == nullptr && cursor != old);
        updateCursor();
    }
    void enter()
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        OSDMenuConfig config;
        config.onChange = false;
        bool shouldEnter = (*handlers[cursor])(config);
        if (shouldEnter) {
            state = OSDState::SUB;
            uint8_t active = 128.0 / config.barLength * config.barActiveLength;
            GBS::OSD_MENU_MOD_SEL::write(iconToRegValue(cursor));
            // the value bar is wider than the icon menu: center it on its own
            GBS::OSD_MENU_HORI_START::write(centeredHoriStart(BAR_WIDTH));
            GBS::OSD_BAR_LENGTH::write(128);
            GBS::OSD_BAR_FOREGROUND_VALUE::write(active);
            GBS::OSD_MENU_BAR_FONT_FORCOR::write(OSD_WHITE);
            GBS::OSD_MENU_BAR_FONT_BGCOR::write(OSD_BLACk);
        }
        GBS::OSD_COMMAND_FINISH::write(true);
    }
    void submit(bool inc)
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        OSDMenuConfig config;
        config.inc = inc;
        config.onChange = true;
        (*handlers[cursor])(config);
        GBS::OSD_MENU_MOD_SEL::write(iconToRegValue(cursor));
        uint8_t active = 128 / config.barLength * config.barActiveLength;
        GBS::OSD_BAR_LENGTH::write(128);
        GBS::OSD_BAR_FOREGROUND_VALUE::write(active);
        GBS::OSD_COMMAND_FINISH::write(true);
    }
};
#endif