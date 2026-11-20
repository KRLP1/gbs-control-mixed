#include "WebSocketHandler.h"
#include "../core/Globals.h"
#include "../core/options.h"
#include "../video/Sharpness.h"

void updateWebSocketData()
{
    if (rto->webServerEnabled && rto->webServerStarted) {
        if (webSocket.connectedClients() > 0) {

            constexpr size_t MESSAGE_LEN = 9;
            char toSend[MESSAGE_LEN] = {0};
            toSend[0] = '#'; // makeshift ping in slot 0

            if (rto->isCustomPreset) {
                toSend[1] = '9';
            } else switch (rto->presetID) {
                case 0x01:
                case 0x11:
                    toSend[1] = '1';
                    break;
                case 0x02:
                case 0x12:
                    toSend[1] = '2';
                    break;
                case 0x03:
                case 0x13:
                    toSend[1] = '3';
                    break;
                case 0x04:
                case 0x14:
                    toSend[1] = '4';
                    break;
                case 0x05:
                case 0x15:
                    toSend[1] = '5';
                    break;
                case 0x06:
                case 0x16:
                    toSend[1] = '6';
                    break;
                case 0x07: // 1080p 4:3 windowbox (NTSC + PAL share this ID)
                    toSend[1] = '7';
                    break;
                case 0x08:
                case 0x18:
                    toSend[1] = 'A';
                    break;
                case PresetHdBypass: // bypass 1
                case PresetBypassRGBHV: // bypass 2
                    toSend[1] = '8';
                    break;
                default:
                    toSend[1] = '0';
                    break;
            }

            toSend[2] = (char)uopt->presetSlot;

            // '@' = 0x40, used for "byte is present" detection; 0x80 not in ascii table
            toSend[3] = '@';
            toSend[4] = '@';
            toSend[5] = '@';

            if (uopt->enableAutoGain) {
                toSend[3] |= (1 << 0);
            }
            if (uopt->wantScanlines) {
                toSend[3] |= (1 << 1);
            }
            if (uopt->wantVdsLineFilter) {
                toSend[3] |= (1 << 2);
            }
            if (uopt->wantPeaking) {
                toSend[3] |= (1 << 3);
            }
            if (uopt->PalForce60) {
                toSend[3] |= (1 << 4);
            }
            if (uopt->wantOutputComponent) {
                toSend[3] |= (1 << 5);
            }

            if (uopt->matchPresetSource) {
                toSend[4] |= (1 << 0);
            }
            if (uopt->enableFrameTimeLock) {
                toSend[4] |= (1 << 1);
            }
            if (uopt->deintMode) {
                toSend[4] |= (1 << 2);
            }
            if (uopt->wantTap6) {
                toSend[4] |= (1 << 3);
            }
            if (uopt->wantStepResponse) {
                toSend[4] |= (1 << 4);
            }
            if (uopt->wantFullHeight) {
                toSend[4] |= (1 << 5);
            }

            if (uopt->enableCalibrationADC) {
                toSend[5] |= (1 << 0);
            }
            if (uopt->preferScalingRgbhv) {
                toSend[5] |= (1 << 1);
            }
            if (uopt->disableExternalClockGenerator) {
                toSend[5] |= (1 << 2);
            }
            if (uopt->reverseRotaryEncoderForOledMenu) {
                toSend[5] |= (1 << 3);
            }
            if (uopt->frameTimeLockMethod == 1) {
                toSend[5] |= (1 << 4);
            }

            // Byte 6: '@' = 0x40 as the "byte is present" marker (like bytes 3-5), which
            // keeps the unused bits at a value above 0x20.
            //   bits 0-1: peaking level 0-3 (derived from uopt->picSharpness, 0 = off)
            //   bit 2:    scanline strength bit 0x40. The strength 0x00..0x60 does not fit
            //             into one ASCII char ('@' + 0x60 > 0x7F), so its lower 6 bits are
            //             in byte 8 and this bit carries the rest.
            toSend[6] = '@';
            toSend[6] |= (uopt->wantPeaking ? (uopt->peakingLevel & 0x03) : 0);
            if (uopt->scanlineStrength & 0x40) {
                toSend[6] |= (1 << 2);
            }

            // Byte 7: sharpness 0..32 (32 = max), '@' + value, so the byte stays above
            // 0x20 and ASCII. Sent while peaking is off as well (then 0 = soft).
            // Byte 8: scanline strength lower 6 bits, '@' + (strength & 0x3F). The real
            // stored strength is sent whether scanlines are on or off, so the WebUI
            // shows what a + / - press starts from.
            toSend[7] = (char)('@' + (uopt->picSharpness > SHARPNESS_MAX_STEP ? SHARPNESS_MAX_STEP : uopt->picSharpness));
            toSend[8] = (char)('@' + (uopt->scanlineStrength & 0x3F));

            // send ping and stats
            if (ESP.getFreeHeap() > 6000) {
                webSocket.broadcastTXT(toSend, MESSAGE_LEN);
            }
        }
    }
}
// Letzter Pong-Empfang pro Client-Slot (WEBSOCKETS_SERVER_CLIENT_MAX = 2).
// Handler laeuft im Async-Kontext: nur 32-Bit-Store, auf Xtensa atomar.
static uint32_t wsLastPong[WEBSOCKETS_SERVER_CLIENT_MAX] = {0};

void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    if (num >= WEBSOCKETS_SERVER_CLIENT_MAX) {
        return;
    }
    if (type == WStype_CONNECTED) {
        wsLastPong[num] = millis(); // Start der Toleranzzeit
    } else if (type == WStype_PONG) {
        wsLastPong[num] = millis(); // Browser antworten protokollseitig (RFC 6455)
    } else if (type == WStype_TEXT && length == 2 && (payload[0] == 's' || payload[0] == 'u')) {
        // Kommandos ueber den offenen WebSocket ("s<cmd>" = serialCommand, "u<cmd>" =
        // userCommand): spart pro Tastendruck einen TCP-Handshake + HTTP-Request
        // (lwIP-Slots, Heap). Die WebUI faellt auf /sc?//uc? (HTTP) zurueck, solange
        // der Socket zu ist. Gleiche Queues wie die HTTP-Handler; SPSC + Fences,
        // Warnung via deferLog, weil dieser Callback im Async-Kontext laeuft.
        const bool queued = (payload[0] == 's') ? serialCommandQueue.push((char)payload[1])
                                                : userCommandQueue.push((char)payload[1]);
        if (!queued) {
            SerialM.deferLog(F("Warning: ws command queue full, dropping\n"));
        }
    } else if (type == WStype_TEXT && length >= 3 && length <= 8 &&
               (payload[0] == 'Y' || payload[0] == 'y') && payload[1] == '=') {
        // Value commands over the websocket ("Y=<0..32>" decimal / "y=<hex>" with the
        // same 0x7f off marker as the HTTP endpoint), pushed into userValueQueue
        // (SPSC + fences, see ValueCommandQueue.h). Wire format: "Y=" / "y=" followed
        // by the ASCII digits of the value, parsed here with the SAME base and range
        // validation as the /gbs/set/* HTTP endpoints, so both paths behave
        // identically. '=' as the separator keeps the frame clearly distinct from
        // the 2-byte s/u commands and from anything the value digits could produce.
        // No free-heap gate on purpose: one small TCP frame instead of a full HTTP
        // request, and it keeps working when the HTTP guards stand down below 10k.
        char valBuf[7];
        memcpy(valBuf, &payload[2], length - 2);
        valBuf[length - 2] = '\0';
        const bool isSharpness = (payload[0] == 'Y');
        char *end = nullptr;
        const long parsed = strtol(valBuf, &end, isSharpness ? 10 : 16);
        bool valid = (end != nullptr && *end == '\0');
        if (isSharpness) {
            valid = valid && parsed >= 0 && parsed <= 32;
        } else {
            valid = valid && parsed >= 0 && (parsed <= 0x60 || parsed == 0x7f);
        }
        const bool queued = valid && userValueQueue.push((char)payload[0], (uint8_t)parsed);
        if (!queued) {
            SerialM.deferLog(F("Warning: ws value command dropped (invalid or queue full)\n"));
        }
    }
}

// Alle 2s pingen, nach >6s ohne Pong disconnectieren (toter Client).
// Aufruf aus handleWiFi() = loop()-Kontext.
void webSocketHeartbeat()
{
    static uint32_t lastTick = 0;
    uint32_t now = millis();
    if (now - lastTick < 2000) {
        return;
    }
    lastTick = now;

    for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (!webSocket.clientIsConnected(i)) {
            continue;
        }
        if (wsLastPong[i] != 0 && (now - wsLastPong[i]) > 6000) {
            webSocket.disconnect(i); // Zombie: Slot freigeben
            continue;
        }
        webSocket.sendPing(i);
    }
}
