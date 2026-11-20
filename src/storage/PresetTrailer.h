#ifndef PRESET_TRAILER_H
#define PRESET_TRAILER_H

#include <stdint.h>
#include <stddef.h>

// A custom preset file is a text dump "N,\r\n" x 432 closed by "};". Behind the "};"
// savePresetToLittleFS() appends one more line with the scanline settings:
//
//     S,<on 0|1>,<strength decimal 0..96>
//
// Older firmware stops reading at the '}' and never sees it; files written by older
// firmware simply have no such line (valid == false) and the settings then come from
// the slot metadata instead. The segment 2 registers (deinterlacer / scanlines) are not
// part of the register dump, so this line is how the preset carries the scanlines.
struct PresetScanlineTrailer {
    bool valid;
    bool on;
    uint8_t strength; // 0x00 = strongest ... 0x60 = weakest
};

// Parses the text that follows the '}' of the preset file (NUL terminated).
// Anything that is not exactly "S,0|1,<0..96>" yields valid == false.
static inline PresetScanlineTrailer parsePresetScanlineTrailer(const char *tail)
{
    PresetScanlineTrailer r = {false, false, 0};
    if (tail == NULL) {
        return r;
    }
    while (*tail != '\0' && *tail != 'S') { // skip ';' and line breaks after the '}'
        if (*tail != ';' && *tail != '\r' && *tail != '\n' && *tail != ' ') {
            return r; // unexpected garbage: do not guess
        }
        tail++;
    }
    if (*tail != 'S') {
        return r;
    }
    tail++;
    if (*tail++ != ',') {
        return r;
    }
    if (*tail != '0' && *tail != '1') {
        return r;
    }
    const bool on = (*tail == '1');
    tail++;
    if (*tail++ != ',') {
        return r;
    }
    if (*tail < '0' || *tail > '9') {
        return r;
    }
    uint16_t value = 0;
    while (*tail >= '0' && *tail <= '9') {
        value = (uint16_t)(value * 10 + (uint16_t)(*tail - '0'));
        if (value > 0x60) {
            return r; // out of range
        }
        tail++;
    }
    if (*tail != '\0' && *tail != '\r' && *tail != '\n' && *tail != ' ') {
        return r; // trailing junk inside the number
    }
    r.valid = true;
    r.on = on;
    r.strength = (uint8_t)value;
    return r;
}

#endif // PRESET_TRAILER_H
