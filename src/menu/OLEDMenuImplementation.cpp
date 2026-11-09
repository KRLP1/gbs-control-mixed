#define OSD_TIMEOUT 8000

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include "OLEDMenuImplementation.h"
#include "OLEDMenuStrings.h"
#include "OTAUpdate.h"
#include "../wifi/WiFiManager.h"
#include "../core/options.h"
#include "../gbs/tv5725.h"
#include "../core/slot.h"
#include "../../lib/WebSockets/WebSockets.h"
#include "../../lib/WebSockets/WebSocketsServer.h"
#include "../data/fonts.h"
#include "OSDManager.h"

typedef TV5725<GBS_ADDR> GBS;
extern void applyPresets(uint8_t videoMode);
extern void setOutModeHdBypass(bool bypass);
extern void saveUserPrefs();
extern float getOutputFrameRate();
extern void loadDefaultUserOptions();
extern uint8_t getVideoMode();
extern runTimeOptions *rto;
extern userOptions *uopt;
extern String slotIndexMap; // Slot-Index 0..71 -> Slot-Zeichen
extern const char *ap_ssid;
extern const char *ap_password;
extern const char *device_hostname_full;
extern String device_hostname;
extern const char *FIRMWARE_VERSION;
extern WebSocketsServer webSocket;
extern OLEDMenuManager oledMenu;
extern OSDManager osdManager;
unsigned long oledMenuFreezeStartTime;
unsigned long oledMenuFreezeTimeoutInMS;

bool resolutionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
        }
        return false;
    }
    oledMenuFreezeTimeoutInMS = 1000; // freeze for 1s
    oledMenuFreezeStartTime = millis();
    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, 16, item->str);
    display->setFont(DejaVu_Sans_Mono_12);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_LOADED));
    display->display();
    uint8_t videoMode = getVideoMode();
    PresetPreference preset = PresetPreference::Output1080P;
    switch (item->tag) {
        case MT_1280x960:
            preset = PresetPreference::Output960P;
            break;
        case MT1280x1024:
            preset = PresetPreference::Output1024P;
            break;
        case MT1280x720:
            preset = PresetPreference::Output720P;
            break;
        case MT1920x1080:
            preset = PresetPreference::Output1080P;
            break;
        case MT1080x960W:
            preset = PresetPreference::Output960PWindowbox;
            break;
        case MT_1024x768:
            preset = PresetPreference::Output1024x768;
            break;
        case MT_480s576:
            preset = PresetPreference::Output480P;
            break;
        case MT_DOWNSCALE:
            preset = PresetPreference::OutputDownscale;
            break;
        case MT_BYPASS:
            preset = PresetPreference::OutputCustomized;
            break;
        default:
            break;
    }
    if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read()) {
        videoMode = rto->videoStandardInput;
    }
    if (item->tag != MT_BYPASS) {
        uopt->presetPreference = preset;
        rto->useHdmiSyncFix = 1;
        if (rto->videoStandardInput == 14) {
            rto->videoStandardInput = 15;
        } else {
            applyPresets(videoMode);
        }
    } else {
        setOutModeHdBypass(false);
        uopt->presetPreference = preset;
        if (rto->videoStandardInput != 15) {
            rto->autoBestHtotalEnabled = 0;
            if (rto->applyPresetDoneStage == 11) {
                rto->applyPresetDoneStage = 1;
            } else {
                rto->applyPresetDoneStage = 10;
            }
        } else {
            rto->applyPresetDoneStage = 1;
        }
    }
    saveUserPrefs();
    manager->freeze();
    return false;
}
bool presetSelectionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
        }
        return false;
    }
    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, 16, item->str);
    display->setFont(DejaVu_Sans_Mono_12);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_LOADED));
    display->display();
    // item->tag = Slot-Index (0..71), nicht das Slot-Zeichen
    if (item->tag >= slotIndexMap.length()) {
        manager->unfreeze(); // ungültiger Index: Slot nicht überschreiben
        return false;
    }
    uopt->presetSlot = slotIndexMap[item->tag];
    uopt->presetPreference = PresetPreference::OutputCustomized;
    saveUserPrefs();
    if (rto->videoStandardInput == 14) {
        // vga upscale path: let synwatcher handle it
        rto->videoStandardInput = 15;
    } else {
        // normal path
        applyPresets(rto->videoStandardInput);
    }
    saveUserPrefs();
    manager->freeze();
    oledMenuFreezeTimeoutInMS = 2000;
    oledMenuFreezeStartTime = millis();

    return false;
}

static bool isValidSlotName(const char *name, size_t maxLen) {
    if (!name || !*name) return false;
    if (strcmp(EMPTY_SLOT_NAME, name) == 0) return false;
    // Erstes Zeichen: jedes sichtbare ASCII-Zeichen (0x21-0x7E). Die WebUI erlaubt auch
    // Namen wie "(Test)" oder "-Mario"; nur Leerzeichen/Steuerzeichen am Anfang gelten
    // als ungueltig (leerer oder beschaedigter Slot).
    unsigned char first = (unsigned char)name[0];
    if (first <= 0x20 || first > 0x7E) return false;
    for (size_t i = 0; i < maxLen; ++i) {
        char c = name[i];
        if (c == '\0') return true;
        if (c < 0x20 || c > 0x7E) return false;
    }
    return false;
}

bool presetsCreationMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool)
{
    // Static statt Heap/Stack: im BSS, ueberlebt den Funktionsaufruf,
    // keine dangling Zeiger wenn registerItem nur den Pointer speichert
    static SlotMetaArray slotsObject;

    File slotsBinaryFileRead = LittleFS.open(SLOTS_FILE, "r");
    manager->clearSubItems(item);
    int curNumSlot = 0;
    bool tooManySlots = false;
    if (slotsBinaryFileRead) {
        slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFileRead.close();

        // Erst zaehlen. Passen nicht alle Slots in das Untermenue, bleibt ein Eintrag
        // fuer den Hinweis frei - sonst loest der Hinweis als 17. Untereintrag
        // den Panic "Maximum number of sub items reached" aus.
        int usedSlots = 0;
        for (int i = 0; i < SLOTS_TOTAL; ++i) {
            const SlotMeta &slot = slotsObject.slot[i];
            if (isValidSlotName(slot.name, sizeof(slot.name))) {
                usedSlots++;
            }
        }
        tooManySlots = usedSlots > OLED_MENU_MAX_SUBITEMS_NUM;
        const int maxShown = tooManySlots ? OLED_MENU_MAX_SUBITEMS_NUM - 1 : OLED_MENU_MAX_SUBITEMS_NUM;

        for (int i = 0; i < SLOTS_TOTAL && curNumSlot < maxShown; ++i) {
            const SlotMeta &slot = slotsObject.slot[i];
            if (!isValidSlotName(slot.name, sizeof(slot.name))) {
                continue;
            }
            curNumSlot++;
            manager->registerItem(item, slot.slot, slot.name, presetSelectionMenuHandler);
        }
    }

    if (tooManySlots) {
        manager->registerItem(item, 0, oledMsg(MSG_TOO_MANY_PRESETS));
    }

    if (!item->numSubItem) {
        manager->registerItem(item, 0, oledMsg(MSG_NO_PRESETS));
    }

    return true;
}
bool resetMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        // not precise
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
            ESP.restart();
            return false;
        }
        return false;
    }

    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(DejaVu_Sans_Mono_12);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    switch (item->tag) {
        case MT_RESET_GBS:
            display->drawString(OLED_MENU_WIDTH / 2, 16, oledMsg(MSG_RESETTING_GBS));
            display->drawString(OLED_MENU_WIDTH / 2, 31, oledMsg(MSG_PLEASE_WAIT));
            display->drawString(OLED_MENU_WIDTH / 2, 46, "...");
            break;
        case MT_RESTORE_FACTORY:
            display->drawString(OLED_MENU_WIDTH / 2, 16, oledMsg(MSG_RESETTING_FACTORY));
            display->drawString(OLED_MENU_WIDTH / 2, 31, oledMsg(MSG_PLEASE_WAIT));
            display->drawString(OLED_MENU_WIDTH / 2, 46, "...");
            break;
        case MT_RESET_WIFI:
            display->drawString(OLED_MENU_WIDTH / 2, 16, oledMsg(MSG_RESETTING_WIFI));
            display->drawString(OLED_MENU_WIDTH / 2, 31, oledMsg(MSG_PLEASE_WAIT));
            display->drawString(OLED_MENU_WIDTH / 2, 46, "...");
            break;
    }
    display->display();
    // Drop all websocket clients but keep the server running
    webSocket.disconnect();
    delay(50);
    switch (item->tag) {
        case MT_RESET_WIFI:
            // Keep ESP8266 behavior (persistent toggle used to erase creds),
            // but don't use WiFi.persistent() on ESP32 (it changes storage to RAM).
#ifdef ESP8266
            WiFi.persistent(true);
            WiFi.disconnect();
            WiFi.persistent(false);
#else
            // ESP32: erase saved AP config (credentials) explicitly
            WiFi.disconnect(true, true);
#endif
            wifiForgetRetryCredentials(); // credentials are gone on purpose, no AP fallback retry with the old ones
            break;
        case MT_RESTORE_FACTORY:
            loadDefaultUserOptions();
            saveUserPrefs();
            break;
    }
    manager->freeze();
    oledMenuFreezeStartTime = millis();
    oledMenuFreezeTimeoutInMS = 2000; // freeze for 2 seconds
    return false;
}
bool currentSettingHandler(OLEDMenuManager *manager, OLEDMenuItem *, OLEDMenuNav nav, bool isFirstTime)
{
    static unsigned long lastUpdateTime = 0;
    if (isFirstTime) {
        lastUpdateTime = 0;
        oledMenuFreezeStartTime = millis();
        oledMenuFreezeTimeoutInMS = 2000; // freeze for 2 seconds if no input
        manager->freeze();
    } else if (nav != OLEDMenuNav::IDLE) {
        manager->unfreeze();
        return false;
    }
    if (millis() - lastUpdateTime <= 200) {
        return false;
    }
    OLEDDisplay &display = *manager->getDisplay();
    display.clear();
    display.setColor(OLEDDISPLAY_COLOR::WHITE);
    display.setFont(ArialMT_Plain_16);
    if (rto->sourceDisconnected || !rto->boardHasPower) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
            return false;
        }
        display.setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
        display.setFont(DejaVu_Sans_Mono_12);
        display.drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_NO_INPUT));
    } else {
        boolean vsyncActive = 0;
        boolean hsyncActive = 0;
        float ofr = getOutputFrameRate();
        uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
        rto->presetID = GBS::GBS_PRESET_ID::read();

        display.setFont(URW_Gothic_L_Book_20);
        display.setTextAlignment(TEXT_ALIGN_LEFT);

        if (rto->presetID == 0x08 || rto->presetID == 0x18) {
            display.drawString(0, 0, "1024x768");
        } else if (rto->presetID == 0x01 || rto->presetID == 0x11) {
            display.drawString(0, 0, "1280x960");
        } else if (rto->presetID == 0x02 || rto->presetID == 0x12) {
            display.drawString(0, 0, "1280x1024");
        } else if (rto->presetID == 0x03 || rto->presetID == 0x13) {
            display.drawString(0, 0, "1280x720");
        } else if (rto->presetID == 0x05 || rto->presetID == 0x15) {
            display.drawString(0, 0, "1920x1080");
        } else if (rto->presetID == 0x06 || rto->presetID == 0x16) {
            display.drawString(0, 0, "Downscale");
        } else if (rto->presetID == 0x04) {
            display.drawString(0, 0, "720x480");
        } else if (rto->presetID == 0x14) {
            display.drawString(0, 0, "768x576");
        } else if (rto->presetID == 0x07) {
            display.drawString(0, 0, "1080p 4:3");
        } else {
            display.drawString(0, 0, "bypass");
        }

        display.drawString(0, 20, String(ofr, 5) + "Hz");

        if (currentInput == 1) {
            display.drawString(0, 41, "RGB");
        } else {
            display.drawString(0, 41, "YpBpR");
        }

        if (currentInput == 1) {
            vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
            if (vsyncActive) {
                display.drawString(70, 41, "V");
                hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
                if (hsyncActive) {
                    display.drawString(53, 41, "H");
                }
            }
        }
    }
    display.display();
    lastUpdateTime = millis();

    return false;
}
bool wifiMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool)
{
    static char ssid[64];
    static char ip[25];
    static char domain[48]; // "http://" + up to 32-char custom hostname + ".local" + NUL
    WiFiMode_t wifiMode = WiFi.getMode();
    manager->clearSubItems(item);
    if (wifiMode == WIFI_STA) {
        sprintf(ssid, "SSID: %s", WiFi.SSID().c_str());
        manager->registerItem(item, 0, ssid);
        if (WiFi.isConnected()) {
            manager->registerItem(item, 0, oledMsg(MSG_STATUS_CONNECTED));
            manager->registerItem(item, 0, oledMsg(MSG_WIFI_URL));
            sprintf(ip, "http://%s", WiFi.localIP().toString().c_str());
            manager->registerItem(item, 0, ip);
            snprintf(domain, sizeof(domain), "http://%s.local", device_hostname.c_str());
            manager->registerItem(item, 0, domain);
        } else {
            // shouldn't happen?
            manager->registerItem(item, 0, oledMsg(MSG_STATUS_DISCONNECTED));
        }
    } else if (wifiMode == WIFI_AP) {
        manager->registerItem(item, 0, oledMsg(MSG_WIFI_AP_INFO));
        sprintf(ssid, "SSID: %s (%s)", ap_ssid, ap_password);
        manager->registerItem(item, 0, ssid);
        manager->registerItem(item, 0, oledMsg(MSG_WIFI_URL));
        manager->registerItem(item, 0, "http://192.168.4.1");
        snprintf(domain, sizeof(domain), "http://%s.local", device_hostname.c_str());
        manager->registerItem(item, 0, domain);
    } else {
        // shouldn't happen?
        manager->registerItem(item, 0, oledMsg(MSG_STATUS_DISCONNECTED));
    }
    return true;
}
bool osdMenuHanlder(OLEDMenuManager *manager, OLEDMenuItem *, OLEDMenuNav nav, bool isFirstTime)
{
    static unsigned long start;
    static long left;
    auto display = manager->getDisplay();

    if (isFirstTime) {
        left = OSD_TIMEOUT;
        start = millis();
        manager->freeze();
        osdManager.tick(OSDNav::ENTER);
    } else {
        display->clear();
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
        // show the selected OSD icon instead of the remaining time
        display->drawString(OLED_MENU_WIDTH / 2, 16, osdManager.selectedName());
        display->display();
        if (uopt->reverseRotaryEncoderForOledMenu) {
            // reverse nav back to normal
            if(nav == OLEDMenuNav::DOWN) {
                nav = OLEDMenuNav::UP;
            } else if(nav == OLEDMenuNav::UP) {
                nav = OLEDMenuNav::DOWN;
            }
        }
        switch (nav) {
            case OLEDMenuNav::ENTER:
                osdManager.tick(OSDNav::ENTER);
                start = millis();
                break;
            case OLEDMenuNav::DOWN:
                if(REVERSE_ROTARY_ENCODER_FOR_OSD) {
                    osdManager.tick(OSDNav::RIGHT);
                } else {
                    osdManager.tick(OSDNav::LEFT);
                }
                start = millis();
                break;
            case OLEDMenuNav::UP:
                if(REVERSE_ROTARY_ENCODER_FOR_OSD) {
                    osdManager.tick(OSDNav::LEFT);
                } else {
                    osdManager.tick(OSDNav::RIGHT);
                }
                start = millis();
                break;
            default:
                break;
        }
        left = OSD_TIMEOUT - (millis() - start);
        if (left <= 0) {
            manager->unfreeze();
            osdManager.menuOff();
        }
    }
    return true;
}

// System menu handler - shows firmware version
bool systemMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool)
{
    manager->clearSubItems(item);
    
    // Show current firmware version
    extern const char* FIRMWARE_VERSION;
    static char versionText[32];
    sprintf(versionText, "Version: %s", FIRMWARE_VERSION);
    manager->registerItem(item, 0, versionText);
    
    // Add "Check for Update" button
    manager->registerItem(item, MT_CHECK_UPDATE, oledMsg(MSG_CHECK_UPDATE), firmwareUpdateMenuHandler);

    // Language submenu: rotate to select Deutsch/English, press to confirm.
    // The choice is saved and the device restarts at once, so the whole menu
    // tree is rebuilt in the new language (initOLEDMenu runs after the
    // preferences are loaded). Default font, like the other System menu items.
    OLEDMenuItem *langMenu = manager->registerItem(item, MT_NULL, oledMsg(MSG_MENU_LANGUAGE));
    // tag 0 = German, tag 1 = English
    manager->registerItem(langMenu, 0, oledMsg(MSG_LANG_GERMAN), languageMenuHandler);
    manager->registerItem(langMenu, 1, oledMsg(MSG_LANG_ENGLISH), languageMenuHandler);
    
    return true;
}

// Language menu handler: item->tag selects the language (0 = German, 1 = English).
// Saving the preference and restarting right away applies it to OLED menu,
// status bar and WebUI (the latter reads the setting via /status/get). Selecting
// the already active language neither saves nor restarts.
bool languageMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        return false;
    }

    uint8_t newLang = item->tag ? 1 : 0;

    // Early out when this language is already active: no need to rewrite the
    // preferences or reboot. oledMenuLanguage() has to match as well, not just
    // uopt->menuLanguage: a change queued from the web UI is already stored in
    // uopt, but the OLED menu only picks it up after a restart, so the restart
    // below is still needed to apply it.
    if (newLang == oledMenuLanguage() && newLang == uopt->menuLanguage) {
        OLEDDisplay *display = manager->getDisplay();
        display->clear();
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
        display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
        display->setFont(ArialMT_Plain_16);
        display->drawString(OLED_MENU_WIDTH / 2, 14, newLang ? "English" : "Deutsch");
        display->setFont(DejaVu_Sans_Mono_12);
        display->drawString(OLED_MENU_WIDTH / 2, 36, oledMsg(MSG_ALREADY_ACTIVE));
        display->display();
        delay(600);
        return false;
    }

    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_16);
    display->drawString(OLED_MENU_WIDTH / 2, 14, newLang ? "English" : "Deutsch");
    display->setFont(DejaVu_Sans_Mono_12);
    display->drawString(OLED_MENU_WIDTH / 2, 36, oledMsg(MSG_REBOOTING));
    display->display();

    uopt->menuLanguage = newLang;
    saveUserPrefs();

    // disconnect the WebSocket clients cleanly, wait a moment, then restart
    webSocket.disconnect();
    delay(400);
    ESP.restart();
    return false;
}

// Firmware update menu handler
bool firmwareUpdateMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime)
{
    static enum UpdateState {
        STATE_CHECKING,
        STATE_SHOW_RESULT,
        STATE_CONFIRM_UPDATE,
        STATE_DOWNLOADING,
        STATE_COMPLETE,
        STATE_ERROR
    } updateState;
    
    static const int RESTART_COUNTDOWN_SECONDS = 5;
    
    static String latestVersion = "";
    static FirmwareUpdater::UpdateStatus updateStatus;
    static int downloadProgress = 0;
    static unsigned long stateStartTime = 0;
    
    OLEDDisplay *display = manager->getDisplay();
    
    if (isFirstTime) {
        updateState = STATE_CHECKING;
        latestVersion = "";
        downloadProgress = 0;
        stateStartTime = millis();
        manager->freeze();
        
        // Close WebSocket and web server to free memory
        extern WebSocketsServer webSocket;
        extern AsyncWebServer server;
        
        Serial.println(F("[Menu] Stopping web services for update..."));
        // Only drop WS clients (closing/rewrapping the server causes rebind issues)
        webSocket.disconnect();
        // HTTP server must be stopped to free resources / avoid handling requests
        // On ESP8266 only - ESP32 has sufficient resources and stopping/restarting causes bind errors
#ifdef ESP8266
        server.end();
#endif
        
        // Clear display to free frame buffer memory
        display->clear();
        display->display();
        
        // Wait longer for connections to close and memory to be freed
        delay(1000);
        yield();
        
        Serial.print(F("[Menu] Free heap after cleanup: "));
        Serial.println(ESP.getFreeHeap());
    }
    
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(DejaVu_Sans_Mono_12);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    
    switch (updateState) {
        case STATE_CHECKING:
            display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_CHECKING));
            display->display();
            
            // Check for update
            updateStatus = FirmwareUpdater::checkForUpdate(latestVersion);
            
            if (updateStatus == FirmwareUpdater::UPDATE_AVAILABLE) {
                updateState = STATE_CONFIRM_UPDATE;
            } else {
                updateState = STATE_SHOW_RESULT;
            }
            stateStartTime = millis();
            break;
            
        case STATE_SHOW_RESULT:
            if (updateStatus == FirmwareUpdater::UP_TO_DATE) {
                // The version is already shown in the menu; a second line here
                // overlapped the text (both were drawn at about the same height).
                display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_UP_TO_DATE));
            } else if (updateStatus == FirmwareUpdater::WIFI_NOT_CONNECTED) {
                display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_WIFI_DISCONNECTED));
            } else {
                display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_CHECK_FAILED));
            }
            display->display();
            
            // Auto-return after 3 seconds
            if (millis() - stateStartTime > 3000 || nav != OLEDMenuNav::IDLE) {
                // Restart web server; WS server stayed up, only clients were dropped
                extern AsyncWebServer server;
                extern runTimeOptions *rto;
                
                if (rto->webServerEnabled) {
#ifdef ESP8266
                    Serial.println(F("[Menu] Restarting web services..."));
                    server.begin();
#endif
                    rto->webServerStarted = true;
                }
                
                manager->unfreeze();
                return false;
            }
            break;
            
        case STATE_CONFIRM_UPDATE:
            display->setFont(DejaVu_Sans_Mono_12);
            display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
            display->drawString(OLED_MENU_WIDTH / 2, 0, oledMsg(MSG_UPDATE_FOUND));
            display->setFont(ArialMT_Plain_10);
            display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_LEFT);
            display->drawString(0, 18, String(oledMsg(MSG_CURRENT_VERSION)) + String(FIRMWARE_VERSION));
            display->drawString(0, 30, String(oledMsg(MSG_LATEST_VERSION)) + latestVersion);
            display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
            display->setFont(ArialMT_Plain_10);
            display->drawString(OLED_MENU_WIDTH / 2, 44, "ENTER = Update");
            display->drawString(OLED_MENU_WIDTH / 2, 54, oledMsg(MSG_DOWN_CANCEL));
            display->display();
            
            if (nav == OLEDMenuNav::ENTER) {
                updateState = STATE_DOWNLOADING;
                stateStartTime = millis();
            } else if (nav == OLEDMenuNav::DOWN) {
                // User canceled: bring web services back up
                extern AsyncWebServer server;
                extern runTimeOptions *rto;
                if (rto->webServerEnabled) {
#ifdef ESP8266
                    Serial.println(F("[Menu] Update canceled, restarting web services..."));
                    server.begin();
#endif
                    rto->webServerStarted = true;
                }
                manager->unfreeze();
                return false;
            }
            break;
            
        case STATE_DOWNLOADING: {
            // Start update if not already started
            if (millis() - stateStartTime < 500) {
                display->setFont(DejaVu_Sans_Mono_12);
                display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
                display->drawString(OLED_MENU_WIDTH / 2, 10, oledMsg(MSG_DOWNLOADING));
                display->setFont(ArialMT_Plain_10);
                
                // Draw initial progress bar
                int barWidth = 100;
                int barHeight = 10;
                int barX = (OLED_MENU_WIDTH - barWidth) / 2;
                int barY = 35;
                display->drawRect(barX, barY, barWidth, barHeight);
                display->drawString(OLED_MENU_WIDTH / 2, 50, "0%");
                display->display();
                break;
            }
            
            if (downloadProgress == 0) {
#ifdef ESP8266
                // ESP8266: the TLS download does not fit into the runtime
                // heap next to the web services (see the block comment in
                // OTAUpdate.cpp). Arm the boot update and restart; the
                // download then runs early in setup() with progress on the
                // OLED, and the device reboots into the new firmware.
                display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
                display->setFont(DejaVu_Sans_Mono_12);
                display->drawString(OLED_MENU_WIDTH / 2, 50, oledMsg(MSG_REBOOTING));
                display->display();
                Serial.println(F("[Menu] Rebooting into boot update"));
                FirmwareUpdater::requestBootUpdate();
                delay(800);
                ESP.restart();
                return false; // not reached
#else
                // Set static pointers for callback (safe during update as only one update at a time)
                static OLEDMenuManager* callbackManager = nullptr;
                static int* callbackProgress = nullptr;
                callbackManager = manager;
                callbackProgress = &downloadProgress;
                
                // Perform update with progress callback that updates display
                updateStatus = FirmwareUpdater::performUpdate([](int progress) {
                    *callbackProgress = progress;
                    
                    // Update display in real-time
                    OLEDDisplay *display = callbackManager->getDisplay();
                    display->clear();
                    display->setColor(OLEDDISPLAY_COLOR::WHITE);
                    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
                    display->setFont(DejaVu_Sans_Mono_12);
                    display->drawString(OLED_MENU_WIDTH / 2, 10, oledMsg(MSG_DOWNLOADING));
                    display->setFont(ArialMT_Plain_10);
                    
                    // Draw progress bar
                    int barWidth = 100;
                    int barHeight = 10;
                    int barX = (OLED_MENU_WIDTH - barWidth) / 2;
                    int barY = 35;
                    display->drawRect(barX, barY, barWidth, barHeight);
                    int fillWidth = (barWidth - 2) * progress / 100;
                    if (fillWidth > 0) {
                        display->fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2);
                    }
                    
                    String progressText = String(progress) + "%";
                    display->drawString(OLED_MENU_WIDTH / 2, 50, progressText);
                    display->display();
                });
                
                if (updateStatus == FirmwareUpdater::SUCCESS) {
                    updateState = STATE_COMPLETE;
                    stateStartTime = millis(); // Start countdown timer
                } else {
                    updateState = STATE_ERROR;
                    stateStartTime = millis();
                }
#endif
            }
            break;
        }
            
        case STATE_COMPLETE: {
            // Calculate remaining seconds
            unsigned long elapsed = millis() - stateStartTime;
            int remainingSeconds = RESTART_COUNTDOWN_SECONDS - (elapsed / 1000);
            
            if (remainingSeconds <= 0) {
                // Time's up - restart now
                display->clear();
                display->setColor(OLEDDISPLAY_COLOR::WHITE);
                display->setFont(DejaVu_Sans_Mono_12);
                display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
                display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_REBOOTING));
                display->display();
                delay(500);
                ESP.restart();
                break;
            }
            
            // Update display with countdown
            display->clear();
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
            display->setFont(DejaVu_Sans_Mono_12);
            display->drawString(OLED_MENU_WIDTH / 2, 0, "Update OK!");
            display->setFont(ArialMT_Plain_10);
            
            char countdownText[40];
            snprintf(countdownText, sizeof(countdownText), oledMsg(MSG_REBOOT_IN), remainingSeconds);
            display->drawString(OLED_MENU_WIDTH / 2, 35, countdownText);
            display->display();
            break;
        }
            
        case STATE_ERROR: {
            display->clear();
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
            display->setFont(DejaVu_Sans_Mono_12);
            
            // Display specific error message based on update status
            switch (updateStatus) {
                case FirmwareUpdater::WIFI_NOT_CONNECTED:
                    display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_WIFI_DISCONNECTED));
                    break;
                case FirmwareUpdater::CHECK_FAILED:
                    display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_CHECK_FAILED));
                    break;
                case FirmwareUpdater::DOWNLOAD_FAILED:
                    display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_DOWNLOAD_FAILED));
                    break;
                case FirmwareUpdater::FLASH_FAILED:
                    display->drawString(OLED_MENU_WIDTH / 2, OLED_MENU_HEIGHT / 2, oledMsg(MSG_FLASH_FAILED));
                    break;
                case FirmwareUpdater::CHECKSUM_FAILED:
                    display->drawString(OLED_MENU_WIDTH / 2, 16, oledMsg(MSG_CHECKSUM_ERROR));
                    display->drawString(OLED_MENU_WIDTH / 2, 31, oledMsg(MSG_SHA256_WRONG));
                    break;
                case FirmwareUpdater::INSUFFICIENT_SPACE:
                    display->drawString(OLED_MENU_WIDTH / 2, 16, oledMsg(MSG_NO_SPACE));
                    display->drawString(OLED_MENU_WIDTH / 2, 31, oledMsg(MSG_FREE_MEM));
                    break;
                default:
                    display->drawString(OLED_MENU_WIDTH / 2, 16, oledMsg(MSG_UPDATE_FAILED));
                    display->drawString(OLED_MENU_WIDTH / 2, 31, oledMsg(MSG_UNKNOWN_ERROR));
                    break;
            }
            display->display();
            
            if (millis() - stateStartTime > 5000 || nav != OLEDMenuNav::IDLE) {
                // Restart web server; WS server stayed up, only clients were dropped
                extern AsyncWebServer server;
                extern runTimeOptions *rto;
                
                if (rto->webServerEnabled) {
#ifdef ESP8266
                    Serial.println(F("[Menu] Restarting web services after failed update..."));
                    server.begin();
#endif
                    rto->webServerStarted = true;
                }
                
                manager->unfreeze();
                return false;
            }
            break;
        }
    }
    
    return true;
}

void initOLEDMenu()
{
    oledStringsInit(); // fix the menu language (preferences are loaded by now)
    OLEDMenuItem *root = oledMenu.rootItem;

    // OSD Menu
    oledMenu.registerItem(root, MT_NULL, oledMsg(MSG_MENU_OSD), osdMenuHanlder, DejaVu_Sans_Mono_10);

    // Resolutions
    OLEDMenuItem *resMenu = oledMenu.registerItem(root, MT_NULL, oledMsg(MSG_MENU_RESOLUTIONS), nullptr, DejaVu_Sans_Mono_10);
    const char *resolutions[7] = {"1280x960", "1280x1024", "1280x720", "1920x1080", "1080p 4:3", "1024x768", "480/576"};
    uint8_t tags[7] = {MT_1280x960, MT1280x1024, MT1280x720, MT1920x1080, MT1080x960W, MT_1024x768, MT_480s576};
    for (int i = 0; i < 7; ++i) {
        oledMenu.registerItem(resMenu, tags[i], resolutions[i], resolutionMenuHandler);
    }
    // downscale and passthrough
    oledMenu.registerItem(resMenu, MT_DOWNSCALE, oledMsg(MSG_MENU_DOWNSCALE), resolutionMenuHandler);
    oledMenu.registerItem(resMenu, MT_BYPASS, oledMsg(MSG_MENU_PASSTHROUGH), resolutionMenuHandler);

    // Presets
    oledMenu.registerItem(root, MT_NULL, oledMsg(MSG_MENU_PRESETS), presetsCreationMenuHandler, DejaVu_Sans_Mono_10);

    // WiFi
    oledMenu.registerItem(root, MT_NULL, oledMsg(MSG_MENU_WIFI), wifiMenuHandler, DejaVu_Sans_Mono_10);

    // Current Settings
    oledMenu.registerItem(root, MT_NULL, oledMsg(MSG_MENU_CURRENT), currentSettingHandler, DejaVu_Sans_Mono_10);

    // System (contains firmware version, update check and language)
    oledMenu.registerItem(root, MT_NULL, oledMsg(MSG_MENU_SYSTEM), systemMenuHandler, DejaVu_Sans_Mono_10);

    // Reset (Misc.)
    OLEDMenuItem *resetMenu = oledMenu.registerItem(root, MT_NULL, oledMsg(MSG_MENU_RESET), nullptr, DejaVu_Sans_Mono_10);
    oledMenu.registerItem(resetMenu, MT_RESET_GBS, oledMsg(MSG_MENU_RESET_GBS), resetMenuHandler, DejaVu_Sans_Mono_10);
    oledMenu.registerItem(resetMenu, MT_RESTORE_FACTORY, oledMsg(MSG_MENU_FACTORY), resetMenuHandler, DejaVu_Sans_Mono_10);
    oledMenu.registerItem(resetMenu, MT_RESET_WIFI, oledMsg(MSG_MENU_RESET_WIFI), resetMenuHandler, DejaVu_Sans_Mono_10);
}
