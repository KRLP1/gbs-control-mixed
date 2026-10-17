# Changelog

## [v1.3.0] – P0 Stability Fixes from rpf16rj nx-improvements

### Added
- New file `src/commands/CommandQueue.h`: SPSC ring buffer replacing shared
  String buffers that were mutated by AsyncWebServer callbacks (caused frozen
  UI, fork Kwakx#4)
- Bounded `serialCommandQueue` and `userCommandQueue` (64-slot each) for
  safe async->loop() command dispatch

### Changed
- WiFi output power raised from 18.0 dBm to 20.5 dBm (max) to better
  overcome GBS-C EMI on problematic PAL presets
- `webSocket.loop()` now runs on ESP8266 (was ESP32-only) — fixes WebSocket
  disconnects on ESP8266
- I2C `rawRead` in tw.h: zero-fills output buffer on failure and checks
  `endTransmission()` return value, preventing stale data being written
  to GBS8200 registers
- OTA progress calculation: divide-by-zero guard when contentLength == 0
- Cleanup: removed unused `serialCommandBuffer` and `userCommandBuffer`
  String declarations from Globals.h/.cpp

### Fixed
- Frozen WebUI caused by String buffer mutation across AsyncWebServer
  callbacks (PAL and NTSC affected; some PAL presets more than others)
- WebSocket disconnects on ESP8266 (webSocket.loop() not called)
- Corrupt WS status frame when presetSlot byte is 0xFF (EOF on short
  preferencesv2.txt) — slot now defaults to 'A' if invalid
- I2C error → stale register data being written to GBS8200
- OTA progress divide-by-zero on empty content

## [v1.2.1] – PAL 1024x768 Auto-Gain Default

### Added
- **PAL 1024x768 preset**: Auto-Gain is now enabled by default on first load.
  - Fixes ADC overdrive spikes on sharp black/white transitions
    (e.g. yellow-on-black text in sd2snes menu).
  - Hook sets `uopt->enableAutoGain = 1` and persists to LittleFS.
- New runtime flag `rto->autoGainHookEnabled` to distinguish hook-enabled
  Auto-Gain from user-enabled.

### Changed
- When switching away from PAL 1024x768 to any other preset, Auto-Gain is
  automatically disabled **only** if it was hook-enabled (marker = true).
  User-enabled Auto-Gain stays on across preset switches.
- Serial command `T` (auto gain toggle) now clears the hook marker,
  so user override is respected and survives subsequent preset loads.

### Fixed
- ADC overdrive spikes on B/W transitions in PAL 1024x768 mode.

## v1.2.0 – 1024x768 Output Resolution

### Added
- **1024x768 output resolution for NTSC and PAL**
  - New preset IDs 0x08 (NTSC) / 0x18 (PAL)
  - Available via OLED menu, web interface and serial command `G`
  - PalForce60 exclusion added for this preset — like Windowbox, it's a native
    dual-timing design and must not go through the NTSC-conversion path
  - NTSC: direct port; vertical centering (IF_VB_ST/SP) recalibrated on real
    hardware (SNES RGB, 60 Hz) — the inherited values left the picture
    offset. Also excluded this preset from the generic SNES-NTSC auto
    vertical-offset (`VPERIOD_IF == 523` → `IF_VB +4`) in `GBSController.cpp`,
    since that correction would otherwise double up on this preset's own
    calibrated values
  - PAL: active window (HB/DIS_HB) recalibrated on real hardware (SNES RGB,
    50 Hz)
    
## v1.1.2 – WiFi Reconnect Cleanup

### Fixed
- Removed redundant third WiFi reconnect timer (10s) from main loop;
  it could interfere with the ESP8266 SDK auto-reconnect and
  PersWiFiManager's own reconnect logic. The two existing mechanisms
  are sufficient.

## v1.1.1 – WebUI Windowbox Button Fix

### Fixed
- Web UI: the "1080p 4:3" button now persists the preset selection
  (survives resync and restart) and shows as active; previously it only
  switched once without saving. New user command 'W'; serial 'B' remains
  a non-persistent test command.

## v1.1.0 – 960p Windowbox Release

### Added
- **960p Windowbox ("1080p 4:3") preset for PAL and NTSC** – 240p/288p sources
  are integer-scaled (4x) to 960 lines in a 1080p frame
  - Automatic PAL/NTSC detection on format change and at startup
  - Available via OLED menu, web interface and serial command `B`
  - Custom preset slots can store the full windowbox state (Preset-ID 0x07)
  - PAL: 4:3 geometry via PLLAD_MD 2373 – **only change PLLAD_MD together
    with the PLLAD_VCORST reset sequence (1/20ms/0/100ms), otherwise the
    output turns green**
  - IF_VB fixed at 60/62, independent of the Full Height option
  - Tested with SNES 240p/288p (RGB); 480i/576i untested
  - Window geometry calibrated on a 32" display

### Fixed
- Web UI: deleting custom presets did not work in v1.0.1; deletion now works

## v1.0.1

- Completed the German translation of the web UI: 9 remaining English help texts
  (output resolution, ADC gain, full-height, low-resolution scaling, PAL 50->60 Hz,
  external clock generator, FrameTime-Lock method)
- No functional changes

## v1.0.0

First release of gbs-control-mixed.

- Base: [Kwakx/gbs-control-nx](https://github.com/Kwakx/gbs-control-nx) **v1.4.0** (94f8979)
- Fixes ported from [JuergenLeber/gbs-control](https://github.com/JuergenLeber/gbs-control) @ 7bfb9b3
  (common ancestor of both lines: ramapcsx2/gbs-control e4e317ab)
- German OLED menu and German web interface

All of Leber's fixes were ported as far as reasonably possible — the only exceptions are
listed under "Not ported" at the end.

### Video / sync stability

- csync RGB sources: SP_H_PULSE_IGNOR handling, true sync-loss detection via HSACT instead
  of IF mode bits, higher deinterlacer stability threshold for csync SD sources (50f3f43)
- CRTC timing change detection (e.g. Amstrad CPC): recovery machinery pauses until the line
  frequency stabilizes (87cd8b7)
- Screen blanks on source power-off instead of freezing the last, often corrupt, frame (bdbfcb7)
- Debouncing (3 samples) against pixel artifacts caused by a flickering sync-on-green
  signal (fb6342c)
- Black level no longer drifts to gray on csync SD sources — clamp release with stable
  sync in mode 0 (7389708)
- Sync watcher correctly classifies csync SD sources as stable, enabling clamp
  re-adjustment and auto-gain (b17442a)
- Display can enter standby when the source is powered off — sync output is stopped in
  addition to video (680f036)
- Atari ST 71 Hz high-res monochrome mode: new `probeDiscreteVSync()` helper with 9-sample
  median and fallback preset handling (46cc3db, 09471e9, cb4a0bd, d1517fb, 7bfb9b3)

### Presets / storage

- Preset slot save corruption fixed: shared slot buffer instead of stack allocation,
  protection against short/missing slot files, RGB/HV upscale presets saved under the
  correct file names (f04ff71)
- Slot metadata buffer (2.3 KB) allocated on demand instead of permanently occupying RAM
  (3c3915d)
- External clock generator (Si5351) reliably restored after a preset switch — retry every
  5 s instead of a single attempt (a68d124)
- Picture stays dark until the clamp position has been measured after loading a preset —
  retry loop up to 500 ms (92f484d)
- Clamp position flag correctly reset when a source disappears (3201025)
- RGB/HV upscale SD<->EDTV format change detection via band comparison instead of pairwise
  thresholds (partial, 9a415d0)

### WiFi / web interface

- ESP8266 WiFi crash fix: /wifi/connect no longer calls WiFi.begin() directly inside an
  AsyncWebServer callback — connection setup and persistence moved to the main loop (80d0d97)
- Custom runtime hostname from /hostname.txt, new endpoints /hostname/get and /hostname/set
  (270c6f0)
- New diagnostics command (clamp/gain/sync/SOG status) via serial and web — command letter
  Q (9b662bf, b863fa9)
- Serial log heap threshold replaced with a hysteresis (disable below 8000, re-enable above
  12000 bytes) to avoid flickering debug output

### Translations

- German OLED menu: live text rendering with the existing DejaVu Sans Mono fonts (umlauts
  and eszett supported natively)
- German web interface: webui sources translated and rebuilt, webui_html.h regenerated

### Not ported (from Leber's tree)

- ENABLE_WIFI compile-time flag for builds without WiFi/web/OTA (2fb1fde, fa71bf6) — would
  require invasive #ifdef changes across two platforms for an uncommon use case
- Leber's alternative /status/get implementation — the existing implementation with proper
  ESP8266/ESP32 handling was kept instead
