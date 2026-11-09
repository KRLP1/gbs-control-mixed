#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>

// GitHub repository configuration
#define GITHUB_REPO_OWNER "KRLP1"
#define GITHUB_REPO_NAME "gbs-control-mixed"
#define GITHUB_API_HOST "api.github.com"
#define GITHUB_API_PATH "/repos/" GITHUB_REPO_OWNER "/" GITHUB_REPO_NAME "/releases/latest"

class FirmwareUpdater {
public:
    enum UpdateStatus {
        UPDATE_AVAILABLE,
        UP_TO_DATE,
        CHECK_FAILED,
        WIFI_NOT_CONNECTED,
        DOWNLOAD_FAILED,
        FLASH_FAILED,
        CHECKSUM_FAILED,
        INSUFFICIENT_SPACE,
        HEAP_EXHAUSTED,
        SUCCESS
    };

    // Check if a newer version is available on GitHub
    // Returns status and populates latestVersion if update available
    static UpdateStatus checkForUpdate(String& latestVersion);

    // Perform the firmware update with progress callback
    // progressCallback receives percentage (0-100)
    static UpdateStatus performUpdate(void (*progressCallback)(int) = nullptr);

    // Combined check and update operation
    static UpdateStatus checkAndUpdate(void (*progressCallback)(int) = nullptr);

    // Boot-time update mode (ESP8266 only). The runtime heap cannot hold the
    // BearSSL TLS stack, the updater flash buffer and the web services at the
    // same time, so an update request arms a record in RTC user memory and
    // restarts; the download then runs early in setup() with the full boot
    // heap (see the block comment in OTAUpdate.cpp). The record carries the
    // release URL / SHA256 / size from the check that prompted the request,
    // so the boot update can go straight to the download without the
    // heap-fragmenting API call. The record is consumed on the first read,
    // so a crash during the download can never cause a reboot loop.
    static void requestBootUpdate();   // arm the record (survives the restart)
    static bool bootUpdatePending();   // true exactly once after that restart
    static void clearBootUpdate();     // disarm the record
    static UpdateStatus runBootUpdate(void (*progressCallback)(int) = nullptr);

    // Interrupted-check recovery (ESP8266 only, always false on ESP32). The
    // check arms an RTC flag for its duration; if the device crashes mid-
    // check (soft WDT in the async/SYS context on a slow GitHub day), the
    // flag survives the reset and the early setup() re-runs the check in the
    // clean boot environment. True exactly once per interrupted check, at
    // most twice in a row - after that the user retries manually.
    static bool checkInterruptedPending();

    // Planned boot check (ESP8266 only, no-op on ESP32). Arms the same RTC
    // flag the crash recovery consumes, but on purpose: when the runtime
    // heap is too low for the check's TLS handshake (measured 2026-11-08:
    // ~14.9-15.0 KB free with the video pipeline running -> soft WDT on
    // both in-place checks of that day; the boot environment has ~22 KB),
    // webOtaProcess() calls this and restarts instead of gambling on the
    // in-place check. setup() then runs the check in the clean boot
    // environment and publishes the result to the WebUI tile.
    static void armCheckOnBoot();

private:
    static String _latestVersion;
    static String _downloadUrl;
    static String _assetApiUrl;
    static String _expectedSha256;
    static long _firmwareSize;
    static bool _updateChecked;

    // Parse GitHub API JSON response from stream
    static bool parseReleaseInfo(Stream& stream, String& version, String& downloadUrl, String& expectedSha256);
    
    // Helper to read JSON string value from stream
    static String readJsonString(Stream& stream);
};

#endif // OTA_UPDATE_H
