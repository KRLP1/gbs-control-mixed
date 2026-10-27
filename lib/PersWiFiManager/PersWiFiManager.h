/* PersWiFiManager
   version 3.0.1
   https://r-downing.github.io/PersWiFiManager/

   modified for inclusion in gbs-control
   see /3rdparty/PersWiFiManager/ for original code and license
*/
#ifndef PERSWIFIMANAGER_H
#define PERSWIFIMANAGER_H

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

#define WIFI_CONNECT_TIMEOUT 45

// AP fallback retry (see PersWiFiManager::handleApRetry()): when the stored
// network could not be reached and the device sits in AP mode, try the stored
// network again after WIFI_AP_RETRY_INTERVAL seconds without any AP client,
// for at most WIFI_AP_RETRY_CONNECT_TIMEOUT seconds per attempt.
#define WIFI_AP_RETRY_INTERVAL 60
#define WIFI_AP_RETRY_CONNECT_TIMEOUT 15

class PersWiFiManager
{

public:
    typedef std::function<void(void)> WiFiChangeHandlerFunction;

    PersWiFiManager(AsyncWebServer &s, DNSServer &d);

    bool attemptConnection(const String &ssid = "", const String &pass = "");

    void setupWiFiHandlers();

    bool begin(const String &ssid = "", const String &pass = "");

    String getApSsid();

    void setApCredentials(const String &apSsid, const String &apPass = "");

    void setConnectNonBlock(bool b);

    void handleWiFi();

    /// AP fallback retry. Must only be called from the top level of loop(),
    /// never from inside video routines: starting or ending a retry switches
    /// the WiFi mode and blocks for a few hundred milliseconds.
    void handleApRetry();

    /// Drop the RAM copy of the stored credentials and stop retrying
    /// (call after the stored credentials were erased on purpose).
    void forgetStaCredentials();

    void startApMode();

    void onConnect(WiFiChangeHandlerFunction fn);

    void onAp(WiFiChangeHandlerFunction fn);

private:
    AsyncWebServer *_server;
    DNSServer *_dnsServer;
    String _apSsid, _apPass;

    bool _connectNonBlock;
    unsigned long _connectStartTime;

    WiFiChangeHandlerFunction _connectHandler;
    WiFiChangeHandlerFunction _apHandler;

    unsigned long _lastConnectionCheck;
    unsigned long _connectionLostTime;

    // AP fallback retry state
    void startStaRetry();
    unsigned long _connectTimeoutMs;
    bool _apFallback;           // AP mode was entered because the stored network could not be reached
    bool _retrying;             // the current connect attempt is an AP fallback retry
    unsigned long _apIdleSince; // AP start, or last time an AP client was seen
    String _staSsid, _staPass;  // RAM copy of the stored credentials (ESP8266)

}; //class

#endif
