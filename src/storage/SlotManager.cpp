#include "SlotManager.h"
#include "../core/Globals.h"
#include "../web/SerialMirror.h"
#include "../gbs/GBSRegister.h"
#include "../video/Scanlines.h"
#include "../video/framesync.h"
#include "../../presets/ntsc_240p.h"
#include "../../presets/pal_240p.h"
#include "../gbs/tv5725.h"
#include <LittleFS.h>
#include <string.h>

void StrClear(char *str, uint16_t length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

const uint8_t *loadPresetFromLittleFS(byte forVideoMode)
{
    static uint8_t preset[432];
    String s = "";
    Ascii8 slot = 0;
    File f;

    f = LittleFS.open("/preferencesv2.txt", "r");
    if (f) {
        SerialM.println(F("preferencesv2.txt opened"));
        uint8_t result[3];
        result[0] = f.read(); // todo: move file cursor manually
        result[1] = f.read();
        result[2] = f.read();

        f.close();
        slot = result[2];
    } else {
        // file not found, we don't know what preset to load
        SerialM.println(F("please select a preset slot first!")); // say "slot" here to make people save usersettings
        if (forVideoMode == 2 || forVideoMode == 4)
            return pal_240p;
        else
            return ntsc_240p;
    }

    SerialM.print(F("loading from preset slot "));
    SerialM.print((char)slot);
    SerialM.print(": ");

    if (forVideoMode == 1) {
        f = LittleFS.open("/preset_ntsc." + String((char)slot), "r");
    } else if (forVideoMode == 2) {
        f = LittleFS.open("/preset_pal." + String((char)slot), "r");
    } else if (forVideoMode == 3) {
        f = LittleFS.open("/preset_ntsc_480p." + String((char)slot), "r");
    } else if (forVideoMode == 4) {
        f = LittleFS.open("/preset_pal_576p." + String((char)slot), "r");
    } else if (forVideoMode == 5) {
        f = LittleFS.open("/preset_ntsc_720p." + String((char)slot), "r");
    } else if (forVideoMode == 6) {
        f = LittleFS.open("/preset_ntsc_1080p." + String((char)slot), "r");
    } else if (forVideoMode == 8) {
        f = LittleFS.open("/preset_medium_res." + String((char)slot), "r");
    } else if (forVideoMode == 14) {
        f = LittleFS.open("/preset_vga_upscale." + String((char)slot), "r");
    } else if (forVideoMode == 0) {
        f = LittleFS.open("/preset_unknown." + String((char)slot), "r");
    }

    if (!f) {
        SerialM.println(F("no preset file for this slot and source"));
        if (forVideoMode == 2 || forVideoMode == 4)
            return pal_240p;
        else
            return ntsc_240p;
    } else {
        SerialM.println(f.name());
        s = f.readStringUntil('}');
        f.close();
    }

    // savePresetToLittleFS() writes exactly sizeof(preset) values, each as "N,\r\n", so splitting
    // on ',' also yields a trailing "\r\n" token after the final comma - one more than there is
    // room for. Bounding this matters beyond that off by one: a file written by another firmware
    // revision, or a damaged one, would otherwise run off the end of this static and into
    // whatever the linker placed after it.
    char *tmp;
    uint16_t i = 0;
    tmp = strtok(&s[0], ",");
    while (tmp && i < sizeof(preset)) {
        preset[i++] = (uint8_t)atoi(tmp);
        tmp = strtok(NULL, ",");
        yield(); // wifi stack
    }

    if (i != sizeof(preset)) {
        SerialM.print(F("warning: preset file holds "));
        SerialM.print(i);
        SerialM.print(F(" of "));
        SerialM.print(sizeof(preset));
        SerialM.println(F(" values, may be damaged"));
    }

    return preset;
}

void savePresetToLittleFS()
{
    typedef TV5725<GBS_ADDR> GBS;
    uint8_t readout = 0;
    File f;
    Ascii8 slot = 0;

    // first figure out if the user has set a preferenced slot
    f = LittleFS.open("/preferencesv2.txt", "r");
    if (f) {
        uint8_t result[3];
        result[0] = f.read(); // todo: move file cursor manually
        result[1] = f.read();
        result[2] = f.read();

        f.close();
        slot = result[2]; // got the slot to save to now
    } else {
        // file not found, we don't know where to save this preset
        SerialM.println(F("please select a preset slot first!"));
        return;
    }

    SerialM.print(F("saving to preset slot "));
    SerialM.println(String((char)slot));

    // Scaling RGB/HV shuffles videoStandardInput between 3, 14 and 15 while the sync watcher
    // works: writeProgramArrayNew() leaves it at 3, the scaling block restores 14, and loading
    // routes through 15. So it cannot name the file on its own. applyPresets() always asks the
    // load side for mode 14 in this configuration, so the save has to agree - otherwise it writes
    // preset_ntsc_480p, or (at 15) matches no branch at all and saves nothing, while reporting
    // success. The load then finds no file and silently falls back to ntsc_240p.
    uint8_t forVideoMode = rto->videoStandardInput;
    if (GBS::GBS_OPTION_SCALING_RGBHV::read() == 1 || rto->isValidForScalingRGBHV) {
        forVideoMode = 14;
    }

    if (forVideoMode == 1) {
        f = LittleFS.open("/preset_ntsc." + String((char)slot), "w");
    } else if (forVideoMode == 2) {
        f = LittleFS.open("/preset_pal." + String((char)slot), "w");
    } else if (forVideoMode == 3) {
        f = LittleFS.open("/preset_ntsc_480p." + String((char)slot), "w");
    } else if (forVideoMode == 4) {
        f = LittleFS.open("/preset_pal_576p." + String((char)slot), "w");
    } else if (forVideoMode == 5) {
        f = LittleFS.open("/preset_ntsc_720p." + String((char)slot), "w");
    } else if (forVideoMode == 6) {
        f = LittleFS.open("/preset_ntsc_1080p." + String((char)slot), "w");
    } else if (forVideoMode == 8) {
        f = LittleFS.open("/preset_medium_res." + String((char)slot), "w");
    } else if (forVideoMode == 14) {
        f = LittleFS.open("/preset_vga_upscale." + String((char)slot), "w");
    } else if (forVideoMode == 0) {
        f = LittleFS.open("/preset_unknown." + String((char)slot), "w");
    } else {
        // 7, 9, 13 and 15 have no preset file of their own
        SerialM.print(F("no preset file defined for video mode "));
        SerialM.println(forVideoMode);
        return;
    }

    if (!f) {
        SerialM.println(F("open save file failed!"));
    } else {
        SerialM.println(F("open save file ok"));

        GBS::GBS_PRESET_CUSTOM::write(1); // use one reserved bit to mark this as a custom preset
        // don't store scanlines
        if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
            disableScanlines();
        }

        if (!rto->extClockGenDetected) {
            if (uopt->enableFrameTimeLock && FrameSync::getSyncLastCorrection() != 0) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
        }

        for (int i = 0; i <= 5; i++) {
            writeOneByte(0xF0, i);
            switch (i) {
                case 0:
                    for (int x = 0x40; x <= 0x5F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    for (int x = 0x90; x <= 0x9F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 1:
                    for (int x = 0x0; x <= 0x2F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 2:
                    // not needed anymore
                    break;
                case 3:
                    for (int x = 0x0; x <= 0x7F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 4:
                    for (int x = 0x0; x <= 0x5F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 5:
                    for (int x = 0x0; x <= 0x6F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
            }
        }
        f.println("};");
        SerialM.print(F("preset saved as: "));
        SerialM.println(f.name());
        f.close();
    }
}

