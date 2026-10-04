# Changelog

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
