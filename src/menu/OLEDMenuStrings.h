#ifndef OLED_MENU_STRINGS_H_
#define OLED_MENU_STRINGS_H_
#include <stdint.h>

// Two-language texts for the OLED menu and the OSD icon names.
//
// Language 0 = German (device default, also used when preferences field #22 is
// missing), 1 = English. The language is read from uopt->menuLanguage once, when
// the menu tree is built (initOLEDMenu), and stays fixed until the next restart:
// the menu items only store pointers to their texts, so changing the preference
// at runtime takes effect after a restart.
//
// Both languages are stored in flash (PROGMEM); only the texts of the active
// language are copied to RAM, so the RAM use is the same as with the former
// German-only string literals (the ESP8266 keeps plain string literals in RAM).

// One entry per text: X(id, German, English). The enum, the German and the English
// text blocks are all generated from this single list, so they cannot get out of
// order. A text may contain printf conversions (MSG_REBOOT_IN) but no NUL characters.
#define OLED_MSG_LIST(X) \
    /* Menu tree (initOLEDMenu) */ \
    X(MSG_MENU_OSD, "OSD-Menü öffnen", "Open OSD Menu") \
    X(MSG_MENU_RESOLUTIONS, "Auflösungen", "Resolutions") \
    X(MSG_MENU_DOWNSCALE, "Verkleinern", "Down-Scale") \
    X(MSG_MENU_PASSTHROUGH, "Durchleitung", "Passthrough") \
    X(MSG_MENU_PRESETS, "Presets", "Presets") \
    X(MSG_MENU_WIFI, "WLAN-Info", "WiFi Info") \
    X(MSG_MENU_CURRENT, "Aktuelle Ausgabe", "Current Output") \
    X(MSG_MENU_SYSTEM, "System", "System") \
    X(MSG_MENU_RESET, "Zurücksetzen", "Reset/Restore") \
    X(MSG_MENU_RESET_GBS, "GBS zurücksetzen", "Reset GBS") \
    X(MSG_MENU_FACTORY, "Werkseinstellungen", "Restore Factory") \
    X(MSG_MENU_RESET_WIFI, "WLAN-Verbindungen löschen", "Clear WiFi Connections") \
    X(MSG_MENU_LANGUAGE, "Sprache / Language", "Sprache / Language") \
    X(MSG_LANG_GERMAN, "Deutsch", "Deutsch") \
    X(MSG_LANG_ENGLISH, "English", "English") \
    X(MSG_ALREADY_ACTIVE, "Bereits aktiv", "Already active") \
    /* Handler messages */ \
    X(MSG_LOADED, "Geladen", "Loaded") \
    X(MSG_TOO_MANY_PRESETS, "Bitte WebUI nutzen, um auf weitere Presets zuzugreifen.", "Please use the Web UI to access more presets.") \
    X(MSG_NO_PRESETS, "Keine Presets. Bitte zuerst über die Web-UI eins anlegen.", "No presets. Please create one in the web UI first.") \
    X(MSG_RESETTING_GBS, "GBS zurücksetzen", "Resetting GBS") \
    X(MSG_RESETTING_FACTORY, "Werksreset", "Factory Restore") \
    X(MSG_RESETTING_WIFI, "WLAN zurücksetzen", "Resetting WiFi") \
    X(MSG_PLEASE_WAIT, "Bitte warten", "Please wait") \
    X(MSG_NO_INPUT, "Kein Signal", "No Input") \
    X(MSG_STATUS_CONNECTED, "Status: Verbunden", "Status: Connected") \
    X(MSG_STATUS_DISCONNECTED, "Status: Getrennt", "Status: Disconnected") \
    X(MSG_WIFI_URL, "Für die Web-UI eine der folgenden URLs verwenden", "Use one of the following URLs for the web UI") \
    X(MSG_WIFI_AP_INFO, "Vor Nutzung der Web-UI mit folgender SSID (Passwort) verbinden", "Connect to the following SSID (password) before using the web UI") \
    X(MSG_CHECK_UPDATE, "Nach Update suchen", "Check for Update") \
    X(MSG_CHECKING, "Prüfe...", "Checking...") \
    X(MSG_UP_TO_DATE, "Aktuell!", "Up to date!") \
    X(MSG_WIFI_DISCONNECTED, "WLAN getrennt", "WiFi not connected") \
    X(MSG_CHECK_FAILED, "Prüfung fehlg.", "Check failed") \
    X(MSG_UPDATE_FOUND, "Update gefunden!", "Update found!") \
    X(MSG_CURRENT_VERSION, "Aktuell: ", "Current: ") \
    X(MSG_LATEST_VERSION, "Neueste: ", "Latest: ") \
    X(MSG_DOWN_CANCEL, "DOWN = Abbrechen", "DOWN = Cancel") \
    X(MSG_DOWNLOADING, "Lade herunter...", "Downloading...") \
    X(MSG_REBOOTING, "Neustart...", "Rebooting...") \
    X(MSG_REBOOT_IN, "Neustart in %d...", "Rebooting in %d...") \
    X(MSG_DOWNLOAD_FAILED, "Download fehlg.", "Download failed") \
    X(MSG_FLASH_FAILED, "Flash fehlg.", "Flash failed") \
    X(MSG_CHECKSUM_ERROR, "Prüfsummenfehler", "Checksum error") \
    X(MSG_SHA256_WRONG, "SHA256 falsch", "SHA256 wrong") \
    X(MSG_NO_SPACE, "Nicht genug Platz", "Not enough space") \
    X(MSG_FREE_MEM, "Freier Speicher", "Free memory") \
    X(MSG_UPDATE_FAILED, "Update fehlg.", "Update failed") \
    X(MSG_UNKNOWN_ERROR, "Unbek. Fehler", "Unknown error") \
    /* OSD icon names (MSG_OSD_BRIGHTNESS + n, same order as OSDIcon) */ \
    X(MSG_OSD_BRIGHTNESS, "Helligkeit", "Brightness") \
    X(MSG_OSD_CONTRAST, "Kontrast", "Contrast") \
    X(MSG_OSD_SCANLINES, "Scanlines", "Scanlines") \
    X(MSG_OSD_LINE_FILTER, "Zeilenfilter", "Line filter") \
    X(MSG_OSD_POS_Y, "Position Y", "Position Y") \
    X(MSG_OSD_POS_X, "Position X", "Position X") \
    X(MSG_OSD_SCALE_Y, "Skalierung Y", "Scale Y") \
    X(MSG_OSD_SCALE_X, "Skalierung X", "Scale X")

enum OLEDMsgId : uint8_t {
#define OLED_MSG_ENUM(id, de, en) id,
    OLED_MSG_LIST(OLED_MSG_ENUM)
#undef OLED_MSG_ENUM
    MSG_COUNT
};

// Selects the language and loads its texts into RAM. Called from initOLEDMenu(),
// i.e. after the user preferences were read. Only the first call has an effect
// and it pins the language for the whole run: never call oledMsg() or this from
// setup() before the preferences are loaded. All current callers run from loop()
// or later; an early call would pin German even when English is stored.
void oledStringsInit();

// Language the menu was set up with (0 = German, 1 = English); also selects the
// language of the status bar / screen saver bitmaps.
uint8_t oledMenuLanguage();

// Text for the id in the active language. The returned pointer stays valid for
// the whole run time. Invalid ids give "" (drawString() with "" is harmless).
const char *oledMsg(uint8_t id);

#endif // OLED_MENU_STRINGS_H_