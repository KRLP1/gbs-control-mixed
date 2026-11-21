#include "UserCommandHandler.h"
#include "../core/Globals.h"
#include "../web/SerialMirror.h"
#include "../config/WiFiConfig.h"
#include "../utils/Logging.h"
#include "../storage/PreferencesManager.h"
#include "../storage/SlotManager.h"
#include "../gbs/GBSController.h"
#include "../gbs/GBSVideoProcessing.h"
#include "../gbs/GBSPhase.h"
#include "../gbs/GBSSync.h"
#include "../video/Scanlines.h"
#include "../video/Sharpness.h"
#include "../video/Deinterlacing.h"
#include "../video/VideoInput.h"
#include "../video/framesync.h"
#include "../core/slot.h"
#include "../gbs/tv5725.h"
#include <LittleFS.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

typedef TV5725<GBS_ADDR> GBS;

// Pending values handed over from the value queue (populated in gbs-control.ino
// loop() before handleType2Command runs; 255 = no value -> command is ignored).
uint8_t pendingSharpnessValue = 255;
uint8_t pendingScanlineStrength = 255;

static void setFrameTimeLockMethod(uint8_t method)
{
    if (uopt->frameTimeLockMethod == method) {
        return;
    }
    if (!rto->extClockGenDetected) {
        FrameSync::reset(uopt->frameTimeLockMethod);
    }
    uopt->frameTimeLockMethod = method;
    saveUserPrefs();
    activeFrameTimeLockInitialSteps();
}

// Reads the SlotMeta entry of the current slot (uopt->presetSlot) from slots.bin: one
// 32 byte record by seek instead of the whole 2.3 KB array, so no big buffer has to sit in
// RAM permanently (a static one costs that much on the ESP8266 for the whole runtime).
static bool readCurrentSlotMeta(SlotMeta &meta)
{
    int currentSlot = slotIndexMap.indexOf(uopt->presetSlot);
    if (currentSlot < 0 || currentSlot >= SLOTS_TOTAL) {
        return false;
    }
    File f = LittleFS.open(SLOTS_FILE, "r");
    if (!f) {
        return false;
    }
    // plausibility check as before: a file of any other size is not ours
    bool ok = (f.size() == sizeof(SlotMetaArray)) &&
              f.seek((uint32_t)currentSlot * sizeof(SlotMeta)) &&
              (f.read((byte *)&meta, sizeof(meta)) == sizeof(meta));
    f.close();
    return ok;
}

// Restores scanline on/off + strength from the slot metadata (slots.bin) of the current
// slot. Used when a preset file carries no scanline line (older firmware), and by the
// "restore filters" command. A slot that was never saved with scanlines (scanlines off and
// strength 0) keeps the current strength instead of resetting it to 0 = strongest lines.
static bool restoreScanlinesFromSlot(bool persist)
{
    SlotMeta meta;
    if (!readCurrentSlotMeta(meta)) {
        return false;
    }
    const bool on = meta.scanlines != 0;
    uint8_t strength = meta.scanlinesStrength;
    // slots.bin is a raw struct dump: a corrupt or foreign file must not set an invalid value
    if (strength > SCANLINES_MAX_STRENGTH_VALUE) {
        strength = 0x30;
    }
    if (!on && strength == 0) {
        strength = uopt->scanlineStrength;
    }
    applyScanlineState(on, strength, persist);
    SerialM.print(F("slot: "));
    SerialM.print((char)uopt->presetSlot);
    SerialM.println(F(" (scanlines from slot data)"));
    return true;
}

void handleType2Command(char argument)
{
    myLog("user", argument);
    switch (argument) {
        case '0':
            SerialM.print(F("pal force 60hz "));
            if (uopt->PalForce60 == 0) {
                uopt->PalForce60 = 1;
                SerialM.println("on");
            } else {
                uopt->PalForce60 = 0;
                SerialM.println("off");
            }
            saveUserPrefs();
            break;
        case '1':
            // reset to defaults button
            webSocket.disconnect();
            loadDefaultUserOptions();
            saveUserPrefs();
            Serial.println(F("options set to defaults, restarting"));
            delay(60);
#ifdef ESP8266
            ESP.reset(); // don't use restart(), messes up websocket reconnects
#else
            ESP.restart();
#endif
            //
            break;
        case '2':
            //
            break;
        case '3': // load custom preset
        {
            uopt->presetPreference = OutputCustomized; // custom
            // Scanlines are part of the preset like the sharpness: the preset file carries
            // them (loadPresetFromLittleFS() applies them while the pending flag is set).
            // Presets saved by older firmware have no scanline line; those fall back to
            // the slot metadata (slots.bin) below.
            presetScanlinesRestored = false;
            if (rto->videoStandardInput == 14) {
                // vga upscale path: let synwatcher handle it (the preset file is read
                // later, not by this command, so the slot metadata is used here)
                rto->videoStandardInput = 15;
                presetScanlinesRestorePending = false;
            } else {
                // normal path
                presetScanlinesRestorePending = true;
                applyPresets(rto->videoStandardInput);
                presetScanlinesRestorePending = false; // consumed by the loader, or not read at all
            }
            if (!presetScanlinesRestored) {
                restoreScanlinesFromSlot(false);
            }
            // saveUserPrefs() below persists everything at once; a deferred
            // write armed by the value steppers is covered by that save
            saveUserPrefs();
        } break;
        case '4': // save custom preset
            savePresetToLittleFS();
            uopt->presetPreference = OutputCustomized; // custom
            saveUserPrefs();
            break;
        case '5':
            //Frame Time Lock toggle
            uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
            saveUserPrefs();
            if (uopt->enableFrameTimeLock) {
                SerialM.println(F("FTL on"));
            } else {
                SerialM.println(F("FTL off"));
            }
            if (!rto->extClockGenDetected) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->enableFrameTimeLock) {
                activeFrameTimeLockInitialSteps();
            }
            break;
        case '6':
            //
            break;
        case '7':
            // toggle on/off, the strength is kept (applyScanlineState prints the new state)
            applyScanlineState(!uopt->wantScanlines, uopt->scanlineStrength, true);
            break;
        case '8':
            // peaking level button: 0=off, 1=normal (22), 2=strong (31), 3=max (32)
            cyclePeakingLevel();
            break;
        case '9':
            //
            break;
        case 'a':
            webSocket.disconnect();
            // write a pending deferred stepper save now: the restart would drop it
            flushDeferredUserPrefsSave();
            Serial.println(F("restart"));
            delay(60);
#ifdef ESP8266
            ESP.reset(); // don't use restart(), messes up websocket reconnects
#else
            ESP.restart();
#endif
            break;
        case 'R':
            uopt->reverseRotaryEncoderForOledMenu ^= 1;
            SerialM.print(F("reverse OLED rotary: "));
            SerialM.println(uopt->reverseRotaryEncoderForOledMenu ? F("on") : F("off"));
            saveUserPrefs();
            break;
        case 'e': // print files on LittleFS
        {
#ifdef ESP8266
            Dir dir = LittleFS.openDir("/");
            while (dir.next()) {
                SerialM.print(dir.fileName());
                SerialM.print(" ");
                SerialM.println(dir.fileSize());
                delay(1); // wifi stack
            }
#else
            File root = LittleFS.open("/");
            File file = root.openNextFile();
            while(file){
                SerialM.print(file.name());
                SerialM.print(" ");
                SerialM.println(file.size());
                file = root.openNextFile();
                delay(1);
            }
#endif
            ////
            File f = LittleFS.open("/preferencesv2.txt", "r");
            if (!f) {
                SerialM.println(F("failed opening preferences file"));
            } else {
                SerialM.print(F("preset preference = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("frame time lock = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("preset slot = "));
                SerialM.println((uint8_t)(f.read()));
                SerialM.print(F("frame lock method = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("auto gain = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("scanlines = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("component output = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("deinterlacer mode = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("line filter = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("peaking = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("preferScalingRgbhv = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("6-tap = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("pal force60 = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("matched = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("step response = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("disable external clock generator = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                if (f.available()) {
                    SerialM.print(F("menu language (0=de, 1=en) = "));
                    SerialM.println((uint8_t)(f.read() - '0'));
                }

                f.close();
            }
        } break;
        case 'f':
        case 'g':
        case 'h':
        case 'p':
        case 's':
        case 'L':
        case 'W':
        case 'G': {
            // load preset via webui
            uint8_t videoMode = getVideoMode();
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput; // last known good as fallback
            //else videoMode stays 0 and we'll apply via some assumptions

            if (argument == 'f')
                uopt->presetPreference = Output960P; // 1280x960
            if (argument == 'g')
                uopt->presetPreference = Output720P; // 1280x720
            if (argument == 'h')
                uopt->presetPreference = Output480P; // 720x480/768x576
            if (argument == 'p')
                uopt->presetPreference = Output1024P; // 1280x1024
            if (argument == 's')
                uopt->presetPreference = Output1080P; // 1920x1080
            if (argument == 'L')
                uopt->presetPreference = OutputDownscale; // downscale
            if (argument == 'W')
                uopt->presetPreference = Output960PWindowbox; // 1080p 4:3 windowbox
            if (argument == 'G')
                uopt->presetPreference = Output1024x768; // 1024x768

            rto->useHdmiSyncFix = 1; // disables sync out when programming preset
            if (rto->videoStandardInput == 14) {
                // vga upscale path: let synwatcher handle it
                rto->videoStandardInput = 15;
            } else {
                // normal path
                applyPresets(videoMode);
            }
            saveUserPrefs();
        } break;
        case 'i':
            // toggle active frametime lock method
            if (!rto->extClockGenDetected) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->frameTimeLockMethod == 0) {
                uopt->frameTimeLockMethod = 1;
            } else if (uopt->frameTimeLockMethod == 1) {
                uopt->frameTimeLockMethod = 0;
            }
            saveUserPrefs();
            activeFrameTimeLockInitialSteps();
            break;
        case 'j':
            // FTL method: VTotal + VSST
            setFrameTimeLockMethod(0);
            break;
        case 'k':
            // FTL method: VTotal only
            setFrameTimeLockMethod(1);
            break;
        case 'l':
            // cycle through available SDRAM clocks
            {
                uint8_t PLL_MS = GBS::PLL_MS::read();
                uint8_t memClock = 0;

                if (PLL_MS == 0)
                    PLL_MS = 2;
                else if (PLL_MS == 2)
                    PLL_MS = 7;
                else if (PLL_MS == 7)
                    PLL_MS = 4;
                else if (PLL_MS == 4)
                    PLL_MS = 3;
                else if (PLL_MS == 3)
                    PLL_MS = 5;
                else if (PLL_MS == 5)
                    PLL_MS = 0;

                switch (PLL_MS) {
                    case 0:
                        memClock = 108;
                        break;
                    case 1:
                        memClock = 81;
                        break; // goes well with 4_2C = 0x14, 4_2D = 0x27
                    case 2:
                        memClock = 10;
                        break; // feedback clock
                    case 3:
                        memClock = 162;
                        break;
                    case 4:
                        memClock = 144;
                        break;
                    case 5:
                        memClock = 185;
                        break; // slight OC
                    case 6:
                        memClock = 216;
                        break; // !OC!
                    case 7:
                        memClock = 129;
                        break;
                    default:
                        break;
                }
                GBS::PLL_MS::write(PLL_MS);
                ResetSDRAM();
                if (memClock != 10) {
                    SerialM.print(F("SDRAM clock: "));
                    SerialM.print(memClock);
                    SerialM.println("Mhz");
                } else {
                    SerialM.print(F("SDRAM clock: "));
                    SerialM.println(F("Feedback clock"));
                }
            }
            break;
        case 'm':
            SerialM.print(F("Line Filter: "));
            if (uopt->wantVdsLineFilter) {
                uopt->wantVdsLineFilter = 0;
                GBS::VDS_D_RAM_BYPS::write(1);
                SerialM.println("off");
            } else {
                uopt->wantVdsLineFilter = 1;
                GBS::VDS_D_RAM_BYPS::write(0);
                SerialM.println("on");
            }
            saveUserPrefs();
            break;
        case 'n':
            SerialM.print(F("ADC gain++ : "));
            uopt->enableAutoGain = 0;
            setAdcGain(GBS::ADC_RGCTRL::read() - 1);
            SerialM.println(GBS::ADC_RGCTRL::read(), HEX);
            break;
        case 'o':
            SerialM.print(F("ADC gain-- : "));
            uopt->enableAutoGain = 0;
            setAdcGain(GBS::ADC_RGCTRL::read() + 1);
            SerialM.println(GBS::ADC_RGCTRL::read(), HEX);
            break;
        case 'A': {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd > 4) && (hbspd < (htotal - 4))) {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() - 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() + 4);
            } else {
                SerialM.println("limit");
            }
        } break;
        case 'B': {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd < (htotal - 4)) && (hbspd > 4)) {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() - 4);
            } else {
                SerialM.println("limit");
            }
        } break;
        case 'C': {
            // vert mask +
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd > 6) && (vbspd < (vtotal - 4))) {
                GBS::VDS_DIS_VB_ST::write(vbstd - 2);
                GBS::VDS_DIS_VB_SP::write(vbspd + 2);
            } else {
                SerialM.println("limit");
            }
        } break;
        case 'D': {
            // vert mask -
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd < (vtotal - 4)) && (vbspd > 6)) {
                GBS::VDS_DIS_VB_ST::write(vbstd + 2);
                GBS::VDS_DIS_VB_SP::write(vbspd - 2);
            } else {
                SerialM.println("limit");
            }
        } break;
        case 'q':
            if (uopt->deintMode != 1) {
                uopt->deintMode = 1;
                disableMotionAdaptDeinterlace();
                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read()) {
                    disableScanlines();
                }
                saveUserPrefs();
            }
            SerialM.println(F("Deinterlacer: Bob"));
            break;
        case 'r':
            if (uopt->deintMode != 0) {
                uopt->deintMode = 0;
                saveUserPrefs();
                // will enable next loop()
            }
            SerialM.println(F("Deinterlacer: Motion Adaptive"));
            break;
        case 't':
            // unused now
            SerialM.print(F("6-tap: "));
            if (uopt->wantTap6 == 0) {
                uopt->wantTap6 = 1;
                GBS::VDS_TAP6_BYPS::write(0);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() - 1);
                SerialM.println("on");
            } else {
                uopt->wantTap6 = 0;
                GBS::VDS_TAP6_BYPS::write(1);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() + 1);
                SerialM.println("off");
            }
            saveUserPrefs();
            break;
        case 'u':
            // restart to attempt wifi station mode connect
            delay(30);
#ifdef ESP8266
            if (pendingWifiSSID.length()) {
                // Backported from JuergenLeber/gbs-control: persist WiFi credentials and STA mode
                // directly via SDK flash functions, here in the main loop rather than inside the
                // /wifi/connect request callback (see WebServer.cpp). wifi_set_opmode() and
                // wifi_station_set_config() write to flash (unlike their *_current() variants,
                // which only update RAM), so the credentials survive the restart below.
                struct station_config wifiConf;
                memset(&wifiConf, 0, sizeof(wifiConf));
                // ssid[32] needs no terminator in the SDK; a 32 character SSID uses all bytes
                // (the struct is zeroed above, shorter SSIDs stay terminated)
                strncpy(reinterpret_cast<char *>(wifiConf.ssid), pendingWifiSSID.c_str(), sizeof(wifiConf.ssid));
                strncpy(reinterpret_cast<char *>(wifiConf.password), pendingWifiPassword.c_str(), sizeof(wifiConf.password) - 1);
                wifi_set_opmode(STATION_MODE);
                wifi_station_set_config(&wifiConf);
                pendingWifiSSID = "";
                pendingWifiPassword = "";
            } else {
                WiFi.mode(WIFI_STA);
            }
            WiFi.hostname(device_hostname.c_str());
#else
            WiFi.mode(WIFI_STA);
            WiFi.setHostname(device_hostname.c_str());
#endif
            delay(30);
#ifdef ESP8266
            ESP.reset();
#else
            ESP.restart();
#endif
            break;
        case 'v': {
            uopt->wantFullHeight = !uopt->wantFullHeight;
            saveUserPrefs();
            uint8_t vidMode = getVideoMode();
            if (uopt->presetPreference == 5) {
                if (GBS::GBS_PRESET_ID::read() == 0x05 || GBS::GBS_PRESET_ID::read() == 0x15) {
                    applyPresets(vidMode);
                }
            }
        } break;
        case 'w':
            uopt->enableCalibrationADC = !uopt->enableCalibrationADC;
            saveUserPrefs();
            break;
        case 'x':
            uopt->preferScalingRgbhv = !uopt->preferScalingRgbhv;
            SerialM.print(F("preferScalingRgbhv: "));
            if (uopt->preferScalingRgbhv) {
                SerialM.println("on");
            } else {
                SerialM.println("off");
            }
            saveUserPrefs();
            break;
        case 'X':
            SerialM.print(F("ExternalClockGenerator "));
            if (uopt->disableExternalClockGenerator == 0) {
                uopt->disableExternalClockGenerator = 1;
                SerialM.println("disabled");
            } else {
                uopt->disableExternalClockGenerator = 0;
                SerialM.println("enabled");
            }
            saveUserPrefs();
            break;
        case 'z':
            // sog slicer level
            if (rto->currentLevelSOG > 0) {
                rto->currentLevelSOG -= 1;
            } else {
                rto->currentLevelSOG = 16;
            }
            setAndUpdateSogLevel(rto->currentLevelSOG);
            optimizePhaseSP();
            SerialM.print("Phase: ");
            SerialM.print(rto->phaseSP);
            SerialM.print(" SOG: ");
            SerialM.print(rto->currentLevelSOG);
            SerialM.println();
            break;
        case 'E':
            // test option for now
            SerialM.print(F("IF Auto Offset: "));
            toggleIfAutoOffset();
            if (GBS::IF_AUTO_OFST_EN::read()) {
                SerialM.println("on");
            } else {
                SerialM.println("off");
            }
            break;
        case 'F':
            // freeze pic
            if (GBS::CAPTURE_ENABLE::read()) {
                GBS::CAPTURE_ENABLE::write(0);
            } else {
                GBS::CAPTURE_ENABLE::write(1);
            }
            break;
        // 'K' was the "Intensität" button (one scanline strength step stronger).
        // Removed together with the WebUI button: the value steppers own that
        // now. The letter stays reserved here so a future user command can
        // take it without colliding with the serial 'K' (HD bypass switch).
        case 'Z':
            // brightness++ (wrap guard: 8 bit register)
            if (GBS::VDS_Y_OFST::read() < 255) {
                GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 1);
                SerialM.print(F("Brightness++ : "));
                SerialM.println(GBS::VDS_Y_OFST::read(), DEC);
            } else {
                SerialM.println("limit");
            }
            break;
        case 'T':
            // brightness-- (wrap guard: 8 bit register)
            if (GBS::VDS_Y_OFST::read() > 0) {
                GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() - 1);
                SerialM.print(F("Brightness-- : "));
                SerialM.println(GBS::VDS_Y_OFST::read(), DEC);
            } else {
                SerialM.println("limit");
            }
        break;
        case 'N':
            // contrast++ (wrap guard: 8 bit register)
            if (GBS::VDS_Y_GAIN::read() < 255) {
                GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() + 1);
                SerialM.print(F("Contrast++ : "));
                SerialM.println(GBS::VDS_Y_GAIN::read(), DEC);
            } else {
                SerialM.println("limit");
            }
        break;
        case 'M':
            // contrast-- (wrap guard: 8 bit register)
            if (GBS::VDS_Y_GAIN::read() > 0) {
                GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() - 1);
                SerialM.print(F("Contrast-- : "));
                SerialM.println(GBS::VDS_Y_GAIN::read(), DEC);
            } else {
                SerialM.println("limit");
            }
        break;
        case 'Q':
             // pb/u gain++ (wrap guard: 8 bit register)
            if (GBS::VDS_UCOS_GAIN::read() < 255) {
                GBS::VDS_UCOS_GAIN::write(GBS::VDS_UCOS_GAIN::read() + 1);
                SerialM.print(F("Pb/U gain++ : "));
                SerialM.println(GBS::VDS_UCOS_GAIN::read(), DEC);
            } else {
                SerialM.println("limit");
            }
            break;
        case 'H':
             // pb/u gain-- (wrap guard: 8 bit register)
            if (GBS::VDS_UCOS_GAIN::read() > 0) {
                GBS::VDS_UCOS_GAIN::write(GBS::VDS_UCOS_GAIN::read() - 1);
                SerialM.print(F("Pb/U gain-- : "));
                SerialM.println(GBS::VDS_UCOS_GAIN::read(), DEC);
            } else {
                SerialM.println("limit");
            }
            break;
        break;
        case 'P':
            // pr/v gain++ (wrap guard: 8 bit register)
            if (GBS::VDS_VCOS_GAIN::read() < 255) {
                GBS::VDS_VCOS_GAIN::write(GBS::VDS_VCOS_GAIN::read() + 1);
                SerialM.print(F("Pr/V gain++ : "));
                SerialM.println(GBS::VDS_VCOS_GAIN::read(), DEC);
            } else {
                SerialM.println("limit");
            }
            break;
        case 'S':
            // pr/v gain-- (wrap guard: 8 bit register)
            if (GBS::VDS_VCOS_GAIN::read() > 0) {
                GBS::VDS_VCOS_GAIN::write(GBS::VDS_VCOS_GAIN::read() - 1);
                SerialM.print(F("Pr/V gain-- : "));
                SerialM.println(GBS::VDS_VCOS_GAIN::read(), DEC);
            } else {
                SerialM.println("limit");
            }
            break;
        case 'O':
            // info
            if (GBS::ADC_INPUT_SEL::read() == 1)
            {
                SerialM.println("RGB reg");
                SerialM.println(F("------------ "));
                SerialM.print(F("Y_OFFSET: "));
                SerialM.println(GBS::VDS_Y_OFST::read(), DEC);
                SerialM.print(F("U_OFFSET: "));
                SerialM.println( GBS::VDS_U_OFST::read(), DEC);
                SerialM.print(F("V_OFFSET: "));
                SerialM.println(GBS::VDS_V_OFST::read(), DEC);
                SerialM.print(F("Y_GAIN: "));
                SerialM.println(GBS::VDS_Y_GAIN::read(), DEC);
                SerialM.print(F("USIN_GAIN: "));
                SerialM.println(GBS::VDS_USIN_GAIN::read(), DEC);
                SerialM.print(F("UCOS_GAIN: "));
                SerialM.println(GBS::VDS_UCOS_GAIN::read(), DEC);
            }
            else
            {
                SerialM.println("YPbPr reg");
                SerialM.println(F("------------ "));
                SerialM.print(F("Y_OFFSET: "));
                SerialM.println(GBS::VDS_Y_OFST::read(), DEC);
                SerialM.print(F("U_OFFSET: "));
                SerialM.println( GBS::VDS_U_OFST::read(), DEC);
                SerialM.print(F("V_OFFSET: "));
                SerialM.println(GBS::VDS_V_OFST::read(), DEC);
                SerialM.print(F("Y_GAIN: "));
                SerialM.println(GBS::VDS_Y_GAIN::read(), DEC);
                SerialM.print(F("USIN_GAIN: "));
                SerialM.println(GBS::VDS_USIN_GAIN::read(), DEC);
                SerialM.print(F("UCOS_GAIN: "));
                SerialM.println(GBS::VDS_UCOS_GAIN::read(), DEC);
            }
            break;
        case 'U':
            // default
            if (GBS::ADC_INPUT_SEL::read() == 1)
            {
                GBS::VDS_Y_GAIN::write(128);
                GBS::VDS_UCOS_GAIN::write(28);
                GBS::VDS_VCOS_GAIN::write(41);
                GBS::VDS_Y_OFST::write(0);
                GBS::VDS_U_OFST::write(0);
                GBS::VDS_V_OFST::write(0);
                GBS::ADC_ROFCTRL::write(adco->r_off);
                GBS::ADC_GOFCTRL::write(adco->g_off);
                GBS::ADC_BOFCTRL::write(adco->b_off);
                SerialM.println("RGB:defauit");
            }
            else
            {
                GBS::VDS_Y_GAIN::write(128);
                GBS::VDS_UCOS_GAIN::write(28);
                GBS::VDS_VCOS_GAIN::write(41);
                GBS::VDS_Y_OFST::write(254);
                GBS::VDS_U_OFST::write(3);
                GBS::VDS_V_OFST::write(3);
                GBS::ADC_ROFCTRL::write(adco->r_off);
                GBS::ADC_GOFCTRL::write(adco->g_off);
                GBS::ADC_BOFCTRL::write(adco->b_off);
                SerialM.println("YPbPr:defauit");
            }
            break;
        case 'V': {
            // /gbs/restore-filters: restore the slot filters from slots.bin.
            // Runs here in the loop() context (I2C + LittleFS).
            // Only the current slot's 32 byte record is read (readCurrentSlotMeta), not the
            // whole array: no 2.3 KB static buffer in RAM.
            SlotMeta meta;
            bool ok = false;
            if (readCurrentSlotMeta(meta)) {
                // scanlines on/off + strength through the one shared function
                // (persisted below together with the other filter fields). Skipped
                // when the preset load just before already took them from the
                // preset file: the file is written together with the slot data,
                // and the file value is the one that was actually saved.
                if (!presetScanlinesRestored) {
                    const bool on = meta.scanlines != 0;
                    uint8_t strength = meta.scanlinesStrength;
                    // same guard as the boot path: slots.bin is a raw struct dump,
                    // a corrupt or foreign file must not set an invalid value
                    if (strength > SCANLINES_MAX_STRENGTH_VALUE)
                        strength = 0x30;
                    if (!on && strength == 0) // slot never saved with scanlines
                        strength = uopt->scanlineStrength;
                    applyScanlineState(on, strength, false);
                }
                uopt->wantVdsLineFilter = meta.wantVdsLineFilter;
                uopt->wantStepResponse = meta.wantStepResponse;
                uopt->wantPeaking = meta.wantPeaking;
                // slots only store on/off: fold it into picSharpness (off -> 0,
                // on -> last non-zero value), so peaking state has one source
                adoptWantPeaking();
                // persist after all five fields are set, not in the middle
                saveUserPrefs();
                ok = true;
            }
            if (!ok) {
                SerialM.println(F("restore filters failed"));
            }
        } break;
        case 'Y': {
            // set sharpness 0..31 (value from userValueQueue, see gbs-control.ino loop).
            // 'Y' chosen because v1.7.4 already uses 'J' for menu language German
            // (and 'I' for English); the old plan's 'J'/'p' are taken.
            // Without a value (255) there is nothing to set: ignore the command.
            // setPictureSharpness saves deferred: the stepper raster must not
            // cost one LittleFS write per click (heap recovery, see there).
            if (pendingSharpnessValue != 255) {
                setPictureSharpness(pendingSharpnessValue);
                pendingSharpnessValue = 255;
            }
        } break;
        case 'y': {
            // scanline value command (value from userValueQueue):
            //   0x00..0x60 = switch scanlines on with this strength (0x00 = strongest),
            //   0x7f       = switch scanlines off (strength is kept).
            // ('l' is the SDRAM clock cycle command, so the value command is 'y')
            // Without a value (255) there is nothing to set: ignore the command.
            // persist = false: the +/- stepper raster ends in one deferred
            // LittleFS write (see PreferencesManager), not one write per click.
            if (pendingScanlineStrength != 255) {
                if (pendingScanlineStrength == SCANLINES_OFF_VALUE) {
                    applyScanlineState(false, uopt->scanlineStrength, false);
                } else {
                    applyScanlineState(true, pendingScanlineStrength, false);
                }
                pendingScanlineStrength = 255;
            }
        } break;
        // Menu language commands ('I' = English, 'J' = German). Queued by /lang/set
        // and executed here in the loop() context, so the LittleFS write cannot
        // race with other saves. The serial console uses the same letters for its
        // own debug commands ('I' = VDS register dump, 'J' = PLL/AD reset), but that
        // is a separate command channel (/sc) and is not affected. Only a direct
        // /uc?I or /uc?J request ends up here.
        case 'I': // menu language English (OLED follows after a restart)
            uopt->menuLanguage = 1;
            saveUserPrefs();
            SerialM.println(F("menu language: English"));
            break;
        case 'J': // menu language German
            uopt->menuLanguage = 0;
            saveUserPrefs();
            SerialM.println(F("menu language: German"));
            break;
        default:
            break;
    }
}
