#include "WebSocketHandler.h"
#include "../core/Globals.h"
#include "../core/options.h"

void updateWebSocketData()
{
    if (rto->webServerEnabled && rto->webServerStarted) {
        if (webSocket.connectedClients() > 0) {

            constexpr size_t MESSAGE_LEN = 7;
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

            // Peaking level (0-3) in bits 0-1 of byte 6.
            // '@' = 0x40 als "byte ist belegt"-Marker (analog zu bytes 3-5),
            // um ungenutzte Bits auf einen Wert > 0x20 zu halten.
            // Nur melden, wenn Peaking per Kantenanhebung-Taste ('f') auch wirklich aktiv ist -
            // sonst wuerde die Peaking-Stufe-Taste/der Fuellbalken einen Wert > 0 anzeigen,
            // waehrend der Chip gerade bypassed ist (uopt->peakingLevel bleibt beim Ausschalten
            // bewusst erhalten, damit 'f' die zuletzt gewaehlte Stufe wieder aufgreift).
            toSend[6] = '@';
            toSend[6] |= (uopt->wantPeaking ? (uopt->peakingLevel & 0x03) : 0);

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

