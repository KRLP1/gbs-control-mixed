#ifndef WEB_OTA_H
#define WEB_OTA_H

#include <Arduino.h>

// States reported by /ota/status (must stay in sync with the web UI)
enum WebOtaState : uint8_t {
    WEB_OTA_IDLE = 0,             // no result yet (fresh boot)
    WEB_OTA_CHECKING = 1,         // GitHub check in progress
    WEB_OTA_UP_TO_DATE = 2,       // last check: already latest version
    WEB_OTA_UPDATE_AVAILABLE = 3, // last check: newer release found
    WEB_OTA_DOWNLOADING = 4,      // firmware download/flash in progress
    WEB_OTA_REBOOT_PENDING = 5,   // flash ok, device is about to restart
    WEB_OTA_ERROR = 6,            // last operation failed (see message)
    WEB_OTA_BOOT_UPDATE_PENDING = 7 // update accepted; the device restarts
                                  // FIRST and downloads/flashes afterwards
                                  // in the boot updater (ESP8266). Nothing is
                                  // written yet when this state is reported,
                                  // so the UI must not claim success here
};

// Failure reasons reported by /ota/status in the "err" field (valid while the
// state is WEB_OTA_ERROR). The web UI maps them to translated texts, so the
// device does not send any language-dependent text. Must stay in sync with the
// web UI (otaErrorKey in index.ts).
enum WebOtaError : uint8_t {
    WEB_OTA_ERR_NONE = 0,
    WEB_OTA_ERR_WIFI = 1,      // not connected to a network
    WEB_OTA_ERR_CHECK = 2,     // GitHub check failed (also: update without check)
    WEB_OTA_ERR_DOWNLOAD = 3,  // download failed or incomplete
    WEB_OTA_ERR_FLASH = 4,     // writing the firmware failed
    WEB_OTA_ERR_CHECKSUM = 5,  // SHA256 mismatch
    WEB_OTA_ERR_SPACE = 6,     // not enough free flash space
    WEB_OTA_ERR_OLED_BUSY = 7, // OLED menu owns the display (update screen)
    WEB_OTA_ERR_UNKNOWN = 8
};

// Requested action: latched from the AsyncWebServer context (webOtaRequest),
// consumed and executed in loop() context (webOtaProcess)
enum WebOtaAction : uint8_t {
    WEB_OTA_ACTION_NONE = 0,
    WEB_OTA_ACTION_CHECK = 1,
    WEB_OTA_ACTION_UPDATE = 2
};

// Shared status block. state/err/progress are volatile uint8_t (atomic reads
// from the async web task). latestVersion is a fixed-size buffer written by
// webOtaProcess() *before* state is updated, so a reader that observes a new
// state also sees the completed string.
struct WebOtaStatus {
    volatile uint8_t state;    // WebOtaState
    volatile uint8_t progress; // 0-100, meaningful while DOWNLOADING
    volatile uint8_t err;      // WebOtaError, meaningful in state ERROR
    char latestVersion[16];    // last release tag seen ("" = none)
};

extern WebOtaStatus webOtaStatus;

// Called from AsyncWebServer request handlers (async context). Only latches
// the requested action and returns true/false; no blocking work in here.
bool webOtaRequest(uint8_t action);

// Called from loop() context: executes a latched request end to end
// (web services are suspended while the updater runs, same as the OLED
// update flow). No-op when nothing was requested.
void webOtaProcess();

#endif // WEB_OTA_H
