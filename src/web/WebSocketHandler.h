#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <Arduino.h>
#include "WebSocketsServer.h" // WStype_t for the handler declarations

// WebSocket data update
void updateWebSocketData();

// WS events + app-level heartbeat (ASYNC mode compiles the library
// heartbeat out on ESP8266, see WebSocketsServer.cpp:635)
void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void webSocketHeartbeat();

#endif // WEBSOCKET_HANDLER_H
