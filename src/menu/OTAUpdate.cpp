#include "OTAUpdate.h"
#ifdef ESP32
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <BearSSLHelpers.h>
#include <Updater.h>
#include <bearssl/bearssl_hash.h>
#include <bearssl/bearssl_ssl.h>        // BR_ERR_TOO_LARGE (record > input buffer)
#include <StackThunk.h>
#include <umm_malloc/umm_heap_select.h> // HeapSelectIram/Dram: probe mirrors the core
#include <flash_hal.h>                  // FS_start: update partition address (resume)
#endif
#include <time.h>
#include <utility>                     // std::move: resolved URL handover below

// ---------------------------------------------------------------------------
// How the download works (and why the buffers are sized this way)
//
// The ESP8266 BearSSL client offers the TLS maximum fragment length
// negotiation (MFLN, RFC 6066) derived from BOTH buffers passed to
// setBufferSizes(); with the 512-byte xmit buffer used here it always
// offers max_fragment_length = 512. That was verified against the live
// hosts with the exact BearSSL stack of the ESP8266 core (Nov 2026,
// TLS 1.2):
//   * api.github.com and github.com CONFIRM the negotiation in their
//     ServerHello. EVERY record - including the whole ~5.6 KB redirect
//     head of github.com and edge-generated replies such as a rate limit
//     403 - arrives in <=512-byte records, so those hops run with tiny
//     512-byte buffers.
//   * release-assets.githubusercontent.com (the signed CDN behind GitHub
//     release assets) does NOT confirm MFLN - the ServerHello carries no
//     max_fragment_length extension, so no reduced record size is agreed
//     at all. Its data records are typically ~1.4 KB (wire-measured
//     2026-11-08: a mix of ~1395-byte records with occasional 3856-byte
//     and full 16408-byte ones), but that is a habit of the edge, not a
//     guarantee: the record limit stays at the TLS maximum of 16 KB, and
//     oversized records DO occur (the successful v1.7.1 boot-update of
//     2026-11-07 logged 26 such drops before completing).
// Two BearSSL engine rules complete the picture:
//   * Unencrypted handshake records (e.g. the CDN's ~4.1 KB certificate
//     chain) are consumed piece by piece and never have to fit the buffer.
//   * Encrypted records must fit the input buffer completely. The CDN hop
//     therefore starts with a 2048-byte input buffer (+ ~325 bytes engine
//     overhead), which covers the typical records while keeping the TLS
//     connect chain small enough for the tight heap - and escalates to
//     16384 on the first BR_ERR_TOO_LARGE (see otaEscalateCdnBuffer),
//     the maximum TLS 1.2 allows, which no legal record can exceed.
// The small buffers are also a heap requirement: the ~15 KB heap of the
// running device is fragmented, and a 4 KB input buffer allocation
// already failed on the device ("unable to allocate memory"), while
// 512/2048-byte buffers allocate reliably.
//
// The download therefore:
//   * requests the firmware with "Range: bytes=a-b" in chunks of 32 KB (8
//     flash sectors, one request each) over ONE kept-alive TLS connection to
//     the CDN. Every fresh connection costs a DNS lookup + TCP connect +
//     (resumed) TLS handshake, makes the WiFi driver burst TX while the heap
//     is at its tightest, and - the deciding factor - every
//     connect/disconnect cycle leaves the heap a little more fragmented. With
//     4 KB chunks on fresh connections (unreleased test builds, 2026-11: 216
//     requests for ~880 KB) that churn coincided with an SDK driver crash
//     right at the second chunk: Exception 29 (StoreProhibited), a ROM memcpy
//     writing to NULL inside beacon/BSS bookkeeping (sta_input ->
//     ieee80211_parse_beacon -> cnx_update_bss_more), i.e. the driver's
//     link-recovery path - the stack also showed TX retry exhaustion
//     (rcReachRetryLimit, pp_tx_idle_timeout) and the next boot first lost
//     the AP with AUTH_EXPIRE. With 32 KB chunks on fresh connections (27
//     requests) the boot-update log of 2026-11-07 still showed the largest
//     free block shrinking from 8.8 KB to 4.4 KB within ten chunks (free heap
//     13.4 KB -> 12.5 KB) until the TLS heap check stopped the download at
//     37 %. A single request for the whole image was not an option either:
//     on the device the connection closed after ~15.4 KB every time. Keeping
//     one connection alive removes the churn without changing the request
//     size the CDN is known to answer completely. If the server closes the
//     connection anyway, the next chunk simply connects afresh. A stalled
//     connection only costs the unwritten remainder, because the byte-exact
//     resume below continues behind the last confirmed byte;
//   * escalates the CDN input buffer rung by rung when a record does not
//     fit it: the TLS engine dies with BR_ERR_TOO_LARGE, otaFetchChunk()
//     drops the connection in a controlled way and returns
//     OTA_FETCH_TOOBIG, and the download loop reconnects one rung up
//     (2048 -> 4096 -> 16384, each hop heap-probe-gated,
//     otaEscalateCdnBuffer). The 4096 rung catches the wire-measured
//     3856-byte oversize records while its connect chain still fits the
//     fragmented heap; the 16384 rung is the TLS 1.2 maximum, which no
//     legal record can exceed, but its ~16.7 KB contiguous block never
//     fits next to the running services. Retrying with the same
//     too-small buffer would run into the same record again - exactly
//     the 26-drop grind of the 2026-11-07 v1.7.1 download. When the next
//     rung does not fit, the download stays on the current one and the
//     byte-exact resume survives the remaining (16 KB-class) drops one
//     at a time;
//   * closes a dropped connection with an RST, not a FIN
//     (otaDropConnection): a graceful close parks the lwIP PCB in
//     TIME_WAIT for minutes, pinning ~100-250 bytes of heap per drop
//     (PCB plus fragmentation). The 2026-11-08 download died from
//     exactly that - the reconnect chain no longer fit while dead PCBs
//     were still counted as free heap (probe failure at 12.5 KB free,
//     attempts aborted after 3, 9 and 5 drops);
//   * keeps the radio out of every sleep mode for the whole download
//     (OtaRadioGuard). setup() already forces WIFI_NONE_SLEEP before the
//     boot update runs; the guard additionally covers the runtime update
//     paths (ESP32 menu) and restores the previous mode if the download
//     ends without a reboot;
//   * waits for the station to re-associate if the link was lost mid
//     download (up to OTA_WIFI_WAIT_MS, one WiFi.reconnect() nudge) instead
//     of firing chunk requests into a dead link;
//   * paces the flash writes: after every erased 4 KB sector the read loop
//     pauses a millisecond, so the driver can drain pending beacon and
//     management processing between two interrupt-masked erase windows;
//   * reuses one TLS session for all chunk connections (BearSSL::Session),
//     so only the first handshake pays the full key exchange cost;
//   * resolves redirects (github.com -> signed CDN URL) on the fly; the
//     signed URL is valid for ~1 h (measured), which is far more than a
//     full download needs. If it ever expires mid-download, the fetch
//     restarts from the original release URL;
//   * falls back to the asset's API endpoint when github.com's web edge
//     fails: the release URL lives on the github.com web tier, which is
//     separate infrastructure from api.github.com (during the 2026-11-08
//     outage every release download got a 504 from the web edge while the
//     API kept answering). The check remembers the asset's "url" (the
//     releases/assets/<id> endpoint); on the first server-side error from
//     the release hop the whole resolution chain switches to it (with
//     Accept: application/octet-stream, which makes the endpoint answer
//     with the same 302 to the signed CDN URL);
//   * retries server-side errors (5xx/429) on a slow schedule (3..60 s):
//     gateway timeouts recover in tens of seconds, and hammering them with
//     1.5 s retries only burns heap and rate limit;
//   * pre-allocates the 6.2 KB BearSSL stack thunk for the whole download
//     (stack_thunk_add_ref) so no TLS connect has to carve that block out
//     of a tired heap;
//   * verifies the SHA256 digest from the release API over the
//     concatenated chunks before the update is committed.
//
// On the ESP32 the default 16 KB buffers handle the CDN records fine, so
// the same code simply runs with one big chunk (= single request).
// ---------------------------------------------------------------------------

#ifdef ESP8266
// 32 KB per request = 8 flash sectors. Verified on the device with fresh
// connections: the CDN answers such ranges completely (11 consecutive chunks).
// A single request for the whole image did NOT work: the connection closed
// after ~15.4 KB every time (2026-11-07 log). The chunks are requested on one
// kept-alive connection, see otaCdnClient.
#define OTA_CHUNK_SIZE     32768L
#define OTA_TLS_IN_BUF_GH   512   // GitHub hosts confirm MFLN 512 -> tiny records
#define OTA_TLS_IN_BUF_CDN  2048  // CDN records are typically ~1.4 KB but NOT
                                  // bounded (MFLN unconfirmed); oversized
                                  // records escalate up the rung ladder
#define OTA_TLS_IN_BUF_STEP 4096  // middle rung: catches the wire-measured
                                  // 3856-byte oversize records while its
                                  // ~13.1 KB connect chain still fits the
                                  // fragmented heap (the 16 KB chain needs a
                                  // ~16.7 KB contiguous block and never fits)
#define OTA_TLS_IN_BUF_MAX 16384  // TLS 1.2 record maximum (setBufferSizes
                                  // clamps here): no legal record exceeds it
#define OTA_TLS_OUT_BUF     512   // xmit buffer pins the MFLN offer at 512

// Heap requirements of one WiFiClientSecure connect(), taken from the core
// sources (WiFiClientSecureBearSSL.cpp / StackThunk.cpp, core 3.1.2) and
// confirmed against the crash logs. connect() allocates, in this order:
//   * br_ssl_client_context via make_shared: 3424 bytes (3408 + ctrl block)
//   * the input and output record buffers: size + 325 / size + 85 bytes
//     (512+512 for the GitHub hops, 2048+512 for the CDN hop)
//   * br_x509_insecure_context via make_shared: 1496 bytes - the allocation
//     that failed in the v1.7.1 boot-update crash ("last failed alloc
//     ... (1496)", caller _installClientX509Validator)
// plus, while the handshake runs, roughly two TCP segments of lwIP receive
// buffers (the CDN's certificate chain is ~4 KB). These blocks do NOT have to
// be contiguous with each other, so a "largest free block >= sum" test is
// both too strict (it rejected boots that would have worked) and too weak
// (total free heap matters as well). otaTlsHeapProbe() therefore allocates
// all of them at once, exactly as connect() will, and frees them again.
// The 6200-byte BearSSL stack thunk is NOT part of it: performUpdate()
// pre-allocates it once for the whole download (stack_thunk_add_ref), so no
// connect has to find that block on a tired heap.
#define OTA_TLS_PROBE_MARGIN  0      // 0 = model the exact connect() sizes. The
                                     // previous +16 margin made the probe
                                     // stricter than the real connect and - the
                                     // decisive flaw - rejected the exact-size
                                     // holes a dropped connection had just
                                     // freed (freed br_ssl_client_context:
                                     // 3432 bytes usable, probe asked for
                                     // 3424+16=3440). The 19:41 reconnect
                                     // failures all sat at exactly 12128 bytes
                                     // free with the freed 3432/2381/605/1504
                                     // holes unusable. Model error is caught
                                     // by the aggregate guard below.
#define OTA_TLS_PROBE_LWIP    1536   // one lwIP receive segment (2 for a full
                                     // handshake - the ~4 KB cert chain
                                     // arrives; 1 for a resumed session, see
                                     // otaCdnSessionHost)
#define OTA_TLS_PROBE_SLACK   2048   // aggregate free-heap guard over all probe
                                     // blocks: absorbs model error (client
                                     // object, unexpected pbufs, DNS leftovers)
                                     // now that the per-block margin is 0
// 6200-byte thunk (StackThunk.cpp, "Largest stack usage seen in the wild at
// 6120") plus margin; checked before stack_thunk_add_ref() because its
// malloc() failure aborts the device instead of throwing.
#define OTA_THUNK_MIN_BLOCK   6300
#else
#define OTA_CHUNK_SIZE   (8L * 1024L * 1024L)  // ESP32: single request
#endif

// Signed CDN URLs are ~900 bytes today; keep a safety margin.
#define OTA_LOCATION_MAX 2048

// Serial log verbosity of the updater. 0 (default) keeps the log to what a
// user needs to follow an update: boot and release info, one line per 10 %
// progress, error paths and the SHA256 verdict. 1 additionally prints the
// per-chunk and per-record diagnostics (chunk offsets, TLS state on dropped
// connections, heap telemetry, buffer escalation) that tracked down the CDN
// record-size and heap fragmentation issues - flip it to 1 before reporting
// an update that misbehaves.
#define OTA_LOG_VERBOSE 0

// Total time one chunk may take (connect + headers + body).
#define OTA_CHUNK_TIMEOUT_MS 30000UL

// ESP8266: bound for every blocking connect phase (DNS, TCP and the TLS
// handshake are each separately limited by the client timeout inside
// connect()). The software watchdog stays ENABLED for the whole download
// (see performUpdate): every phase inside connect() - the DNS wait in
// hostByName(), the TCP wait in ClientContext::connect() and the BearSSL
// handshake loops - pumps yield()/esp_yield(), and with the soft watchdog
// running those yields feed both the software and the hardware watchdog.
// The timeouts exist so a stalled hop fails into the retry logic after a
// bounded time instead of eating the whole 30 s chunk budget.
#define OTA_CONNECT_TIMEOUT_MS 5000UL

// How long to wait for the station to re-associate after a mid-download
// link loss before the chunk (and with it the update) fails cleanly.
// Re-associations typically take 3-10 s; 20 s leaves margin for slow APs.
#define OTA_WIFI_WAIT_MS 20000UL

// Static member initialization
String FirmwareUpdater::_latestVersion = "";
String FirmwareUpdater::_downloadUrl = "";
String FirmwareUpdater::_assetApiUrl = "";
String FirmwareUpdater::_expectedSha256 = "";
long FirmwareUpdater::_firmwareSize = 0;
bool FirmwareUpdater::_updateChecked = false;

extern const char* FIRMWARE_VERSION;
extern void SerialM_print(const char* str);
extern void SerialM_println(const char* str);

// ---------------------------------------------------------------------------
// Radio guard: keep the WiFi radio fully awake for the whole download.
// CPU-bound handshakes and flash sector erases (interrupts masked for tens
// of milliseconds each) collide badly with a sleeping radio: missed ACKs,
// beacon loss, and finally the driver's link-recovery path - which is where
// an earlier 4 KB-chunk test build crashed (Exception 29, ROM memcpy to NULL
// inside beacon/BSS processing while the link was struggling). gbs-control's setup() already
// forces WIFI_NONE_SLEEP before the boot update runs; this guard covers the
// runtime update paths too and restores whatever mode was active if the
// download ends without a reboot.
// ---------------------------------------------------------------------------
class OtaRadioGuard {
public:
#ifdef ESP8266
    OtaRadioGuard() : _prev(WiFi.getSleepMode()) { WiFi.setSleepMode(WIFI_NONE_SLEEP); }
    ~OtaRadioGuard() { WiFi.setSleepMode(_prev); }
#else
    OtaRadioGuard() : _prev(WiFi.getSleep()) { WiFi.setSleep(false); }
    ~OtaRadioGuard() { WiFi.setSleep(_prev); }
#endif
private:
#ifdef ESP8266
    WiFiSleepType_t _prev;
#else
    bool _prev;
#endif
};

// ---------------------------------------------------------------------------
// Tiny URL parser: splits scheme://host[:port]/path?query into host, port and
// path. The query string is kept in the path: the signed CDN URLs carry
// their authentication in it and must be requested verbatim.
// ---------------------------------------------------------------------------
static bool otaParseUrlIdx(const String& url, String& host, uint16_t& port,
                           int& pathStart, int& pathLen)
{
    int scheme = url.indexOf(F("://"));
    if (scheme < 0) {
        return false;
    }
    bool https = url.startsWith(F("https://"));
    port = https ? 443 : 80;
    unsigned int hostStart = (unsigned int)scheme + 3;
    pathStart = url.indexOf('/', hostStart);
    if (pathStart < 0) {
        return false;
    }
    host = url.substring(hostStart, pathStart);
    pathLen = (int)url.length() - pathStart;
    int colon = host.indexOf(':');
    if (colon >= 0) {
        port = (uint16_t)host.substring(colon + 1).toInt();
        host = host.substring(0, colon);
    }
    return (host.length() > 0 && pathLen > 0);
}

// ---------------------------------------------------------------------------
// Byte-wise HTTP response head parser.
//
// GitHub responses cannot be parsed with line-based String reads:
//   * the redirect Location value is ~900 bytes,
//   * the Content-Security-Policy header alone is ~2.9 KB,
//   * and a trimmed line of exactly "Location:" (the read aborted right
//     after the colon) once produced an empty redirect target.
// This state machine never builds whole lines: it extracts only the status
// code, Content-Length and Location, all matched case-insensitively.
// ---------------------------------------------------------------------------
struct OtaHeadParser {
    // results (valid once httpCode != 0 / headerDone)
    int httpCode;
    long contentLength;
    String location;
    bool headerDone;
    bool locationOverflow;
    // internals
    char status[16];
    uint8_t statusLen;
    uint8_t inHeaders;
    uint8_t locPos, lenPos;      // match positions inside "location" / "content-length"
    uint8_t locOk, lenOk;        // still a candidate for the respective name
    uint8_t pastColon, isLocation, isLength, valueStarted, lineEmpty;
    char lenBuf[12];
    uint8_t lenLen;
};

static void otaHeadInit(OtaHeadParser& p)
{
    p.httpCode = 0;
    p.contentLength = -1;
    p.location = "";
    p.headerDone = false;
    p.locationOverflow = false;
    memset(p.status, 0, sizeof(p.status));
    p.statusLen = 0;
    p.inHeaders = 0;
    p.locPos = p.lenPos = 0;
    p.locOk = p.lenOk = 1;
    p.pastColon = p.isLocation = p.isLength = p.valueStarted = 0;
    p.lineEmpty = 1;
    memset(p.lenBuf, 0, sizeof(p.lenBuf));
    p.lenLen = 0;
}

static void otaHeadFeed(OtaHeadParser& p, char ch)
{
    if (p.headerDone) {
        return;
    }

    if (!p.inHeaders) {
        // Status line: "HTTP/1.x NNN reason"
        if (ch == '\n') {
            p.inHeaders = 1;
            if (p.statusLen >= 12) {
                p.httpCode = atoi(&p.status[9]);
            }
            return;
        }
        if (ch != '\r' && p.statusLen < sizeof(p.status) - 1) {
            p.status[p.statusLen++] = ch;
        }
        return;
    }

    if (ch == '\n') {
        if (p.isLength && p.lenLen > 0) {
            p.lenBuf[p.lenLen] = 0;
            p.contentLength = atol(p.lenBuf);
        }
        if (p.lineEmpty) {
            p.headerDone = true;  // blank line terminates the header block
            return;
        }
        // Prepare for the next header line.
        p.locPos = p.lenPos = 0;
        p.locOk = p.lenOk = 1;
        p.pastColon = p.isLocation = p.isLength = p.valueStarted = 0;
        p.lenLen = 0;
        p.lineEmpty = 1;
        return;
    }
    if (ch == '\r') {
        return;
    }
    p.lineEmpty = 0;

    if (!p.pastColon) {
        if (ch == ':') {
            p.pastColon = 1;
            p.isLocation = (p.locOk && p.locPos == 8);   // strlen("location")
            p.isLength  = (p.lenOk && p.lenPos == 14);   // strlen("content-length")
        } else {
            char lc = ch;
            if (lc >= 'A' && lc <= 'Z') {
                lc += 32;
            }
            if (p.locOk) {
                if (lc == "location"[p.locPos]) p.locPos++; else p.locOk = 0;
            }
            if (p.lenOk) {
                if (lc == "content-length"[p.lenPos]) p.lenPos++; else p.lenOk = 0;
            }
        }
        return;
    }

    // Inside the header value.
    if (!p.valueStarted) {
        if (ch == ' ' || ch == '\t') {
            return;  // skip whitespace after the colon
        }
        p.valueStarted = 1;
    }
    if (p.isLocation) {
        if (p.location.length() == 0) {
            // Allocate only when a redirect actually arrives (not for every chunk
            // response), once, so the appends below never re-allocate.
            p.location.reserve(OTA_LOCATION_MAX);
        }
        if ((unsigned)p.location.length() < OTA_LOCATION_MAX) {
            p.location += ch;
        } else {
            p.locationOverflow = true;
        }
    } else if (p.isLength) {
        if (p.lenLen < sizeof(p.lenBuf) - 1) {
            p.lenBuf[p.lenLen++] = ch;
        }
    }
}

// Reads the response head (status line + headers) from a connected client.
// Returns true when a complete header block was seen; httpCode may still be
// valid on false (truncated head) - the caller decides what it needs.
static bool otaReadHead(WiFiClient& client, OtaHeadParser& p)
{
    unsigned long deadline = millis() + OTA_CHUNK_TIMEOUT_MS;
    otaHeadInit(p);
    while ((long)(millis() - deadline) < 0) {
        int avail = client.available();
        if (avail > 0) {
            int c = client.read();
            if (c < 0) {
                break;
            }
            otaHeadFeed(p, (char)c);
            if (p.headerDone) {
                return true;
            }
        } else {
            if (!client.connected()) {
                break;  // server closed (possibly mid-head)
            }
            delay(2);
            yield();
#ifdef ESP8266
            ESP.wdtFeed();  // belt-and-braces next to the yield(): keeps
                            // both watchdogs fresh even while waiting
#endif
        }
    }
    return p.headerDone;
}

// ---------------------------------------------------------------------------
// Shared TLS session for the chunked download. Only the ESP8266 needs it
// (the ESP32 downloads with a single request).
// ---------------------------------------------------------------------------
#ifdef ESP8266
static BearSSL::Session otaTlsSession;

// Test-allocates everything one WiFiClientSecure::connect() needs (see the
// comment at OTA_TLS_PROBE_MARGIN) at the same time, in the same order, with
// the non-throwing malloc, then frees them again in reverse order. Returns false
// if any block does not fit. The real connect() uses a throwing `new` for the
// client and x509 contexts, so running out there would panic the device; this
// turns it into a clean failure beforehand. The record buffers are probed
// through the same IRAM-first, DRAM-fallback path the core uses for them
// (WiFiClientSecureCtx::_alloc_iobuf), so the probe result matches what the
// real connect() will experience - including the 16 KB input buffer of the
// escalation, whose ~16.7 KB block can only ever fit DRAM.
// lwipSegs models the lwIP receive segments the handshake needs: 2 for a
// full handshake (the ~4 KB certificate chain arrives then), 1 for a resumed
// session (session cache hit for this host - no cert chain, tiny flights;
// see otaCdnSessionHost).
static void* otaProbeAllocIobuf(size_t sz)
{
    void* p;
    {
        HeapSelectIram iram;  // same region preference as _alloc_iobuf()
        p = malloc(sz);
    }
    if (!p) {
        HeapSelectDram dram;
        p = malloc(sz);
    }
    return p;
}

static bool otaTlsHeapProbe(int inBuf, int lwipSegs)
{
    if (lwipSegs < 1 || lwipSegs > 2) {
        lwipSegs = 2;
    }
    // Aggregate free-heap guard (OTA_TLS_PROBE_SLACK): with the per-block
    // margin at 0 the probe models the real connect() exactly, so a wholesale
    // underestimate of the chain shows up here instead of in a panic.
    if (ESP.getFreeHeap() < (uint32_t)(3424 + inBuf + 325 + 597 + 1496
                                       + lwipSegs * OTA_TLS_PROBE_LWIP
                                       + OTA_TLS_PROBE_SLACK)) {
        return false;
    }
    void* blocks[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    // Allocation order of the real connect() (_connectSSL):
    blocks[0] = malloc(3424 + OTA_TLS_PROBE_MARGIN);   // br_ssl_client_context (make_shared, DRAM)
    if (blocks[0]) {
        blocks[1] = otaProbeAllocIobuf((size_t)inBuf + 325 + OTA_TLS_PROBE_MARGIN);  // input record buffer
    }
    if (blocks[1]) {
        blocks[2] = otaProbeAllocIobuf((size_t)OTA_TLS_OUT_BUF + 85 + OTA_TLS_PROBE_MARGIN);  // output record buffer
    }
    if (blocks[2]) {
        blocks[3] = malloc(1496 + OTA_TLS_PROBE_MARGIN);  // br_x509_insecure_context (DRAM)
    }
    for (int s = 0; s < lwipSegs; s++) {
        if (blocks[3 + s]) {
            blocks[4 + s] = malloc(OTA_TLS_PROBE_LWIP);  // lwIP receive segment
        }
    }
    bool ok = true;
    for (int s = 0; s < lwipSegs; s++) {
        ok = ok && (blocks[4 + s] != nullptr);
    }
    for (int i = 5; i >= 0; i--) {
        if (blocks[i]) {
            free(blocks[i]);
        }
    }
    return ok;
}

// CDN input-buffer state for the rung escalation (see the block comment on
// top): starts at OTA_TLS_IN_BUF_CDN for every download; every
// BR_ERR_TOO_LARGE escalates one rung towards OTA_TLS_IN_BUF_MAX
// (probe-gated via otaEscalateCdnBuffer), a reconnect whose chain no longer
// fits the aging heap steps down one rung at a time, and the next oversized
// record retries the escalation. Reset by performUpdate().
static int otaCdnInBuf = OTA_TLS_IN_BUF_CDN;
static bool otaCdnEscalated = false;  // true only at the 16384 top rung

// Host of the CDN connection whose TLS session is currently cached in
// otaTlsSession. A reconnect to the same host can RESUME the session: the
// handshake then transfers no certificate chain, needs far fewer lwIP
// receive segments, and the heap probe above accounts for 1 instead of 2
// (fixes7). Reset by performUpdate(); cleared whenever a connect to the host
// fails (the session may not have been cached server-side).
static char otaCdnSessionHost[64] = {0};
#endif

// Result codes of otaFetchChunk()
enum OtaFetchResult {
    OTA_FETCH_OK = 0,
    OTA_FETCH_EXPIRED,   // 401/403 from the CDN: signature expired, restart from the release URL
    OTA_FETCH_FAILED,
    OTA_FETCH_SERVER,    // 5xx/429: the server itself is in trouble (or the API
                         // rate limit said no) - slow retry schedule, and at the
                         // release hop a switch to the API asset endpoint
    OTA_FETCH_HEAP,      // the heap cannot carry the TLS connect chain (probe failed)
    OTA_FETCH_TOOBIG     // ESP8266 only: encrypted CDN record larger than the
                         // input buffer (BR_ERR_TOO_LARGE) - escalates the buffer
};

// ---------------------------------------------------------------------------
// Keep-alive connection to the CDN. Every connect/disconnect cycle leaves the
// ~15 KB heap a little more fragmented (see the block comment on top), so the
// connection of a finished chunk is kept open and the next chunk is requested
// on it ("Connection: keep-alive"). It is created with `new` because a
// WiFiClientSecure holds the 6 KB BearSSL stack for as long as it exists - it
// must not be a static object. If the server closed it (or it is otherwise
// unusable), otaFetchChunk() falls back to a fresh connection.
// ---------------------------------------------------------------------------
static WiFiClientSecure* otaCdnClient = nullptr;

static void otaCdnRelease()
{
    if (otaCdnClient) {
        otaCdnClient->stop();
        delete otaCdnClient;
        otaCdnClient = nullptr;
    }
}

#ifdef ESP8266
// lwIP parks every gracefully closed connection in TIME_WAIT (up to
// MEMP_NUM_TCP_PCB_TIME_WAIT = 5 PCBs, kept for minutes) and each parked PCB
// pins ~160 bytes of heap. The 2026-11-08 boot-update logs show a plateau:
// again and again the heap probe of a CDN reconnect failed at EXACTLY 12128
// bytes free, 792 bytes (= 5 x ~158) below the 12.8-12.9 KB where the same
// reconnects succeeded - consistent with five parked PCBs on a heap that has
// no reserve (the probe passed at 12808, failed at 12648; a hypothesis, the
// "Freed TIME_WAIT" log line below shows whether it holds). The core's own
// recipe (esp8266/Arduino FAQ, test_ClientContext) kills them: they only
// protect against stray late packets of an already closed connection, which
// is irrelevant for connections to a different server port each time.
// Declarations as in the core test - no lwIP header needed.
struct tcp_pcb;
extern struct tcp_pcb* tcp_tw_pcbs;
extern "C" void tcp_abort(struct tcp_pcb* pcb);

static void otaTcpCleanup()
{
    uint32_t before = ESP.getFreeHeap();
    while (tcp_tw_pcbs) {
        tcp_abort(tcp_tw_pcbs);
    }
    uint32_t after = ESP.getFreeHeap();
    if (after > before + 64UL) {
        Serial.print(F("[OTA] Freed TIME_WAIT connections: +"));
        Serial.print(after - before);
        Serial.println(F(" bytes"));
    }
}

// Close a connection whose TLS engine was killed by an oversized record.
// abort() ends the TCP connection with an RST instead of a FIN: a graceful
// close parks the lwIP PCB in TIME_WAIT for minutes, and the dropped
// connections of the 2026-11-08 download each left ~100-250 bytes of heap
// pinned that way (PCB plus fragmentation) until the reconnect chain no
// longer fit (probe failure at 12.5 KB free after 3-9 drops). The server
// never reads our close_notify anyway - its oversized record killed the
// engine first - so nothing is lost. stop() afterwards frees the SSL
// engine context and the record buffers.
static void otaDropConnection(WiFiClientSecure& client)
{
    client.abort();  // RST: no TIME_WAIT PCB left behind on our side
    client.stop();   // free the SSL engine + record buffers
}

// Escalate the CDN input buffer one rung after a BR_ERR_TOO_LARGE:
// 2048 -> 4096 -> 16384. The intermediate rung catches the wire-measured
// 3856-byte oversize records (see the block comment on top) while its
// connect chain still fits the fragmented heap; the top rung catches every
// legal TLS 1.2 record but needs a ~16.7 KB contiguous block, which the
// ~15 KB device heap cannot carry next to the running services. Called
// from the download loop once the failed hop's client is already destroyed
// and the kept-alive connection is released, so the probe sees the fullest
// heap this download will ever offer again. When the next rung does not
// fit, the download stays on the current one: the byte-exact resume then
// survives oversized records one drop at a time, and the next
// BR_ERR_TOO_LARGE retries the escalation.
static void otaEscalateCdnBuffer()
{
    if (otaCdnEscalated) {
        return;  // already running with the 16 KB buffer
    }
    otaCdnRelease();  // free the kept-alive chain before probing
    otaTcpCleanup();  // parked TIME_WAIT PCBs would only shrink the heap seen below
    int next = (otaCdnInBuf < OTA_TLS_IN_BUF_STEP) ? OTA_TLS_IN_BUF_STEP
                                                   : OTA_TLS_IN_BUF_MAX;
#if OTA_LOG_VERBOSE
    Serial.print(F("[OTA] CDN record exceeded the input buffer, trying "));
    Serial.print(next / 1024);
    Serial.print(F(" KB: heap free "));
    Serial.print(ESP.getFreeHeap());
    Serial.print(F(", max block "));
    Serial.println(ESP.getMaxFreeBlockSize());
#endif
    if (otaTlsHeapProbe(next, 1)) {  // reconnect after an established
                                      // connection: resumed session, no chain
        otaCdnInBuf = next;
        otaCdnEscalated = (next == OTA_TLS_IN_BUF_MAX);
#if OTA_LOG_VERBOSE
        Serial.print(F("[OTA] "));
        Serial.print(next / 1024);
        Serial.println(F(" KB input buffer fits, reconnecting with it"));
#endif
    } else {
#if OTA_LOG_VERBOSE
        Serial.print(F("[OTA] "));
        Serial.print(next / 1024);
        Serial.print(F(" KB chain does not fit, staying at "));
        Serial.print(otaCdnInBuf);
        Serial.println(F(" bytes (resuming per record)"));
#else
        // Default verbosity: one notice instead of ~20 repeats. The
        // per-record reconnects pause the download for a few seconds each;
        // this line explains the pauses without flooding the log.
        static bool otaOversizeExplained = false;
        if (!otaOversizeExplained) {
            otaOversizeExplained = true;
            Serial.println(F("[OTA] CDN records exceed the input buffer; "
                             "resuming per record (OTA_LOG_VERBOSE for details)"));
        }
#endif
    }
}
#endif

// Bounded wait that keeps the watchdogs fed: the server-error retry schedule
// waits up to 60 s in one piece, and neither watchdog may run dry while the
// download sits in it. delay() alone feeds the software watchdog on the
// ESP8266, but the explicit ESP.wdtFeed() keeps the hardware one fresh too
// (same belt-and-braces as otaReadHead). On the ESP32 delay() yields to the
// async tasks, so the web UI keeps answering during the wait.
static void otaWaitFed(uint32_t ms)
{
    unsigned long t0 = millis();
    while (millis() - t0 < ms) {
        delay(20);
        yield();
#ifdef ESP8266
        ESP.wdtFeed();
#endif
    }
}

// Owns the client of one hop and deletes it on every exit of the hop, unless
// it was handed over to otaCdnClient (release()).
struct OtaClientHolder {
    WiFiClientSecure* p;
    OtaClientHolder() : p(nullptr) {}
    ~OtaClientHolder()
    {
        if (p) {
            p->stop();
            delete p;
        }
    }
    WiFiClientSecure* release()
    {
        WiFiClientSecure* r = p;
        p = nullptr;
        return r;
    }
private:
    OtaClientHolder(const OtaClientHolder&);
    OtaClientHolder& operator=(const OtaClientHolder&);
};

#ifdef ESP8266
// ---------------------------------------------------------------------------
// Pre-resolves the redirect chain (github.com or api.github.com -> signed
// CDN URL) and verifies the final URL with a 1-byte range request - all
// BEFORE the pad trick, Update.begin() and the resume replay carve the boot
// heap into the two ~7 KB holes the download then has to live with (fixes7).
//
// Why: both the 19:09 (resume on) and 19:41 (resume off) runs did the
// redirect dance only AFTER Update.begin: the api.github.com/github.com
// client (~6.4 KB alloc/free churn) plus the ~900-byte signed URL Strings
// carved the biggest hole from ~7.5 KB down to ~6.5 KB, and the first CDN
// connect chain no longer fit - in the 19:09 run every resumed boot died
// right there, six times in a row (that, NOT the replay itself, is why the
// resume looked broken; the "Heap after replay" line shows the replay costs
// nothing). Resolving first turns the first chunk connect of EVERY attempt
// (fresh or resumed) into a direct CDN connect on the fullest heap this boot
// can offer. The redirect-following inside otaFetchChunk() stays as the
// fallback for a mid-download expiry of the signed URL.
//
// The connections are closed with an RST (otaDropConnection): a graceful
// close would park the PCB in TIME_WAIT for minutes, pinning ~100-250 bytes
// of heap right through the download that needs them.
//
// Returns true with url set to the final (CDN) URL. false leaves the inline
// redirect path in charge - nothing is lost, just a round trip.
// ---------------------------------------------------------------------------
static bool otaResolveRedirects(String& url)
{
    for (int hop = 0; hop < 5; hop++) {
        String host;
        uint16_t port = 443;
        int pathStart = 0, pathLen = 0;
        if (!otaParseUrlIdx(url, host, port, pathStart, pathLen)) {
            return false;
        }
        bool cdnHop = host.endsWith(F("githubusercontent.com"));
        int inBuf = cdnHop ? OTA_TLS_IN_BUF_CDN : OTA_TLS_IN_BUF_GH;
        if (!otaTlsHeapProbe(inBuf, 2)) {
            // Before Update.begin() this only happens on a genuinely tiny
            // boot heap; the inline path gets its chance anyway.
            return false;
        }
        OtaClientHolder holder;
        holder.p = new (std::nothrow) WiFiClientSecure();
        if (!holder.p) {
            return false;
        }
        WiFiClientSecure& client = *holder.p;
        client.setInsecure();  // same trust model as the update check
        client.setBufferSizes(inBuf, OTA_TLS_OUT_BUF);
        client.setSession(&otaTlsSession);
        if (WiFi.status() != WL_CONNECTED) {
            return false;  // the boot update checked; do not wait here
        }
        ESP.wdtFeed();
        client.setTimeout(OTA_CONNECT_TIMEOUT_MS);
        if (!client.connect(host.c_str(), port)) {
            // An oversized record during a CDN handshake still proves the
            // redirect target is alive and TLS-speaking; the download
            // itself escalates its buffer as needed.
            char sslErr[128] = {0};
            if (cdnHop
                && client.getLastSSLError(sslErr, sizeof(sslErr)) == BR_ERR_TOO_LARGE) {
                otaCdnSessionHost[0] = '\0';  // handshake died: no session
                return true;
            }
            return false;
        }
        ESP.wdtFeed();
        if (cdnHop) {
            // Connection established: the TLS session is cached for this
            // host now (see otaCdnSessionHost).
            strncpy(otaCdnSessionHost, host.c_str(), sizeof(otaCdnSessionHost) - 1);
            otaCdnSessionHost[sizeof(otaCdnSessionHost) - 1] = '\0';
        }
        bool apiHop = (host == GITHUB_API_HOST);
        client.print(F("GET "));
        client.write((const uint8_t*)(url.c_str() + pathStart), (size_t)pathLen);
        client.print(F(" HTTP/1.1\r\nHost: "));
        client.print(host);
        client.print(F("\r\nUser-Agent: GBS-Control"));
        if (apiHop) {
            client.print(F("\r\nAccept: application/octet-stream"));
        }
        client.print(F("\r\nRange: bytes=0-0\r\nConnection: close\r\n\r\n"));
        OtaHeadParser head;
        bool headComplete = otaReadHead(client, head);
        {
            char errText[128] = {0};
            if (client.getLastSSLError(errText, sizeof(errText)) == BR_ERR_TOO_LARGE) {
                // The CDN answered with a record larger than the buffer: the
                // URL is final (and was just refreshed by the redirect).
                otaDropConnection(client);
                return cdnHop;
            }
        }
        otaDropConnection(client);
        if (!headComplete && head.httpCode == 0) {
            return false;
        }
        int code = head.httpCode;
        if ((code == 301 || code == 302 || code == 303 || code == 307 || code == 308)
            && head.location.length() > 0) {
            if (head.location.startsWith("/")) {
                String base = (url.startsWith("https://")) ? "https://" : "http://";
                int slash = url.indexOf('/', 8);
                String origin = (slash > 0) ? url.substring(0, slash) : url;
                url = origin + head.location;
            } else {
                url = head.location;
            }
            Serial.print(F("[OTA] Pre-resolve: redirect to host: "));
            int hEnd = url.indexOf('/', 8);
            Serial.println(url.substring(8, hEnd > 0 ? hEnd : url.length()));
            continue;
        }
        if (code == 206 || code == 200) {
            Serial.print(F("[OTA] Pre-resolve: final URL answers ranges (host: "));
            Serial.print(host);
            Serial.println(F(")"));
            return true;
        }
        // 401/403/4xx/5xx/...: let the inline path apply its retry and
        // API-fallback logic - it knows the schedules.
        return false;
    }
    return false;
}
#endif  // ESP8266

// ---------------------------------------------------------------------------
// Downloads len bytes at offset from startUrl (following up to 5 redirects),
// writes them into the Update partition and the SHA256 context, and leaves
// currentUrl pointing at the final (redirected) URL so later chunks can
// skip the redirect hops. The connection to the CDN stays open afterwards
// (otaCdnClient) for the next chunk.
// ---------------------------------------------------------------------------
static OtaFetchResult otaFetchChunk(String& currentUrl, long offset, long len,
                                    void* shaCtx, long* consumed)
{
    // *consumed = bytes of this chunk already written to flash and the hash. It is
    // valid on every return, also on failure, so that a retry continues behind them
    // instead of writing the same bytes twice.
    *consumed = 0;
    bool skipReuse = false;  // set when a kept-alive CDN connection turned out stale
    for (int hop = 0; hop <= 5; hop++) {
        String host;
        uint16_t port = 443;
        int pathStart = 0, pathLen = 0;
        if (!otaParseUrlIdx(currentUrl, host, port, pathStart, pathLen)) {
            Serial.println(F("[OTA] Invalid URL"));
            return OTA_FETCH_FAILED;
        }

#if OTA_LOG_VERBOSE
        Serial.print(F("[OTA] Chunk @"));
        Serial.print(offset);
        Serial.print(F(" from "));
        Serial.println(host);
#endif

        bool cdnHop = host.endsWith(F("githubusercontent.com"));
        OtaClientHolder holder;
        bool reused = false;
        if (cdnHop && !skipReuse && otaCdnClient != nullptr) {
            // Keep-alive: the previous chunk left its CDN connection open.
            if (otaCdnClient->connected()) {
                holder.p = otaCdnClient;
                otaCdnClient = nullptr;
                reused = true;
            } else {
                otaCdnRelease();  // the server closed it in the meantime
            }
        }
        if (!reused) {
#ifdef ESP8266
            // The BearSSL connect chain allocates (in order): the 3424-byte
            // br_ssl_client_context, the record buffers and the 1496-byte
            // br_x509_insecure_context - each with a plain `new` whose failure
            // PANICS the device (core abi.cpp: "Unhandled C++ exception: OOM",
            // exactly the v1.7.1 crash). The 6200-byte stack thunk is already
            // held by performUpdate() for the whole download, so it is not part
            // of the per-hop budget. Probe whether the whole chain fits and fail
            // cleanly instead of crashing; the caller retries and, if the heap
            // does not recover, aborts (and, in boot mode, restarts for a fresh
            // heap).
            {
                otaTcpCleanup();  // see there: parked TIME_WAIT PCBs eat the probe's reserve
                uint32_t freeHeap = ESP.getFreeHeap();
                uint32_t maxBlock = ESP.getMaxFreeBlockSize();
                int inBuf = cdnHop ? otaCdnInBuf : OTA_TLS_IN_BUF_GH;
                // A reconnect to the host whose TLS session is cached can
                // RESUME the session: no certificate chain, one lwIP receive
                // segment less (see otaCdnSessionHost) - the difference that
                // lets a post-drop reconnect fit the fragmented heap again.
                bool resumedSession = cdnHop && otaCdnSessionHost[0] != '\0'
                                      && strcmp(host.c_str(), otaCdnSessionHost) == 0;
                int lwipSegs = resumedSession ? 1 : 2;
                bool probeOk = otaTlsHeapProbe(inBuf, lwipSegs);
                if (!probeOk && cdnHop && otaCdnInBuf > OTA_TLS_IN_BUF_CDN) {
                    // The escalated chain no longer fits the aging heap.
                    // Rather than failing the download, step down one rung
                    // at a time (16384 -> 4096 -> 2048): the byte-exact
                    // resume survives oversized records one drop at a time,
                    // and the next BR_ERR_TOO_LARGE retries the escalation.
                    while (!probeOk && otaCdnInBuf > OTA_TLS_IN_BUF_CDN) {
                        otaCdnInBuf = (otaCdnInBuf > OTA_TLS_IN_BUF_STEP)
                                          ? OTA_TLS_IN_BUF_STEP
                                          : OTA_TLS_IN_BUF_CDN;
                        otaCdnEscalated = false;
                        inBuf = otaCdnInBuf;
                        Serial.print(F("[OTA] Escalated TLS chain no longer fits, stepping down to "));
                        Serial.print(otaCdnInBuf);
                        Serial.println(F(" bytes"));
                        probeOk = otaTlsHeapProbe(inBuf, lwipSegs);
                    }
                    freeHeap = ESP.getFreeHeap();
                    maxBlock = ESP.getMaxFreeBlockSize();
                }
                if (offset == 0 || !probeOk) {
                    Serial.print(F("[OTA] Heap before connect: free "));
                    Serial.print(freeHeap);
                    Serial.print(F(", max block "));
                    Serial.print(maxBlock);
                    Serial.println(probeOk ? F(" (TLS probe ok)") : F(" (TLS probe failed)"));
                }
                if (!probeOk) {
                    Serial.println(F("[OTA] Not enough heap for TLS"));
                    return OTA_FETCH_HEAP;
                }
            }
#endif

            holder.p = new (std::nothrow) WiFiClientSecure();
            if (!holder.p) {
                Serial.println(F("[OTA] Out of memory for the TLS client"));
                return OTA_FETCH_HEAP;
            }
            WiFiClientSecure& client = *holder.p;
            client.setInsecure();  // same trust model as the update check
#ifdef ESP8266
            // With the 512-byte xmit buffer BearSSL offers MFLN 512.
            // GitHub's own hosts confirm it: even the ~5.6 KB redirect head
            // of github.com arrives in <=512-byte records, so that hop needs
            // only a tiny input buffer. The release CDN does NOT confirm
            // MFLN, so its record sizes are only bound by the TLS maximum:
            // the hop starts with 2048 bytes (typical records are ~1.4 KB)
            // and escalates to 16384 on the first oversized record (see the
            // block comment on top and otaEscalateCdnBuffer).
            client.setBufferSizes(cdnHop ? otaCdnInBuf : OTA_TLS_IN_BUF_GH,
                                  OTA_TLS_OUT_BUF);
            client.setSession(&otaTlsSession);  // resume after the first handshake
#endif
            // A link lost mid-download (driver busy recovering, AP cleaning up
            // stale state) must not be hammered with chunk requests: wait for
            // the re-association instead, nudging the auto-reconnect once.
            // This turns "download dies while the radio comes back" into a
            // short pause plus a byte-exact resume.
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println(F("[OTA] WiFi lost, waiting for reconnect..."));
                unsigned long since = millis();
                bool nudged = false;
                while (WiFi.status() != WL_CONNECTED && millis() - since < OTA_WIFI_WAIT_MS) {
                    delay(100);
#ifdef ESP8266
                    ESP.wdtFeed();  // keep both watchdogs fresh during the wait
#endif
                    if (!nudged && millis() - since > 7000UL) {
                        WiFi.reconnect();  // one nudge for the auto-reconnect
                        nudged = true;
                    }
                }
                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println(F("[OTA] WiFi did not come back"));
                    return OTA_FETCH_FAILED;
                }
                Serial.println(F("[OTA] WiFi back, continuing"));
            }

#ifdef ESP8266
            // Bound DNS + TCP + TLS handshake so a stalled hop fails cleanly
            // into the retry logic after a bounded time. The watchdog stays
            // ENABLED (see performUpdate): the connect phases yield internally,
            // and with the soft watchdog running those yields feed both
            // watchdogs. The explicit feed only starts the hop on a fresh
            // window. (An earlier revision stopped the soft watchdog for the
            // download - with it stopped nothing could feed the hardware
            // watchdog from inside connect(), and the device reset with
            // "wdt reset" (rst cause:4) at the second chunk, right when the
            // CDN host's DNS cache entry had expired and a fresh lookup plus
            // TCP setup crossed the ~6 s hardware window.)
            ESP.wdtFeed();
            client.setTimeout(OTA_CONNECT_TIMEOUT_MS);
#else
            client.setTimeout(OTA_CHUNK_TIMEOUT_MS);
#endif

            if (!client.connect(host.c_str(), port)) {
                Serial.println(F("[OTA] Connection failed"));
#ifdef ESP8266
                // Distinguish OOM (buffer allocation) from record-size problems.
                char sslErr[128] = {0};
                int sslCode = client.getLastSSLError(sslErr, sizeof(sslErr));
                if (sslErr[0]) {
                    Serial.print(F("[OTA] TLS error: "));
                    Serial.println(sslErr);
                }
                Serial.print(F("[OTA] max free heap block: "));
                Serial.println(ESP.getMaxFreeBlockSize());
                if (sslCode == BR_ERR_TOO_LARGE) {
                    // The handshake flight itself exceeded the input buffer.
                    return OTA_FETCH_TOOBIG;
                }
#endif
                return OTA_FETCH_FAILED;
            }

#ifdef ESP8266
            ESP.wdtFeed();  // connected: fresh window for request/head/body
            if (offset == 0) {
                // Which side of the MFLN split is this host? github.com and
                // api.github.com echo the extension (records stay <= 512),
                // the release CDN does not implement MFLN at all (wire-
                // measured 2026-11-08, see the block comment on top).
                Serial.print(F("[OTA] MFLN "));
                Serial.print(client.getMFLNStatus() ? F("negotiated")
                                                    : F("not negotiated"));
                Serial.print(F(" ("));
                Serial.print(host);
                Serial.println(F(")"));
            }
#endif
#ifdef ESP8266
            if (cdnHop) {
                // The connection is established: the TLS session is cached
                // for this host now, so a later reconnect (drop, escalation)
                // probes with the cheaper resumed-handshake budget.
                strncpy(otaCdnSessionHost, host.c_str(), sizeof(otaCdnSessionHost) - 1);
                otaCdnSessionHost[sizeof(otaCdnSessionHost) - 1] = '\0';
            }
#endif
        }
        WiFiClientSecure& client = *holder.p;

        // The API asset endpoint needs the octet-stream Accept to answer
        // with the 302 to the signed CDN URL; without it (or with a JSON
        // accept) it returns the asset's metadata document instead.
        bool apiHop = (host == GITHUB_API_HOST);
        client.print(F("GET "));
        // The request path is written straight from currentUrl instead of a
        // parsed String copy: the signed CDN path is ~800 bytes, and a copy
        // of it sat parked in the middle of the biggest heap hole during
        // every connect of the 19:09/19:41 runs - exactly what shrank the
        // hole below the connect chain's needs (fixes7; same idea as the
        // unused no-path-copy patch, folded in here).
        client.write((const uint8_t*)(currentUrl.c_str() + pathStart), (size_t)pathLen);
        client.print(F(" HTTP/1.1\r\nHost: "));
        client.print(host);
        client.print(F("\r\nUser-Agent: GBS-Control"));
        if (apiHop) {
            client.print(F("\r\nAccept: application/octet-stream"));
        }
        client.print(F("\r\nRange: bytes="));
        client.print(offset);
        client.print('-');
        client.print(offset + len - 1);
        client.print(cdnHop ? F("\r\nConnection: keep-alive\r\n\r\n")
                            : F("\r\nConnection: close\r\n\r\n"));

        OtaHeadParser head;
        bool headComplete = otaReadHead(client, head);
#ifdef ESP8266
        // An oversized record kills the TLS engine before or while the head
        // arrives. Check it BEFORE the stale-keep-alive shortcut below, which
        // would otherwise mistake the dead connection for a clean server
        // close and reconnect with the same too-small buffer.
        {
            char errText[128] = {0};
            if (client.getLastSSLError(errText, sizeof(errText)) == BR_ERR_TOO_LARGE) {
                Serial.println(F("[OTA] TLS record too large (during head)"));
                otaDropConnection(client);
                return OTA_FETCH_TOOBIG;
            }
        }
#endif
        if (reused && !headComplete && head.httpCode == 0) {
            // Nothing came back on the kept-alive connection: the server had
            // closed it. Not an error - connect afresh without using a retry.
            Serial.println(F("[OTA] Kept-alive connection was closed, reconnecting"));
            skipReuse = true;
            continue;  // holder destroys the stale client
        }
        if (head.locationOverflow) {
            Serial.println(F("[OTA] Redirect URL too long"));
            client.stop();
            return OTA_FETCH_FAILED;
        }

        int code = head.httpCode;
        if ((code == 301 || code == 302 || code == 303 || code == 307 || code == 308)
            && head.location.length() > 0) {
            // Follow the redirect; remember the final URL for later chunks.
            if (head.location.startsWith("/")) {
                String base = (currentUrl.startsWith("https://")) ? "https://" : "http://";
                int slash = currentUrl.indexOf('/', 8);
                String origin = (slash > 0) ? currentUrl.substring(0, slash)
                                            : currentUrl;
                currentUrl = origin + head.location;
            } else {
                currentUrl = head.location;
            }
            Serial.print(F("[OTA] Redirect to host: "));
            int hEnd = currentUrl.indexOf('/', 8);
            Serial.println(currentUrl.substring(8, hEnd > 0 ? hEnd : currentUrl.length()));
            client.stop();
            continue;  // next hop
        }

        if (code == 401 || code == 403) {
            if (apiHop) {
                // 401/403 from api.github.com is not an expired signature
                // but the API rate limit: the right answer is the slow
                // retry schedule, not a pointless re-resolve loop.
                Serial.println(F("[OTA] API rejected the request (rate limit?)"));
                client.stop();
                return OTA_FETCH_SERVER;
            }
            // Signed CDN URL expired (or was rejected). Caller re-resolves.
            Serial.println(F("[OTA] CDN rejected the request (expired URL?)"));
            client.stop();
            return OTA_FETCH_EXPIRED;
        }

        if (code >= 500 || code == 429) {
            // Server-side trouble (edge/origin outage, overload, rate limit):
            // an immediate retry would only hammer a gateway that just timed
            // out. The slow schedule (and, at the release hop, the switch to
            // the API asset endpoint) lives in the download loop.
            Serial.print(F("[OTA] Server error "));
            Serial.print(code);
            Serial.print(F(" from "));
            Serial.println(host);
            client.stop();
            return OTA_FETCH_SERVER;
        }

        if (code != 206 && code != 200) {
            Serial.print(F("[OTA] Unexpected HTTP code: "));
            Serial.println(code);
            client.stop();
            return OTA_FETCH_FAILED;
        }
        // A 200 instead of 206 is acceptable when the request already
        // covers the whole file (single-request download on the ESP32).
        if (!headComplete || head.contentLength != len) {
            Serial.print(F("[OTA] Bad response head (code "));
            Serial.print(code);
            Serial.print(F(", length "));
            Serial.println(head.contentLength);
            Serial.println(F(")"));
            client.stop();
            return OTA_FETCH_FAILED;
        }

        // Stream exactly len bytes into the flash updater and the hash.
        uint8_t buff[512];
        long& got = *consumed;
        unsigned long lastData = millis();
        while (got < len) {
            if (millis() - lastData > OTA_CHUNK_TIMEOUT_MS) {
                Serial.println(F("[OTA] Chunk timeout"));
                client.stop();
                return OTA_FETCH_FAILED;
            }
            int avail = client.available();
            if (avail > 0) {
                size_t want = (size_t)len - (size_t)got;
                if (want > sizeof(buff)) {
                    want = sizeof(buff);
                }
                if ((size_t)avail < want) {
                    want = (size_t)avail;
                }
                int n = client.read(buff, want);
                if (n < 0) {
#ifdef ESP8266
                    char errText[128] = {0};
                    if (client.getLastSSLError(errText, sizeof(errText)) == BR_ERR_TOO_LARGE) {
                        Serial.println(F("[OTA] TLS record too large (in body)"));
                        otaDropConnection(client);
                        return OTA_FETCH_TOOBIG;
                    }
#endif
                    break;
                }
                if (n == 0) {
                    // available() can over-report between calls; just retry
                    delay(2);
#ifdef ESP8266
                    ESP.wdtFeed();
#endif
                    continue;
                }
#ifdef ESP32
                mbedtls_sha256_update((mbedtls_sha256_context*)shaCtx, buff, n);
#else
                br_sha256_update((br_sha256_context*)shaCtx, buff, n);
#endif
                if (Update.write(buff, (size_t)n) != (size_t)n) {
                    Serial.println(F("[OTA] Flash write error"));
                    Update.printError(Serial);
                    client.stop();
                    return OTA_FETCH_FAILED;
                }
                got += n;
                lastData = millis();
                yield();
#ifdef ESP8266
                ESP.wdtFeed();  // data may flow for seconds without a pause
#endif
                // Update.write() just erased+programmed a flash sector
                // (interrupts masked for tens of ms) whenever a 4 KB
                // boundary of the total update was crossed. Give the WiFi
                // driver a processing window before the next erase.
                if (((unsigned long)(offset + got) & 0xFFFUL) < (unsigned long)n) {
                    delay(1);
                }
            } else {
                if (!client.connected()) {
#if OTA_LOG_VERBOSE
                    Serial.println(F("[OTA] Connection closed by server"));
#endif
#ifdef ESP8266
                    // Say WHY the TLS engine closed (e.g. a record larger than
                    // the input buffer) instead of just that it did, and route
                    // the too-large case into the escalation instead of the
                    // plain retry logic (a retry with the same buffer would
                    // hit the same record again).
                    char sslErr[128] = {0};
                    int sslCode = client.getLastSSLError(sslErr, sizeof(sslErr));
#if OTA_LOG_VERBOSE
                    Serial.print(F("[OTA] TLS state: "));
                    Serial.print(sslErr[0] ? sslErr : "no error recorded");
                    Serial.print(F(", heap free "));
                    Serial.print(ESP.getFreeHeap());
                    Serial.print(F(", max block "));
                    Serial.println(ESP.getMaxFreeBlockSize());
#endif
                    if (sslCode == BR_ERR_TOO_LARGE) {
                        client.stop();
                        return OTA_FETCH_TOOBIG;
                    }
#endif
                    break;
                }
                delay(2);
                yield();
#ifdef ESP8266
                ESP.wdtFeed();  // see otaReadHead()
#endif
            }
        }
        if (got != len) {
            client.stop();
            Serial.print(F("[OTA] Chunk incomplete: "));
            Serial.print(got);
            Serial.print('/');
            Serial.println(len);
            return OTA_FETCH_FAILED;
        }
        if (cdnHop) {
            // Complete response, nothing left unread: keep the connection for
            // the next chunk (a stale one is detected there and replaced).
            otaCdnClient = holder.release();
        } else {
            client.stop();
        }
        return OTA_FETCH_OK;
    }
    Serial.println(F("[OTA] Too many redirects"));
    return OTA_FETCH_FAILED;
}

// ---------------------------------------------------------------------------
// Update check (unchanged trust model: api.github.com echoes MFLN 512 -
// wire-measured 2026-11-08 - so its records stay small and the 512-byte
// buffers of the check keep working).
// ---------------------------------------------------------------------------
#ifdef ESP8266
// Interrupted-check recovery. The runtime update check runs in loop()
// context right after the web services were torn down around it, but the
// async TCP stack keeps serving the browser's open connections from the SYS
// context while the check's HTTPS request blocks the main task. On a slow
// GitHub day (the 2026-11-08 outage) that window is long enough for the
// software watchdog to starve in SYS context - the 2026-11-08 10:17 log
// shows it: "Soft WDT reset", Exception (4), ctx: sys, inside
// pbuf_alloc_LWIP2 under ethernet_input -> AsyncClient::add -> tcp_write.
// The device reboots and the WebUI shows no check result at all.
//
// OtaCheckGuard (below) arms a flag in RTC user memory for the whole
// checkForUpdate() call; every regular return path clears it again. Only a
// crash mid-check leaves it armed across the reset. gbs-control.ino then
// re-runs the check early in the next setup() - the same clean boot
// environment the boot update downloads in, before the web services start -
// and hands the result to the WebUI OTA tile as if the click had succeeded.
// Recovery is bounded: after two interrupted recoveries the flag is
// disarmed and the user retries manually from the web UI.
// ---------------------------------------------------------------------------
#define OTA_CHECK_FLAG_MAGIC 0x4F544346UL  // "OTCF"
// RTC user memory is 512 bytes (words 0..127); the boot record takes bytes
// 0..435 (words 0..108, record layout v2 with the resume field). Words
// 120-121 (bytes 480..487) sit safely past it and inside the area the SDK
// hands to rtcUserMemoryWrite().
#define OTA_CHECK_FLAG_SLOT 120

struct OtaCheckFlagRecord {
    uint32_t magic;
    uint32_t retries;  // recovery attempts already spent on this check
};

static void otaCheckFlagWrite(uint32_t retries)
{
    OtaCheckFlagRecord rec;
    rec.magic = OTA_CHECK_FLAG_MAGIC;
    rec.retries = retries;
    ESP.rtcUserMemoryWrite(OTA_CHECK_FLAG_SLOT, (uint32_t*)&rec, sizeof(rec));
}

static void otaCheckFlagClear()
{
    OtaCheckFlagRecord rec;
    rec.magic = 0;
    rec.retries = 0;
    ESP.rtcUserMemoryWrite(OTA_CHECK_FLAG_SLOT, (uint32_t*)&rec, sizeof(rec));
}

bool FirmwareUpdater::checkInterruptedPending()
{
    OtaCheckFlagRecord rec;
    memset(&rec, 0, sizeof(rec));
    ESP.rtcUserMemoryRead(OTA_CHECK_FLAG_SLOT, (uint32_t*)&rec, sizeof(rec));
    if (rec.magic != OTA_CHECK_FLAG_MAGIC) {
        return false;
    }
    if (rec.retries >= 2) {
        // Too many interrupted checks in a row: stop auto-retrying and let
        // the user trigger a fresh one (which arms the flag again).
        otaCheckFlagClear();
        return false;
    }
    otaCheckFlagWrite(rec.retries + 1);
    return true;
}

// Planned boot check (see the header): the same flag the crash guard arms,
// with a fresh retry budget, set deliberately before a controlled restart.
void FirmwareUpdater::armCheckOnBoot()
{
    otaCheckFlagWrite(0);
}

// Arms the flag for the scope of one checkForUpdate() call; the destructor
// runs on every return path, so only a crash leaves it armed.
struct OtaCheckGuard {
    OtaCheckGuard()
    {
        OtaCheckFlagRecord rec;
        memset(&rec, 0, sizeof(rec));
        ESP.rtcUserMemoryRead(OTA_CHECK_FLAG_SLOT, (uint32_t*)&rec, sizeof(rec));
        otaCheckFlagWrite(rec.magic == OTA_CHECK_FLAG_MAGIC ? rec.retries : 0);
    }
    ~OtaCheckGuard() { otaCheckFlagClear(); }
};
#endif // ESP8266

FirmwareUpdater::UpdateStatus FirmwareUpdater::checkForUpdate(String& latestVersion) {
    _updateChecked = false;
    _latestVersion = "";
    _downloadUrl = "";
    _assetApiUrl = "";
    _expectedSha256 = "";
    _firmwareSize = 0;

#ifdef ESP8266
    // Crash recovery bookkeeping (see OtaCheckGuard above): armed until this
    // check finishes on any return path. On ESP32 there is nothing to do.
    OtaCheckGuard checkGuard;
#endif
    // Keep the radio fully awake for the whole check (OtaRadioGuard, same
    // as the download): framesync toggles WIFI_LIGHT_SLEEP while it measures
    // line periods, and a TLS handshake against a dozing radio loses beacons
    // and exhausts TX retries - the SDK driver's link-recovery path is where
    // the Exception-29 crash of 2026-11-07 (first check in that log) lives.
    // The guard restores whatever mode framesync had chosen.
    OtaRadioGuard radioGuard;

    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[OTA] WiFi not connected"));
        return WIFI_NOT_CONNECTED;
    }

    Serial.print(F("[OTA] WiFi connected, IP: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("[OTA] Free heap: "));
    Serial.println(ESP.getFreeHeap());

    // Ensure time is set for SSL certificate validation
    time_t now = time(nullptr);
    if (now < 8 * 3600 * 2) { // If time is not set (less than 16 hours from epoch)
        Serial.println(F("[OTA] Syncing time with NTP..."));
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        
        // Wait up to 5 seconds for time sync
        int timeout = 50; // 5 seconds
        while (time(nullptr) < 8 * 3600 * 2 && timeout > 0) {
            delay(100);
            timeout--;
        }
        
        now = time(nullptr);
        if (now < 8 * 3600 * 2) {
            Serial.println(F("[OTA] Failed to sync time - SSL may fail"));
        } else {
            Serial.print(F("[OTA] Time synced: "));
            Serial.println(ctime(&now));
        }
    }

    WiFiClientSecure client;
    HTTPClient https;

    // Use insecure mode to avoid certificate validation issues
    // GitHub's certificates change frequently and cert bundles are memory-intensive
    client.setInsecure();
    
#ifdef ESP8266
    // api.github.com honors the 512-byte MFLN that the 512-byte xmit
    // buffer negotiates, so every record of the check - including
    // edge-generated replies like a rate limit 403 - fits the tiny input
    // buffer. This is exactly the configuration that already worked in
    // v1.7.0; larger input buffers risk an OOM handshake on the
    // fragmented heap (a 4 KB buffer failed on the device).
    client.setBufferSizes(OTA_TLS_IN_BUF_GH, OTA_TLS_OUT_BUF);
#endif

    Serial.println(F("[OTA] Checking for updates..."));
    
    String url = String("https://") + GITHUB_API_HOST + GITHUB_API_PATH;
    Serial.print(F("[OTA] URL: "));
    Serial.println(url);
    
    if (!https.begin(client, url)) {
        Serial.println(F("[OTA] Failed to begin HTTPS connection"));
        return CHECK_FAILED;
    }

    // Configure HTTP client
    https.addHeader("User-Agent", "GBS-Control");
    https.addHeader("Accept", "application/vnd.github.v3+json");
    https.setTimeout(15000); // 15 second timeout
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Follow redirects
    https.setRedirectLimit(5); // Max 5 redirects

    Serial.println(F("[OTA] Sending GET request..."));
    int httpCode = https.GET();

    Serial.print(F("[OTA] HTTP code: "));
    Serial.println(httpCode);

    if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY && httpCode != HTTP_CODE_FOUND) {
        Serial.print(F("[OTA] HTTP error: "));
        Serial.println(httpCode);
        if (httpCode < 0) {
            Serial.println(F("[OTA] Connection failed"));
#ifdef ESP8266
            // Show WHY the TLS connection died: "unable to allocate
            // memory" (heap), "record is too large" (buffer vs. records),
            // or a transport error.
            char sslErr[128] = {0};
            client.getLastSSLError(sslErr, sizeof(sslErr));
            if (sslErr[0]) {
                Serial.print(F("[OTA] TLS error: "));
                Serial.println(sslErr);
            }
            Serial.print(F("[OTA] max free heap block: "));
            Serial.println(ESP.getMaxFreeBlockSize());
#endif
        }
        https.end();
        return CHECK_FAILED;
    }

    // Get the stream from HTTP client
    WiFiClient& stream = https.getStream();

    // Parse the JSON response from stream
    if (!parseReleaseInfo(stream, _latestVersion, _downloadUrl, _expectedSha256)) {
        Serial.println(F("[OTA] Failed to parse release info"));
        https.end();
        return CHECK_FAILED;
    }
    
    https.end();
    
    // SHA256 digest is required
    if (_expectedSha256.isEmpty()) {
        Serial.println(F("[OTA] Error: SHA256 digest not available in API"));
        return CHECK_FAILED;
    }
    
    Serial.print(F("[OTA] Expected SHA256: "));
    Serial.println(_expectedSha256);

    if (_firmwareSize <= 0) {
        Serial.println(F("[OTA] Error: asset size missing in API response"));
        return CHECK_FAILED;
    }

    latestVersion = _latestVersion;
    _updateChecked = true;

    Serial.print(F("[OTA] Current version: "));
    Serial.println(FIRMWARE_VERSION);
    Serial.print(F("[OTA] Latest version: "));
    Serial.println(_latestVersion);
    Serial.print(F("[OTA] Firmware size: "));
    Serial.println(_firmwareSize);

    // Compare versions
    // Note: We use simple string comparison (==) instead of semantic version comparison.
    // This allows rollback scenarios: if a newer version has critical bugs, you can
    // change the "latest" GitHub release to an older stable version, and users will
    // be able to update/downgrade to it. Any version mismatch triggers UPDATE_AVAILABLE.
    if (_latestVersion == FIRMWARE_VERSION) {
        Serial.println(F("[OTA] Already up to date"));
        return UP_TO_DATE;
    }

    Serial.println(F("[OTA] Update available!"));
    Serial.print(F("[OTA] Download URL: "));
    Serial.println(_downloadUrl);
    
    return UPDATE_AVAILABLE;
}

// ---------------------------------------------------------------------------
// Update download. Flow: verify preconditions, reserve the flash space, then
// pull the firmware in OTA_CHUNK_SIZE pieces (see the block comment at the
// top of this file for why chunking is required on the ESP8266, and how an
// oversized CDN record escalates the input buffer to the TLS maximum).
// ---------------------------------------------------------------------------
#ifdef ESP8266
// Cross-reboot resume of the boot download (see runBootUpdate below).
//
// Why: the 2026-11-08 fixes4 logs showed every clean retry restarting from
// byte 0 - five boots re-downloaded ~4.4 MB for an 883 KB image, and the
// second attempt spent the whole 4-retry budget (success on the very last
// one). Runs die from heap exhaustion after a handful of CDN reconnects, so
// individual runs are short (0-210 KB of progress in those logs) and the
// accumulated progress of the earlier runs was thrown away every reboot.
//
// How: performUpdate() tracks how many bytes are safely committed to the
// update partition (otaResumeWatermark - always a full flash sector behind
// the bytes handed to Update.write(), which may still buffer the last
// <= 4096 bytes). runBootUpdate() persists that watermark in the boot record; the
// next boot replays those bytes back from flash through Update.write() and
// the SHA256 context (performUpdate), which lands the write pointer on the
// resume position without re-downloading anything, and the download simply
// continues behind them. Update.begin() places the image at
// (FS_start - round_up(total, 4096)) - the same address on every boot of
// the same running firmware for the same total - so the committed prefix is
// found again at the very address computed from the record's size field.
//
// Safety: a resume that does not match the flash content (record from a
// different firmware state, layout change, power loss without RTC) is
// caught by the byte-0 image-magic check before the replay, and anything
// that slips through dies in the SHA256 comparison - runBootUpdate() then
// re-arms ONE retry from byte 0 instead of trusting the state again. The
// old firmware stays the boot target until Update.end() succeeds after a
// verified hash, so a bad resume can never brick the device.
//
// "in" is set by runBootUpdate() right before it calls performUpdate();
// "out" is reset at performUpdate() entry and read by runBootUpdate()
// afterwards. Plain statics: no heap, the download needs every byte.
// Cross-reboot resume switch (fixes7: back ON). The 19:09 run (resume on,
// pre-fixes6/7) showed every resumed boot failing the TLS heap probe of the
// first CDN hop ~460 bytes below a fresh boot - the other firmware variant
// concluded the replay was at fault and switched the resume off. The actual
// cause was the redirect dance running AFTER Update.begin() on the already
// split heap (api.github.com client churn + the ~900-byte signed URL
// Strings); the pre-resolution in performUpdate() now gives every boot -
// fresh or resumed - the same direct CDN connect on the fullest heap, and
// the "Heap after replay" line confirms the replay itself costs nothing.
// Without the resume every retry restarts at byte 0: the 19:41 run
// downloaded ~1.1 MB across seven boots (32-242 KB each) and abandoned at
// 24% because the progress was discarded on every reboot.
#define OTA_BOOT_RESUME 1
static uint32_t otaResumeStart = 0;       // in: byte offset to resume from
static bool     otaResumeArmed = false;   // in: the caller is the boot update
static uint32_t otaResumeWatermark = 0;   // out: bytes safely in flash
static bool     otaResumeWasUsed = false; // out: a replay actually happened
#endif

FirmwareUpdater::UpdateStatus FirmwareUpdater::performUpdate(void (*progressCallback)(int)) {
    if (!_updateChecked || _downloadUrl.isEmpty() || _firmwareSize <= 0) {
        Serial.println(F("[OTA] Must check for update first"));
        return CHECK_FAILED;
    }

    // Radio fully awake for the whole download (see OtaRadioGuard above);
    // restores the previous sleep mode on every exit, reboot or not.
    OtaRadioGuard radioGuard;

#ifdef ESP8266
    // Pre-allocate the 6.2 KB BearSSL stack thunk for the WHOLE download
    // (stack_thunk_add_ref in the core keeps it allocated while at least one
    // reference exists). Without this, every hop had to re-carve the 6200-
    // byte block from a tired heap first: in the v1.7.1 crash the thunk took
    // the last 6736-byte block, the SSL context another hole, and the
    // 1496-byte x509 context found nothing left -> OOM panic. Held from now
    // on, no connect below competes for it. The pre-check is required
    // because a failed thunk malloc aborts the device instead of throwing.
    if (ESP.getMaxFreeBlockSize() < OTA_THUNK_MIN_BLOCK) {
        Serial.print(F("[OTA] Not enough contiguous heap for the TLS stack (need "));
        Serial.print(OTA_THUNK_MIN_BLOCK);
        Serial.print(F(", max block "));
        Serial.println(ESP.getMaxFreeBlockSize());
        return HEAP_EXHAUSTED;
    }
    stack_thunk_add_ref();
    struct ThunkReleaseGuard {
        ~ThunkReleaseGuard() { stack_thunk_del_ref(); }
    } otaThunkGuard;
    Serial.print(F("[OTA] TLS stack held, heap now: free "));
    Serial.print(ESP.getFreeHeap());
    Serial.print(F(", max block "));
    Serial.println(ESP.getMaxFreeBlockSize());
#endif

    Serial.println(F("[OTA] Starting update..."));
    Serial.print(F("[OTA] Initial URL: "));
    Serial.println(_downloadUrl);
    Serial.print(F("[OTA] Free heap before update: "));
    Serial.println(ESP.getFreeHeap());
#ifdef ESP8266
    // Free heap and largest block are the download's currency: before each
    // connect, otaTlsHeapProbe() checks that the whole TLS connect chain fits.
    Serial.print(F("[OTA] Max free heap block: "));
    Serial.println(ESP.getMaxFreeBlockSize());
#endif

    // Keep local copies; the static state is reset afterwards.
    String releaseUrl = _downloadUrl;   // github.com URL for re-resolving
    String assetApiUrl = _assetApiUrl;  // API asset endpoint (fallback host)
    String expectedSha256 = _expectedSha256;
    long total = _firmwareSize;
    _downloadUrl = "";
    _assetApiUrl = "";
    _latestVersion = "";
    _expectedSha256 = "";
    _firmwareSize = 0;

    // A boot retry can start directly at the API asset endpoint (see
    // runBootUpdate): that endpoint IS the fallback, so the 5xx switch in
    // the download loop must not swap it onto itself - it would just reset
    // the retry budgets for the same host. Recognized by the URL: GitHub
    // asset API endpoints always live under https://api.github.com/.
    bool usingApiFallback = releaseUrl.startsWith("https://" GITHUB_API_HOST "/");
#ifdef ESP8266
    // Fresh CDN state for this download (see otaEscalateCdnBuffer): no
    // kept-alive connection, buffer rung or session-host note from an
    // earlier attempt - the TLS session cache does not survive a reboot.
    // Cleared BEFORE the pre-resolution below, which may establish a fresh
    // CDN session of its own.
    otaCdnRelease();
    otaCdnInBuf = OTA_TLS_IN_BUF_CDN;
    otaCdnEscalated = false;
    otaCdnSessionHost[0] = '\0';

    // fixes7: resolve the redirect chain on the fresh boot heap, BEFORE the
    // pad trick, Update.begin() and the resume replay split it into the two
    // ~7 KB holes the download has to live with (see otaResolveRedirects).
    // On success every chunk connect - the first of this attempt included -
    // goes straight to the CDN.
    String resolvedUrl = releaseUrl;
    bool haveResolvedUrl = false;
    Serial.println(F("[OTA] Pre-resolving the redirect chain"));
    if (otaResolveRedirects(resolvedUrl)) {
        haveResolvedUrl = true;
    } else if (!usingApiFallback && !assetApiUrl.isEmpty()) {
        // github.com's web edge is the ailing part (the 2026-11-08 outage
        // answered every release download with a 504): resolve via the
        // asset's API endpoint instead - same signed CDN URL, and the whole
        // chain stays on the API endpoint afterwards.
        Serial.println(F("[OTA] Release URL did not resolve, trying the API asset URL"));
        resolvedUrl = assetApiUrl;
        if (otaResolveRedirects(resolvedUrl)) {
            haveResolvedUrl = true;
            releaseUrl = assetApiUrl;
            usingApiFallback = true;
        }
    }
    if (!haveResolvedUrl) {
        Serial.println(F("[OTA] Pre-resolve failed, redirects follow inline"));
    }
#else
    String resolvedUrl = releaseUrl;  // ESP32: single request, redirects inline
    bool haveResolvedUrl = false;
#endif
#ifdef ESP8266
    // Update.begin() sizes its flash buffer from the free heap (core
    // Updater.cpp): 4096 bytes if more than 8192 bytes are free, otherwise the
    // low-memory fallback of 256 bytes. The 4096-byte buffer stays allocated
    // for the whole download and, in the boot-update logs, left the CDN hop
    // with a 5-9 KB largest block and ~9.5 KB free where its connect chain
    // needs ~11 KB (every run ended in a failed TLS heap check). The 256-byte
    // buffer is the core's own supported path: flash sectors are still erased
    // on each 4 KB boundary, data is just programmed in 256-byte pages, i.e.
    // more (shorter) flashWrite calls. Park most of the heap in a temporary
    // block so that begin() takes the small buffer, then give the block back
    // right away. If the block cannot be allocated, begin() simply takes the
    // 4096-byte buffer as before.
    void* otaPad = nullptr;
    {
        uint32_t freeNow = ESP.getFreeHeap();
        if (freeNow > 8192UL) {
            uint32_t padSize = freeNow - 8000UL;  // leaves ~8000 free, below the 8192 threshold
            if (padSize + 64UL <= ESP.getMaxFreeBlockSize()) {
                otaPad = malloc(padSize);
            }
        }
    }
#endif
    bool updateBegun = Update.begin(total);
#ifdef ESP8266
    if (otaPad) {
        free(otaPad);
        otaPad = nullptr;
    }
#endif
    if (!updateBegun) {
        Serial.print(F("[OTA] Not enough space. "));
#ifdef ESP8266
        Serial.print(F("Free: "));
        Serial.println(ESP.getFreeSketchSpace());
#else
        Serial.println(F("Details:"));
        Update.printError(Serial);
#endif
        return INSUFFICIENT_SPACE;
    }

#ifdef ESP8266
    // Update.begin() keeps its flash buffer on the heap for the whole download
    // (256 bytes with the pad trick above, 4096 without); this shows what is
    // left for the TLS connections.
    Serial.print(F("[OTA] Heap after Update.begin: free "));
    Serial.print(ESP.getFreeHeap());
    Serial.print(F(", max block "));
    Serial.println(ESP.getMaxFreeBlockSize());
#endif

    Serial.print(F("[OTA] Downloading "));
    Serial.print(total);
    Serial.print(F(" bytes in "));
#ifdef ESP8266
    Serial.print((total + OTA_CHUNK_SIZE - 1) / OTA_CHUNK_SIZE);
    Serial.print(F(" chunks of "));
    Serial.print(OTA_CHUNK_SIZE);
    Serial.println(F(" bytes"));
#else
    Serial.println(F("one request"));
#endif
    
#ifdef ESP8266
    // The software watchdog must stay ENABLED for the whole download.
    // Stopping it (ESP.wdtDisable, as this code did before) also stops the
    // SDK from feeding the ~6 s hardware watchdog - with the soft watchdog
    // stopped, delay()/yield() no longer pet the hardware one (see the
    // wdtDisable comment in the core's Esp.cpp) - and the explicit
    // ESP.wdtFeed() calls in this file cannot reach inside client.connect():
    // the DNS lookup (up to 5 s), the TCP connect (up to 5 s) and the TLS
    // handshake (up to 5 s) run there without a feed. On 2026-11-07 that
    // reset the device exactly there ("wdt reset", rst cause:4, chunk
    // @32768: the CDN host's DNS cache entry had expired, fresh lookup plus
    // TCP setup crossed the hardware window). With the soft watchdog
    // running, every blocking phase inside connect() yields internally and
    // those yields feed both watchdogs; a genuine hang now fails as a
    // recoverable "Soft WDT reset" instead of a hard reset without message.
#endif

#ifdef ESP32
    mbedtls_sha256_context sha256Ctx;
    mbedtls_sha256_init(&sha256Ctx);
    mbedtls_sha256_starts(&sha256Ctx, 0);
#else
    br_sha256_context sha256Ctx;
    br_sha256_init(&sha256Ctx);
#endif
    
#ifdef ESP8266
    // Cross-reboot resume (see the static block above): capture the armed
    // offset once, then reset the tracking for this run.
    uint32_t otaResumeLen = 0;
    if (otaResumeArmed && otaResumeStart > 0 && otaResumeStart < (uint32_t)total) {
        otaResumeLen = otaResumeStart;
    }
    otaResumeArmed = false;
    otaResumeStart = 0;
    otaResumeWatermark = 0;
    otaResumeWasUsed = false;
#endif
    
    // Start at the pre-resolved CDN URL when the fresh-heap resolution
    // succeeded (fixes7); otherwise at the entry URL, and otaFetchChunk()
    // follows the redirects inline as before.
    //
    // The resolved URL is MOVED into the working URL instead of copied:
    // the ~900 byte signed CDN URL would otherwise sit on the heap TWICE
    // for the whole download (resolvedUrl is never read again past this
    // point, but stays alive until the function returns) - roughly 900
    // bytes more free heap in the per-record resume phase, where the
    // download logs bottomed out at ~2.7-4.3 KB. Written as an explicit
    // if/else rather than `cond ? std::move(a) : b`: that ternary does
    // work (the mixed value categories collapse to a prvalue, so the
    // false branch copies), but the plain spelling does not depend on
    // that rule and keeps visible that releaseUrl must survive intact
    // here (the expired-signature restart and the API fallback below
    // both read it).
    String currentUrl;
    if (haveResolvedUrl) {
        currentUrl = std::move(resolvedUrl);
    } else {
        currentUrl = releaseUrl;
    }
    long offset = 0;
    int reResolves = 0;
    int attempts = 0;         // quick retries for the current chunk (WiFi hiccups)
    int srvRetries = 0;       // slow retries after server-side errors (5xx/429)
    int lastProgress = -1;
    OtaFetchResult fetch = OTA_FETCH_FAILED;
    bool heapExhausted = false;
#ifdef ESP8266
    int tooLargeStalls = 0;  // consecutive oversized-record drops w/o progress
#endif
    
#ifdef ESP8266
    if (otaResumeLen > 0) {
        // Replay the committed prefix from flash into the update stream and
        // the hash (see the static block above). Update.begin() just placed
        // the image at the same address the aborted attempt used, so the
        // bytes read back here are exactly the ones already counted.
        uint32_t roundedSize = ((uint32_t)total + 4095UL) & ~4095UL;
        uint32_t updateAddr = ((uint32_t)FS_start - 0x40200000UL) - roundedSize;
        bool replayOk = false;
        uint32_t firstWord = 0;
        if (ESP.flashRead(updateAddr, &firstWord, 4)
            && (firstWord & 0xFFUL) == 0xE9UL) {
            // byte 0 of the previous partial download is the ESP image magic;
            // anything else means the flash content does not match the record
            //
            // The prefix is read back ONE WHOLE 4 KB SECTOR at a time. The
            // replay rewrites the very flash it reads from, and Update.write()
            // erases a sector when it flushes the first page of it (core
            // Updater.cpp, _writeBuffer): with the small 256-byte updater
            // buffer, reading 512 bytes and writing them back wiped the rest
            // of that sector before it was read, so everything but the first
            // 512 bytes of every sector came back as 0xFF (host-simulated
            // against the core's write path: ~87% of a 68 KB prefix wrong,
            // hash mismatch at the end). With the whole sector in RAM before
            // the first write, the erase can no longer destroy unread data.
            // The 4 KB buffer lives only for the replay (freed before the
            // first connect); if it cannot be had, the download starts at 0.
            uint8_t* replayBuf = (uint8_t*)malloc(4096);
            if (replayBuf) {
                uint32_t done = 0;
                replayOk = true;
                while (done < otaResumeLen) {
                    uint32_t n = (otaResumeLen - done > 4096UL) ? 4096UL
                                                                : (otaResumeLen - done);
                    if (!ESP.flashRead(updateAddr + done, (uint32_t*)replayBuf, n)
                        || Update.write(replayBuf, n) != n) {
                        replayOk = false;
                        break;
                    }
                    br_sha256_update(&sha256Ctx, replayBuf, n);
                    done += n;
                    yield();
                    ESP.wdtFeed();
                }
                free(replayBuf);
            }
        }
        if (replayOk) {
            offset = (long)otaResumeLen;
            otaResumeWasUsed = true;
            // The prefix is committed again: a run that dies before it makes
            // any new progress must hand the same watermark to the next boot
            // instead of resetting it to 0.
            otaResumeWatermark = otaResumeLen;
            Serial.print(F("[OTA] Heap after replay: free "));
            Serial.print(ESP.getFreeHeap());
            Serial.print(F(", max block "));
            Serial.println(ESP.getMaxFreeBlockSize());
            Serial.print(F("[OTA] Resuming at byte "));
            Serial.print(offset);
            Serial.print(F(" of "));
            Serial.print(total);
            Serial.println(F(" (replayed from flash)"));
            if (progressCallback) {
                progressCallback((int)((offset * 100) / total));
            }
        } else {
            // No usable prefix on flash, or the replay hit a flash/write
            // error: start the whole download over. The replay may have
            // advanced Update's write pointer, so re-begin the update from
            // scratch - with the same pad trick as above, so the small flash
            // buffer is kept.
            Serial.println(F("[OTA] Resume not usable, downloading from 0"));
            Update.end(false);
            void* otaPad2 = nullptr;
            {
                uint32_t freeNow = ESP.getFreeHeap();
                if (freeNow > 8192UL) {
                    uint32_t padSize = freeNow - 8000UL;  // leaves ~8000 free
                    if (padSize + 64UL <= ESP.getMaxFreeBlockSize()) {
                        otaPad2 = malloc(padSize);
                    }
                }
            }
            bool updateBegun2 = Update.begin(total);
            if (otaPad2) {
                free(otaPad2);
            }
            if (!updateBegun2) {
                Serial.print(F("[OTA] Not enough space (restart). Free: "));
                Serial.println(ESP.getFreeSketchSpace());
                return INSUFFICIENT_SPACE;
            }
            br_sha256_init(&sha256Ctx);
        }
    }
#endif
    
    while (offset < total) {
        long thisLen = (total - offset < OTA_CHUNK_SIZE) ? (total - offset) : OTA_CHUNK_SIZE;

        long consumed = 0;
        fetch = otaFetchChunk(currentUrl, offset, thisLen, &sha256Ctx, &consumed);
        offset += consumed;  // already in flash/hash, even if the chunk failed half-way
#ifdef ESP8266
        // Track the highest watermark safely committed to flash. Update
        // keeps up to one buffer of the bytes handed to it unflushed (256
        // bytes with the small buffer, a whole 4096-byte sector if the
        // 4096-byte buffer had to be taken), so stay a full sector behind;
        // rounded down to a sector boundary the replay then reads whole
        // sectors. runBootUpdate() persists this for the next boot's resume
        // (see the static block above).
        if (consumed > 0) {
            uint32_t passed = (uint32_t)offset;
            uint32_t safe = (passed > 4096UL) ? ((passed - 4096UL) & ~4095UL) : 0UL;
            if (safe > otaResumeWatermark) {
                otaResumeWatermark = safe;
            }
        }
#endif
        if (consumed > 0) {
            attempts = 0;    // progress was made
            srvRetries = 0;
        }
        
        if (fetch == OTA_FETCH_OK) {
            attempts = 0;
            srvRetries = 0;
#ifdef ESP8266
            tooLargeStalls = 0;  // the chunk went through
#endif
            int progress = (int)((offset * 100) / total);
                if (progress != lastProgress) {
                    lastProgress = progress;
                    if (progressCallback) {
                        progressCallback(progress);
                    }
                    if (progress % 10 == 0) {
                        Serial.print(F("[OTA] Progress: "));
                        Serial.print(progress);
                        Serial.print(F("% ("));
                    Serial.print(offset);
                        Serial.print(F(" / "));
                    Serial.print(total);
                        Serial.print(F("), heap "));
                        Serial.println(ESP.getFreeHeap());
                    }
                }
            continue;
                }

        if (fetch == OTA_FETCH_EXPIRED && reResolves < 3) {
            // The signed CDN URL expired mid-download: start over from the
            // release URL (fresh signature) and retry the same offset.
            reResolves++;
            Serial.println(F("[OTA] Restarting from release URL (new signature)"));
            currentUrl = releaseUrl;
            continue;
        }

#ifdef ESP8266
        if (fetch == OTA_FETCH_HEAP) {
            // A retry cannot defragment the heap (the updater buffer and the
            // thunk stay allocated); only a restart with a fresh boot heap
            // can. Report it to the caller instead of burning the retries.
            heapExhausted = true;
            break;
        }

        if (fetch == OTA_FETCH_TOOBIG) {
            // The CDN sent an encrypted record larger than the input buffer.
            // otaFetchChunk() dropped the connection in a controlled way
            // (RST, see otaDropConnection); every byte before the oversized
            // record is already in flash and in the hash (offset advanced
            // above). This is not a network failure: reconnecting with the
            // same too-small buffer would run into the same record again.
            // Escalate one rung up the buffer ladder (if the heap allows)
            // and reconnect immediately - no retry delay, no attempt spent.
            // When no bigger rung fits, the byte-exact resume grinds on at
            // the current rung, one drop per oversized record.
            otaEscalateCdnBuffer();
            if (consumed > 0) {
                tooLargeStalls = 0;  // progress was made
            } else if (++tooLargeStalls >= 6) {
                Serial.println(F("[OTA] Oversized records without progress, aborting"));
                break;
            }
            delay(250);
            ESP.wdtFeed();
            continue;
        }
#endif
        
        // Dead or ailing release hop: the github.com web edge is separate
        // infrastructure from api.github.com (the 2026-11-08 outage answered
        // every release download with a 504 while the API kept answering),
        // so switch the whole resolution chain to the asset's API endpoint
        // once. A server-side error switches immediately; a plain network
        // failure switches after the quick retries had their chance.
        if (!usingApiFallback && !assetApiUrl.isEmpty() && currentUrl == releaseUrl
            && (fetch == OTA_FETCH_SERVER || attempts >= 2)) {
            usingApiFallback = true;
            releaseUrl = assetApiUrl;
            currentUrl = assetApiUrl;
            attempts = 0;      // fresh quick-retry budget for the new host
            srvRetries = 0;
            Serial.println(F("[OTA] Release host unavailable, switching to the API asset URL"));
            continue;  // no connection spent: the next fetch starts at the API hop
        }

        if (fetch == OTA_FETCH_SERVER) {
            // Server-side errors recover on the order of tens of seconds;
            // the slow schedule waits that out instead of burning heap and
            // rate limit on 1.5 s reconnect cycles.
            static const uint16_t srvWaitMs[6] = { 3000, 8000, 20000, 45000, 60000, 60000 };
            if (srvRetries < 6) {
                Serial.print(F("[OTA] Server error, retrying in "));
                Serial.print(srvWaitMs[srvRetries] / 1000);
                Serial.println(F("s"));
                otaWaitFed(srvWaitMs[srvRetries]);
                srvRetries++;
                continue;
            }
            break;  // the server stayed down for this whole boot
        }

        // Transient network error: retry the same chunk a couple of times
        // before giving up (a lost chunk must not kill the whole update).
        if (attempts < 2) {
            attempts++;
            Serial.println(F("[OTA] Retrying chunk..."));
            otaWaitFed(1500);
            continue;
        }
        break;  // give up on this chunk
    }

    // Close the kept-alive CDN connection (also on every failure exit above).
    otaCdnRelease();

    if (fetch != OTA_FETCH_OK || offset != total) {
        Serial.println(heapExhausted ? F("[OTA] Download aborted: heap exhausted")
                                     : F("[OTA] Download incomplete"));
        Update.end(false); // Abort update
        return heapExhausted ? HEAP_EXHAUSTED : DOWNLOAD_FAILED;
    }
    
    // Verify SHA256 hash over the concatenated chunks
    uint8_t calculatedHash[32];
#ifdef ESP32
    mbedtls_sha256_finish(&sha256Ctx, calculatedHash);
    mbedtls_sha256_free(&sha256Ctx);
#else
    br_sha256_out(&sha256Ctx, calculatedHash);
#endif
    
    // Convert calculated hash to hex string
    String calculatedHashHex = "";
    for (int i = 0; i < 32; i++) {
        if (calculatedHash[i] < 0x10) {
            calculatedHashHex += "0";
        }
        calculatedHashHex += String(calculatedHash[i], HEX);
    }
    calculatedHashHex.toLowerCase();
    
    Serial.print(F("[OTA] Calculated SHA256: "));
    Serial.println(calculatedHashHex);
    Serial.print(F("[OTA] Expected SHA256:   "));
    Serial.println(expectedSha256);
    
    if (calculatedHashHex != expectedSha256) {
        Serial.println(F("[OTA] SHA256 checksum mismatch! Aborting update."));
        Update.end(false); // Abort update
        return CHECKSUM_FAILED;
    }
    
    Serial.println(F("[OTA] SHA256 checksum verified"));
    
    if (!Update.end()) {
        Serial.println(F("[OTA] Update end failed"));
        Update.printError(Serial);
        return FLASH_FAILED;
    }
    
    if (!Update.isFinished()) {
        Serial.println(F("[OTA] Update not finished"));
        return FLASH_FAILED;
    }

    Serial.println(F("[OTA] Update successful!"));
    Serial.println(F("[OTA] Ready to reboot (will be handled by menu)"));
    
    return SUCCESS;
}

FirmwareUpdater::UpdateStatus FirmwareUpdater::checkAndUpdate(void (*progressCallback)(int)) {
    String latestVersion;
    UpdateStatus checkStatus = checkForUpdate(latestVersion);
    
    if (checkStatus == UPDATE_AVAILABLE) {
        return performUpdate(progressCallback);
    }
    
    return checkStatus;
}

// ---------------------------------------------------------------------------
// Boot-time update mode (ESP8266)
//
// Why this exists: the ESP8266 BearSSL stack allocates a 6200-byte "stack
// thunk" (core StackThunk.cpp) when the first WiFiClientSecure is constructed,
// plus a 3424-byte client context, a 1496-byte x509 validator and the TLS
// record buffers on every connect(), while Update.begin() holds its flash
// buffer (4096 bytes by default) for the whole download. Next to the running web
// services that does not fit into the ~15.7 KB runtime heap - the device
// aborted with "Unhandled C++ exception: OOM" on the very first chunk. Early
// in setup() the heap is large enough, so a web- or menu-triggered update
// arms a record in RTC user memory (survives the soft restart, not a power
// cycle), restarts, and the download runs via runBootUpdate() before the web
// services start. The record is consumed on the first read, so a crash during
// the download can never cause a reboot loop.
//
// The record also carries the release metadata (URL / SHA256 / size) from
// the check that prompted the request. The boot update then downloads
// directly, WITHOUT repeating the api.github.com call: that check (HTTPClient
// + NTP + TLS + JSON streaming) is what fragmented the boot heap down to a
// 8024-byte largest block in the v1.7.1 crash log - 2.2 KB of which were
// still allocated when the CDN connect needed ~13.7 KB. Skipping it keeps
// the boot heap in one piece for the download. Only when the record has no
// valid metadata (flag armed without a preceding check) does the boot update
// fall back to the old check-first flow.
//
// Since fixes5 the record additionally carries a download watermark: when a
// clean retry re-arms it, runBootUpdate() notes how many bytes of the image
// the failed attempt had already committed to flash. The next boot replays
// that prefix back from flash (performUpdate) and continues the download
// behind it, so retries accumulate progress instead of restarting from byte
// 0 (see the resume block above performUpdate for the mechanics and the
// safety net).
// ---------------------------------------------------------------------------
#ifdef ESP8266
#define OTA_BOOT_MAGIC  0x4F544132UL  // "OTA2" - record layout v2 (resume field
                                     // added in fixes5). Records armed by older
                                     // firmware are ignored cleanly instead of
                                     // being misread (their CRC covers fewer
                                     // fields, their magic differs).
#define OTA_BOOT_CHECK  (OTA_BOOT_MAGIC ^ 0xFFFFFFFFUL)

// Clean-retry budget of the boot update. Every retry is a fresh boot with a
// fresh heap; the record is consumed on read either way, so a crash can
// never reboot-loop. Two budgets (fixes7): attempts that made NO progress
// beyond the armed watermark burn the small one - something is wrong beyond
// the CDN's mood. Attempts that ADVANCED the watermark get the large one:
// with the cross-reboot resume every such boot is permanent progress, and a
// grinding download (the 19:41 run advanced 32-242 KB per boot against
// 884 KB total, all discarded) must not lose its budget to the grind.
#define OTA_BOOT_MAX_RETRIES              10
#define OTA_BOOT_MAX_RETRIES_PROGRESS     24

struct OtaBootRecord {
    uint32_t magic;
    uint32_t check;    // separates the record from random RTC garbage at cold boot
    uint32_t size;     // firmware size in bytes; 0 = no metadata, boot re-checks
    uint32_t crc;      // FNV-1a over the payload fields
    uint8_t  retries;  // restart retries already spent on heap exhaustion
    char     url[160];         // github.com release URL (not the signed CDN URL)
    char     assetApiUrl[160]; // api.github.com asset endpoint (web-edge fallback)
    char     sha256[72];   // expected digest, lowercase hex
    char     version[20];  // release tag, for logs and the WebUI tile
    uint32_t resumeOffset; // committed bytes of the previous attempt (fixes5;
                           // 0 = start from byte 0; < size always)
};

// The record must stay clear of the check-recovery flag at RTC words 120-121
// (bytes 480..487) and inside the 512 RTC bytes the SDK hands out.
static_assert(sizeof(OtaBootRecord) <= 476, "boot record grew into the check flag slot");

// Boot metadata handed from bootUpdatePending() to runBootUpdate() (plain
// static buffers - no heap, the download needs every contiguous byte).
static char otaBootUrl[sizeof(((OtaBootRecord*)0)->url)] = {0};
static char otaBootAssetApiUrl[sizeof(((OtaBootRecord*)0)->assetApiUrl)] = {0};
static char otaBootSha[sizeof(((OtaBootRecord*)0)->sha256)] = {0};
static char otaBootVersion[sizeof(((OtaBootRecord*)0)->version)] = {0};
static uint32_t otaBootSize = 0;
static uint8_t otaBootRetries = 0;
static uint32_t otaBootResumeOffset = 0;  // validated resume watermark
static bool otaBootMetaValid = false;

// FNV-1a over the payload fields (everything except magic/check/crc itself).
static uint32_t otaBootCrc(const OtaBootRecord* rec)
{
    uint32_t hash = 2166136261UL;
    const uint8_t* p;
    p = (const uint8_t*)&rec->size;
    for (unsigned i = 0; i < sizeof(rec->size); i++) { hash = (hash ^ p[i]) * 16777619UL; }
    p = (const uint8_t*)&rec->retries;
    for (unsigned i = 0; i < sizeof(rec->retries); i++) { hash = (hash ^ p[i]) * 16777619UL; }
    p = (const uint8_t*)rec->url;
    for (unsigned i = 0; i < sizeof(rec->url); i++) { hash = (hash ^ p[i]) * 16777619UL; }
    p = (const uint8_t*)rec->assetApiUrl;
    for (unsigned i = 0; i < sizeof(rec->assetApiUrl); i++) { hash = (hash ^ p[i]) * 16777619UL; }
    p = (const uint8_t*)rec->sha256;
    for (unsigned i = 0; i < sizeof(rec->sha256); i++) { hash = (hash ^ p[i]) * 16777619UL; }
    p = (const uint8_t*)rec->version;
    for (unsigned i = 0; i < sizeof(rec->version); i++) { hash = (hash ^ p[i]) * 16777619UL; }
    p = (const uint8_t*)&rec->resumeOffset;
    for (unsigned i = 0; i < sizeof(rec->resumeOffset); i++) { hash = (hash ^ p[i]) * 16777619UL; }
    return hash;
}

static void otaWriteBootRecord(const char* url, const char* assetApiUrl,
                               const char* sha256, uint32_t size,
                               const char* version, uint8_t retries,
                               uint32_t resumeOffset)
{
    OtaBootRecord rec;
    memset(&rec, 0, sizeof(rec));
    rec.magic = OTA_BOOT_MAGIC;
    rec.check = OTA_BOOT_CHECK;
    rec.retries = retries;
    rec.size = size;
    if (resumeOffset > 0 && size > 0 && resumeOffset < size) {
        rec.resumeOffset = resumeOffset;
    }
    if (url) strncpy(rec.url, url, sizeof(rec.url) - 1);
    if (assetApiUrl) strncpy(rec.assetApiUrl, assetApiUrl, sizeof(rec.assetApiUrl) - 1);
    if (sha256) strncpy(rec.sha256, sha256, sizeof(rec.sha256) - 1);
    if (version) strncpy(rec.version, version, sizeof(rec.version) - 1);
    rec.crc = otaBootCrc(&rec);
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rec, sizeof(rec));
}

void FirmwareUpdater::requestBootUpdate()
{
    // Called after a check found UPDATE_AVAILABLE: the static members still
    // hold the release info (they survive until the next check / update).
    // Persist it so the boot update can skip the heap-fragmenting API call.
    // Anything that does not fit the fixed-size RTC record falls back to the
    // flag-only record (the boot update then checks via the API itself)
    // instead of storing a truncated URL.
    const char* url = nullptr;
    const char* assetApi = nullptr;
    const char* sha = nullptr;
    const char* ver = nullptr;
    uint32_t size = 0;
    if (!_downloadUrl.isEmpty() && _firmwareSize > 0 && !_expectedSha256.isEmpty()
        && (unsigned)_downloadUrl.length() < sizeof(((OtaBootRecord*)0)->url)
        && (unsigned)_expectedSha256.length() < sizeof(((OtaBootRecord*)0)->sha256)
        && (_latestVersion.isEmpty()
            || (unsigned)_latestVersion.length() < sizeof(((OtaBootRecord*)0)->version))) {
        url = _downloadUrl.c_str();
        sha = _expectedSha256.c_str();
        size = (uint32_t)_firmwareSize;
        if (!_latestVersion.isEmpty()) {
            ver = _latestVersion.c_str();
        }
    }
    // The API fallback endpoint is optional: without it the boot update
    // simply has no alternative when the github.com web edge is down.
    if (!_assetApiUrl.isEmpty()
        && (unsigned)_assetApiUrl.length() < sizeof(((OtaBootRecord*)0)->assetApiUrl)) {
        assetApi = _assetApiUrl.c_str();
    }
    // A fresh update request always starts from byte 0 - a resume watermark
    // may only come from the retry bookkeeping in runBootUpdate().
    otaWriteBootRecord(url, assetApi, sha, size, ver, 0, 0);
}

bool FirmwareUpdater::bootUpdatePending()
{
    OtaBootRecord rec;
    memset(&rec, 0, sizeof(rec));
    ESP.rtcUserMemoryRead(0, (uint32_t*)&rec, sizeof(rec));
    if (rec.magic != OTA_BOOT_MAGIC || rec.check != OTA_BOOT_CHECK) {
        return false;
    }
    clearBootUpdate();  // exactly one attempt per armed record

    otaBootMetaValid = false;
    otaBootRetries = rec.retries;
    otaBootResumeOffset = 0;
    if (rec.size > 0 && rec.size < 0x400000UL
        && strncmp(rec.url, "https://", 8) == 0
        && strlen(rec.sha256) == 64
        && rec.crc == otaBootCrc(&rec)) {
        strncpy(otaBootUrl, rec.url, sizeof(otaBootUrl) - 1);
        otaBootUrl[sizeof(otaBootUrl) - 1] = 0;
        strncpy(otaBootAssetApiUrl, rec.assetApiUrl, sizeof(otaBootAssetApiUrl) - 1);
        otaBootAssetApiUrl[sizeof(otaBootAssetApiUrl) - 1] = 0;
        strncpy(otaBootSha, rec.sha256, sizeof(otaBootSha) - 1);
        otaBootSha[sizeof(otaBootSha) - 1] = 0;
        strncpy(otaBootVersion, rec.version, sizeof(otaBootVersion) - 1);
        otaBootVersion[sizeof(otaBootVersion) - 1] = 0;
        otaBootSize = rec.size;
        otaBootResumeOffset = (rec.resumeOffset < rec.size) ? rec.resumeOffset : 0;
        otaBootMetaValid = true;
    }
    return true;
}

void FirmwareUpdater::clearBootUpdate()
{
    OtaBootRecord rec;
    memset(&rec, 0, sizeof(rec));
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rec, sizeof(rec));
}

FirmwareUpdater::UpdateStatus FirmwareUpdater::runBootUpdate(void (*progressCallback)(int))
{
    // The caller (gbs-control.ino, early setup()) has already brought WiFi up.
    if (WiFi.status() != WL_CONNECTED) {
        return WIFI_NOT_CONNECTED;
    }

    if (otaBootMetaValid) {
        // Straight to the download with the persisted release info - no
        // api.github.com call, no NTP sync, no JSON parsing between the boot
        // heap and the TLS download that needs it (see the block comment
        // above). The SHA256 from the check still protects the image.
        Serial.println(F("[OTA] Boot update: using release info from the update request"));
        Serial.print(F("[OTA] Boot update: URL "));
        Serial.println(otaBootUrl);
        if (otaBootRetries > 0) {
            Serial.print(F("[OTA] Boot update: clean retry "));
            Serial.println(otaBootRetries);
        }
        _downloadUrl = otaBootUrl;
        _assetApiUrl = otaBootAssetApiUrl;
        // A clean retry has proof that the github.com web edge is ailing
        // this session: the attempt that armed this reboot just failed there
        // (the 2026-11-08 outage answered every release download with a 504,
        // four times in a row). Start the chain directly at the API asset
        // endpoint instead: one TLS session less to pay for out of the boot
        // heap, one round trip less, and the first chunk skips straight to
        // the 302 redirect it needs anyway. Without asset metadata (or on
        // the first attempt) the release URL stays the entry point and the
        // in-download fallback switches after a 5xx as before.
        if (otaBootRetries > 0 && otaBootAssetApiUrl[0] != '\0') {
            Serial.println(F("[OTA] Boot update: release host failed before, starting at the API asset URL"));
            _downloadUrl = otaBootAssetApiUrl;
        }
        _expectedSha256 = otaBootSha;
        _firmwareSize = (long)otaBootSize;
        _latestVersion = otaBootVersion;
        _updateChecked = true;
    } else {
        Serial.println(F("[OTA] Boot update: checking..."));
        String latestVersion;
        UpdateStatus st = checkForUpdate(latestVersion);
        if (st != UPDATE_AVAILABLE) {
            // UP_TO_DATE / CHECK_FAILED / ...: report it, boot continues normally.
            return st;
        }
    }

    Serial.println(F("[OTA] Boot update: downloading and flashing..."));
    if (OTA_BOOT_RESUME && otaBootResumeOffset > 0) {
        // Cross-reboot resume (see the static block above performUpdate):
        // arm the committed watermark of the previous attempt; the download
        // replays those bytes from flash and continues behind them.
        otaResumeStart = otaBootResumeOffset;
        otaResumeArmed = true;
        Serial.print(F("[OTA] Boot update: resuming the previous download at byte "));
        Serial.println(otaBootResumeOffset);
    }
    UpdateStatus st = performUpdate(progressCallback);
    if (st == SUCCESS) {
        Serial.println(F("[OTA] Boot update finished, rebooting into new firmware"));
        delay(250);
        ESP.restart();
    } else if (otaBootMetaValid
               && (st == HEAP_EXHAUSTED || st == DOWNLOAD_FAILED)
               && otaBootRetries < ((otaResumeWatermark > otaBootResumeOffset)
                                        ? OTA_BOOT_MAX_RETRIES_PROGRESS
                                        : OTA_BOOT_MAX_RETRIES)) {
        // HEAP_EXHAUSTED: a retry in this boot cannot defragment the heap
        // (the updater buffer and the thunk stay allocated); only a fresh
        // boot heap can. DOWNLOAD_FAILED: the stream died (stall, link
        // loss, or the oversized-record drops stopped making progress).
        // Every fresh boot is a new roll over the CDN's record mix: the
        // 2026-11-07 download survived all 26 drops, the 2026-11-08 one
        // died on all three attempts (3, 9 and 5 drops in). The RST drop
        // close and the 4096 rung improved the odds per attempt, but the
        // CDN's mood varies from minute to minute - the budget above keeps
        // the overall odds high while still bounding a persistent problem
        // (the record is consumed on read either way, so a crash can never
        // cause a reboot loop).
        // Instead of restarting from byte 0, the retry arms the committed
        // watermark (otaResumeWatermark, a full flash sector behind the
        // bytes handed to Update.write()): the next boot replays those
        // bytes from flash and continues behind them, so even short-lived
        // attempts make permanent progress - the 2026-11-08 13:15 run
        // needed five boots for ~1.1 MB of actual payload.
        bool madeProgress = (otaResumeWatermark > otaBootResumeOffset);
        Serial.print(F("[OTA] "));
        Serial.print(st == HEAP_EXHAUSTED ? F("Heap exhausted")
                                          : F("Download failed"));
        Serial.print(F(", rebooting for a clean retry ("));
        Serial.print(otaBootRetries + 1);
        Serial.print(F("/"));
        Serial.print(madeProgress ? OTA_BOOT_MAX_RETRIES_PROGRESS
                                  : OTA_BOOT_MAX_RETRIES);
        Serial.println(madeProgress ? F(", progress kept)")
                                    : F(")"));
        otaWriteBootRecord(otaBootUrl,
                           otaBootAssetApiUrl[0] ? otaBootAssetApiUrl : nullptr,
                           otaBootSha, otaBootSize, otaBootVersion,
                           otaBootRetries + 1,
                           OTA_BOOT_RESUME ? otaResumeWatermark : 0);
        delay(250);
        ESP.restart();
    } else if (otaBootMetaValid && otaBootRetries < OTA_BOOT_MAX_RETRIES
               && st == CHECKSUM_FAILED && otaResumeWasUsed) {
        // A resumed download whose hash does not add up: the resume state
        // itself is the prime suspect (stale record, flash layout change),
        // not the network - a plain retry would resume into the same bad
        // state. Restart once from byte 0 with the resume disabled.
        Serial.println(F("[OTA] Checksum mismatch after a resumed download, retrying from byte 0"));
        otaWriteBootRecord(otaBootUrl,
                           otaBootAssetApiUrl[0] ? otaBootAssetApiUrl : nullptr,
                           otaBootSha, otaBootSize, otaBootVersion,
                           otaBootRetries + 1, 0);
        delay(250);
        ESP.restart();
    }
    if (st == HEAP_EXHAUSTED || st == DOWNLOAD_FAILED) {
        // Out of clean retries (or armed without metadata): give up for
        // good, boot normally and let the user re-trigger from the WebUI
        // once the server side has recovered.
        Serial.println(F("[OTA] Boot update abandoned, booting normally - retry from the web UI later"));
    }
    return st;
}
#else
// ESP32: enough RAM, the download runs directly at runtime (WebOta and the
// OLED menu call performUpdate); the boot mode is ESP8266-only.
void FirmwareUpdater::requestBootUpdate() {}
bool FirmwareUpdater::bootUpdatePending() { return false; }
void FirmwareUpdater::clearBootUpdate() {}
bool FirmwareUpdater::checkInterruptedPending() { return false; }
void FirmwareUpdater::armCheckOnBoot() {}
FirmwareUpdater::UpdateStatus FirmwareUpdater::runBootUpdate(void (*progressCallback)(int))
{
    (void)progressCallback;
    return CHECK_FAILED;
}
#endif

// Reads a JSON number (e.g. after "size":) from the stream.
static long otaReadJsonNumber(Stream& stream)
{
    String num;
    while (stream.available()) {
        int c = stream.peek();
        if (c >= '0' && c <= '9') {
            num += (char)c;
            stream.read();
        } else {
            break;
        }
    }
    return num.isEmpty() ? 0 : atol(num.c_str());
}

bool FirmwareUpdater::parseReleaseInfo(Stream& stream, String& version, String& downloadUrl, String& expectedSha256) {
    // 1. Find tag_name
    if (!stream.find("\"tag_name\":")) {
        return false;
    }
    version = readJsonString(stream);
    if (version.isEmpty()) {
        return false;
    }

    // 2. Find assets array
    if (!stream.find("\"assets\":")) {
        return false;
    }

    // 3. Find our firmware asset and remember its API endpoint. GitHub's
    //    asset objects carry their API url ("url" key, the
    //    releases/assets/<id> endpoint) before the human-readable "name",
    //    so walking url -> name pairs identifies each asset without
    //    buffering the JSON. The API endpoint is the download fallback for
    //    when github.com's web edge fails (see performUpdate): it redirects
    //    to the same signed CDN URL. If the walk ever comes up empty the
    //    download simply stays on the release URL.
#ifdef ESP32
    String firmwareName = "firmware_esp32.bin";
#else
    String firmwareName = "firmware.bin";
#endif
    bool matchedAsset = false;
    String assetApiUrl;
    while (stream.available()) {
        if (!stream.find("\"url\":")) {
            break;  // no (further) asset objects
        }
        String u = readJsonString(stream);
        if (u.indexOf("/releases/assets/") < 0) {
            continue;  // uploader/user objects carry api urls too - only the
                       // asset endpoint points at /releases/assets/<id>
        }
        assetApiUrl = u;
        if (!stream.find("\"name\":")) {
            break;
        }
        String n = readJsonString(stream);
        if (n == firmwareName) {
            _assetApiUrl = assetApiUrl;
            matchedAsset = true;
            break;  // the stream now sits right behind the asset name: the
                    // size/digest/url scan below continues from here
        }
        assetApiUrl = "";  // that url belonged to a different asset
    }
    if (!matchedAsset) {
        Serial.println(F("[OTA] firmware not found in assets"));
        return false;
    }

    // 4. Find size, digest and browser_download_url sequentially.
    // Stream.find() consumes the stream, so the scan order must match the
    // GitHub API field order inside the asset object:
    //   "name" ... "size" ... "digest" ... "browser_download_url".
    // This stream-based approach prevents OOM on ESP8266 by avoiding
    // buffering the full JSON.
    
    bool foundUrl = false;
    bool foundDigest = false;
    bool foundSize = false;
    
    while (stream.available()) {
        if (!foundSize) {
            if (stream.find("\"size\":")) {
                long sz = otaReadJsonNumber(stream);
                if (sz > 0) {
                    _firmwareSize = sz;
                    foundSize = true;
                }
            }
        }

        if (!foundDigest) {
             if (stream.find("\"digest\":")) {
                 String digestVal = readJsonString(stream);
                 if (digestVal.startsWith("sha256:")) {
                     expectedSha256 = digestVal.substring(7);
                     expectedSha256.toLowerCase();
                     foundDigest = true;
                 }
             }
        }
        
        if (!foundUrl) {
            if (stream.find("\"browser_download_url\":")) {
                downloadUrl = readJsonString(stream);
                foundUrl = true;
            }
        }
        
        if (foundUrl && foundDigest && foundSize) break;
        
        // Break if stream ends to avoid infinite loop
        if (!stream.available()) break;
    }
    
    return !downloadUrl.isEmpty();
}

String FirmwareUpdater::readJsonString(Stream& stream) {
    // Expecting: "value" or value (if boolean/number, but here we read strings)
    // We assume we just matched "key":
    // So next char should be " or whitespace then "
    
    // Skip until "
    while (stream.available()) {
        char c = stream.peek();
        if (c == '"') {
            stream.read(); // Consume opening quote
            break;
        } else if (isSpace(c)) {
            stream.read(); // Consume whitespace
        } else {
            // Unexpected char or maybe null?
            return ""; 
        }
    }
    
    return stream.readStringUntil('"');
}
