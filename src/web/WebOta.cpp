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
        state == WEB_OTA_REBOOT_PENDING || state == WEB_OTA_BOOT_UPDATE_PENDING) {
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
        case FirmwareUpdater::HEAP_EXHAUSTED:   // no separate WebUI text; the serial log has details
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

#ifdef ESP8266
    if (isUpdate) {
        // The TLS download does not fit into the runtime heap next to the web
        // services: the BearSSL stack alone needs ~11 KB (6.2 KB stack thunk
        // + 3.4 KB client context + 1.5 KB x509 validator + record buffers)
        // and the updater holds another 4 KB flash sector buffer, but only
        // ~15.7 KB are free even with the services torn down - the device
        // aborted with an OOM exception on the first chunk. Arm the boot
        // update and restart instead; the download runs early in setup()
        // (gbs-control.ino) with the full boot heap. The ESP32 keeps the
        // direct runtime download below.
        // State note: at this point NOTHING has been downloaded or flashed -
        // the report to the UI must be the boot-update-pending state, not the
        // "flashed ok" REBOOT_PENDING (the 2026-11-07 WebUI screenshots showed
        // "Update erfolgreich!" before a single byte had arrived, then the
        // honest "Update läuft ..." after the restart).
        SerialM.println(F("[WebOTA] Rebooting into boot update"));
        webOtaStatus.state = WEB_OTA_BOOT_UPDATE_PENDING;
        FirmwareUpdater::requestBootUpdate();
        // Give the UI time to pick up the BOOT_UPDATE_PENDING state before
        // restart; the device is offline while the boot update downloads.
        delay(5000);
        ESP.restart();
        return;
    }
#endif

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
    // The async TCP stack still serves the browser's open connections from
    // the SYS context for a while after end(): queued websocket frames, TCP
    // close handshakes, the tab's immediate reconnect attempts. All of that
    // churns LWIP pbufs on the very heap the TLS handshake below needs. Give
    // it time to drain before carving the heap up (the 2026-11-08 10:17 log
    // shows what happens without enough of it: soft WDT in SYS context,
    // pbuf_alloc under ethernet_input -> AsyncClient::add -> tcp_write, and
    // the check never returns). delay() services the SYS context.
    delay(2500);
    yield();

#ifdef ESP8266
    if (!isUpdate) {
        // Heap gate for the in-place check. With the video pipeline running
        // there is not enough contiguous heap left for the check's TLS
        // handshake next to the async TCP stack: both in-place checks of
        // 2026-11-08 (10:17 and 11:51, ~14.9-15.0 KB free at this point)
        // died in a soft WDT reset in SYS context, even with the 2.5 s
        // drain time above, and only the crash recovery saved the result.
        // Take that reboot deliberately instead: arm the boot check,
        // restart, and let setup() run the check in the clean boot
        // environment (~22 KB free there, both recovery checks of
        // 2026-11-08 succeeded). The WebUI rides the restart out as a brief
        // connection loss while "checking", then the result lands in the
        // OTA tile as usual.
        static const uint32_t OTA_CHECK_MIN_FREE_HEAP = 18000;
        static const uint32_t OTA_CHECK_MIN_MAX_BLOCK = 12000;
        uint32_t checkFreeHeap = ESP.getFreeHeap();
        uint32_t checkMaxBlock = ESP.getMaxFreeBlockSize();
        if (checkFreeHeap < OTA_CHECK_MIN_FREE_HEAP || checkMaxBlock < OTA_CHECK_MIN_MAX_BLOCK) {
            SerialM.print(F("[WebOTA] Heap too low for an in-place check (free "));
            SerialM.print((unsigned long)checkFreeHeap);
            SerialM.print(F(", max block "));
            SerialM.print((unsigned long)checkMaxBlock);
            SerialM.println(F("), rebooting into the boot check"));
            FirmwareUpdater::armCheckOnBoot();
            // The /ota/check response is long since flushed (4 s of delays
            // above); this margin only covers a slow browser ACK.
            delay(2000);
            ESP.restart();
        }
    }
#endif

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
