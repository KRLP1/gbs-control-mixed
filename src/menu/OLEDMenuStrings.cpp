#include "OLEDMenuStrings.h"
#include <Arduino.h>
#include "../core/Globals.h"

// Both text blocks are one flash string each: all texts of a language, every one
// terminated by a NUL byte, in OLED_MSG_LIST order.
#define OLED_MSG_DE(id, de, en) de "\0"
#define OLED_MSG_EN(id, de, en) en "\0"
static const char oledBlockDE[] PROGMEM = OLED_MSG_LIST(OLED_MSG_DE);
static const char oledBlockEN[] PROGMEM = OLED_MSG_LIST(OLED_MSG_EN);
#undef OLED_MSG_DE
#undef OLED_MSG_EN

#define OLED_BLOCK_MAX \
    (sizeof(oledBlockDE) > sizeof(oledBlockEN) ? sizeof(oledBlockDE) : sizeof(oledBlockEN))

static char oledTexts[OLED_BLOCK_MAX]; // RAM copy of the active language
static uint16_t oledTextOffset[MSG_COUNT]; // start of each text within oledTexts
static uint8_t oledLanguage = 0;
static bool oledStringsLoaded = false;

void oledStringsInit()
{
    if (oledStringsLoaded) {
        return; // the menu items keep pointers into oledTexts: never rewrite it
    }
    oledLanguage = (uopt != nullptr && uopt->menuLanguage == 1) ? 1 : 0;
    const char *block = oledLanguage ? oledBlockEN : oledBlockDE;
    const size_t size = oledLanguage ? sizeof(oledBlockEN) : sizeof(oledBlockDE);
    memcpy_P(oledTexts, block, size);

    // index the texts: each one starts right after the NUL of the previous one
    size_t offset = 0;
    for (uint8_t i = 0; i < MSG_COUNT; i++) {
        oledTextOffset[i] = (uint16_t)offset;
        offset += strlen(oledTexts + offset) + 1;
    }
    oledStringsLoaded = true;
}

uint8_t oledMenuLanguage()
{
    oledStringsInit(); // normally already done by initOLEDMenu()
    return oledLanguage;
}

const char *oledMsg(uint8_t id)
{
    oledStringsInit(); // normally already done by initOLEDMenu()
    if (id >= MSG_COUNT) {
        return "";
    }
    return oledTexts + oledTextOffset[id];
}
