#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <Arduino.h>
#include "WebSocketsServer.h" // WStype_t fuer die Handler-Deklarationen

// WebSocket data update
void updateWebSocketData();

// WS-Events + App-Level-Heartbeat (ASYNC-Modus kompiliert den
// Bibliotheks-Heartbeat auf ESP8266 weg, siehe WebSocketsServer.cpp:635)
void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void webSocketHeartbeat();

#endif // WEBSOCKET_HANDLER_H
