#ifndef USER_COMMAND_HANDLER_H
#define USER_COMMAND_HANDLER_H

#include <Arduino.h>
#include "../core/State.h"

// User command processing (from web UI)
void handleType2Command(char argument);

// Pending values from the value queue (userValueQueue), read by the 'Y'/'y'
// command cases; 255 = no value queued -> the command is ignored.
extern uint8_t pendingSharpnessValue;
extern uint8_t pendingScanlineStrength;

#endif // USER_COMMAND_HANDLER_H

