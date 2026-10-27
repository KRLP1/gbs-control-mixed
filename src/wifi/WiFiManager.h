#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// WiFi management
void handleWiFi(boolean instant);

// AP fallback retry, call from the top level of loop() only (see PersWiFiManager::handleApRetry()).
void handleWiFiRetry();

// Forget the RAM copy of the stored WiFi credentials (call after they were erased on purpose).
void wifiForgetRetryCredentials();

#endif // WIFI_MANAGER_H

