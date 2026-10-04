#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>

// WiFi configuration
#define THIS_DEVICE_MASTER

// External declarations - definitions are in WiFiConfig.cpp
extern const char *ap_ssid;
extern const char *ap_password;
extern const char *device_hostname_full;
extern const char *device_hostname_partial;
extern String device_hostname; // runtime hostname, loadable from /hostname.txt

// Backported from JuergenLeber/gbs-control: WiFi credentials received via /wifi/connect, applied
// and persisted in the main loop (UserCommandHandler.cpp, case 'u') instead of directly inside the
// AsyncWebServer request callback - see WebServer.cpp for why.
extern String pendingWifiSSID;
extern String pendingWifiPassword;

extern const char ap_info_string[] PROGMEM;
extern const char st_info_string[] PROGMEM;

#endif // WIFI_CONFIG_H

