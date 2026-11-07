#include "WebOta.h"
#include "../menu/OTAUpdate.h"
#include "../core/Globals.h"
#include "../config/Config.h"

WebOtaStatus webOtaStatus = { WEB_OTA_IDLE, 0, WEB_OTA_ERR_NONE, "" };
static volatile uint8_t webOtaAction = WEB_OTA_ACTION_NONE;

bool webOtaRequest(uint8_t action)
{
    // One pending request at a time; refuse while any OTA work is active or
    // the device is already waiting for its reboot.
    if (webOtaAction != WEB_OTA_ACTION_NONE) {
        return false;
    }
    uint8_t state = webOtaStatus.state;
    if (state == WEB_OTA_CHECKING || state == WEB_OTA_DOWNLOADING ||
        state == WEB_OTA_REBOOT_PENDING) {
        return false;
    }
    webOtaAction = action;
    return true;
}

// Map the FirmwareUpdater status to a WebOtaError code (not by enum value, so
// the web UI protocol does not depend on the order of FirmwareUpdater::UpdateStatus).
static void webOtaSetResult(FirmwareUpdater::UpdateStatus status)
{
    uint8_t err;
    switch (status) {
        case FirmwareUpdater::WIFI_NOT_CONNECTED:
            err = WEB_OTA_ERR_WIFI;
            break;
        case FirmwareUpdater::CHECK_FAILED:
            err = WEB_OTA_ERR_CHECK;
            break;
        case FirmwareUpdater::DOWNLOAD_FAILED:
            err = WEB_OTA_ERR_DOWNLOAD;
            break;
        case FirmwareUpdater::FLASH_FAILED:
            err = WEB_OTA_ERR_FLASH;
            break;
        case FirmwareUpdater::CHECKSUM_FAILED:
            err = WEB_OTA_ERR_CHECKSUM;
            break;
        case FirmwareUpdater::INSUFFICIENT_SPACE:
            err = WEB_OTA_ERR_SPACE;
            break;
        default:
            err = WEB_OTA_ERR_UNKNOWN;
            break;
    }
    webOtaStatus.err = err;
    webOtaStatus.state = WEB_OTA_ERROR; // last: err is valid once the state shows ERROR
}

void webOtaProcess()
{
    if (webOtaAction == WEB_OTA_ACTION_NONE) {
        return;
    }
    uint8_t action = webOtaAction;
    webOtaAction = WEB_OTA_ACTION_NONE;

#if USE_NEW_OLED_MENU
    // The OLED update screen drives FirmwareUpdater on its own; refuse web
    // requests while that (or any other frozen custom screen) is active.
    if (oledMenu.isFrozen()) {
        webOtaStatus.err = WEB_OTA_ERR_OLED_BUSY;
        webOtaStatus.state = WEB_OTA_ERROR;
        SerialM.deferLog(F("[WebOTA] rejected: OLED menu is active\n"));
        return;
    }
#endif

    bool isUpdate = (action == WEB_OTA_ACTION_UPDATE);
    webOtaStatus.state = isUpdate ? WEB_OTA_DOWNLOADING : WEB_OTA_CHECKING;
    webOtaStatus.progress = 0;
    webOtaStatus.err = WEB_OTA_ERR_NONE;
    webOtaStatus.latestVersion[0] = '\0';

    SerialM.println(isUpdate ? F("[WebOTA] update requested") : F("[WebOTA] check requested"));

    // Let the HTTP response for the triggering request flush before the
    // async server goes away (delay() services the SYS context on ESP8266).
    delay(1500);

    // Same teardown as the OLED update flow: drop websocket clients and, on
    // ESP8266, stop the HTTP server to free heap for the SSL handshake.
    // ESP32 keeps the server up (stopping/restarting causes bind errors).
    webSocket.disconnect();
#ifdef ESP8266
    server.end();
#endif
    delay(1000);
    yield();

    FirmwareUpdater::UpdateStatus result;

    if (isUpdate) {
        result = FirmwareUpdater::performUpdate([](int progress) {
            // Progress is only observable on ESP32, where the async server
            // keeps answering /ota/status while loop() is blocked here.
            webOtaStatus.progress = (uint8_t)progress;
        });

        if (result == FirmwareUpdater::SUCCESS) {
            SerialM.println(F("[WebOTA] update ok, rebooting"));
            webOtaStatus.state = WEB_OTA_REBOOT_PENDING;
            // Bring the web server back so the UI can show the reboot notice,
            // then restart (same countdown length as the OLED flow).
#ifdef ESP8266
            if (rto->webServerEnabled) {
                server.begin();
                rto->webServerStarted = true;
            }
#endif
            delay(5000);
            ESP.restart();
            return;
        }

        webOtaSetResult(result);
        SerialM.println(F("[WebOTA] update failed"));
    } else {
        String latestVersion = "";
        result = FirmwareUpdater::checkForUpdate(latestVersion);

        if (result == FirmwareUpdater::UPDATE_AVAILABLE) {
            strncpy(webOtaStatus.latestVersion, latestVersion.c_str(),
                    sizeof(webOtaStatus.latestVersion) - 1);
            webOtaStatus.latestVersion[sizeof(webOtaStatus.latestVersion) - 1] = '\0';
            webOtaStatus.state = WEB_OTA_UPDATE_AVAILABLE;
        } else if (result == FirmwareUpdater::UP_TO_DATE) {
            // Also report the found version when "up to date": the WebUI uses
            // it to fill the "Available:" tile. Without this, the tile keeps
            // showing "-" and the check looks like nothing happened.
            strncpy(webOtaStatus.latestVersion, latestVersion.c_str(),
                    sizeof(webOtaStatus.latestVersion) - 1);
            webOtaStatus.latestVersion[sizeof(webOtaStatus.latestVersion) - 1] = '\0';
            webOtaStatus.state = WEB_OTA_UP_TO_DATE;
        } else {
            webOtaSetResult(result);
        }
    }

    // Bring the web services back up (nothing to do on ESP32, server never
    // stopped). webServerStarted stays in sync like in the OLED flow.
#ifdef ESP8266
    if (rto->webServerEnabled) {
        server.begin();
        rto->webServerStarted = true;
    }
#endif
}
