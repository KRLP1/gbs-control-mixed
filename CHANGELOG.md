# Changelog

## [v1.4.9] – Frame-Sync Re-Arm Fix + Documentation Corrections

### Fixed
- Frame Time Lock: the success branch of the frame-sync block in loop()
  now re-arms `syncLockFailIgnore` to 16 on every successful
  `FrameSync::run*()`, including from state 0 (`!= 16` instead of
  `> 0`). Since v1.4.6 the failure-path decrement is guarded (no more
  0 -> 255 wrap), so the old `> 0` condition could never re-arm a
  counter that had reached 0 and the 16-failure glitch tolerance stayed
  gone until the next preset load or successful `runAutoBestHTotal()`.
  Behavior for counter values 1..16 and the failure path (decrement,
  single reset at 0) are unchanged.

### Changed
- No behavior change. Corrected the v1.4.8 notes on the `VDS_HB_ST` and
  `IF_HSYNC_RST` guards: the old expressions `(read() - 5) > 0` and
  `(read() - 4) > ...` could not underflow. Both registers are 12/11 bit,
  `read()` returns `uint16_t`, and integer promotion evaluates the
  subtraction as signed `int`, so a value below the constant gave a
  negative result and a false comparison. The v1.4.8 rewrites are
  behavior-identical; they only make the lower bound explicit.
- PersWiFiManager: rewrote the comment on `WiFi.disconnect(false)`. The
  argument only controls whether station mode is switched off; the call
  always clears the configured SSID/password. On ESP8266 whether that
  reaches flash depends on `WiFi.persistent()` (true only on the
  new-credentials path, where `WiFi.begin()` overwrites it right away;
  false on the empty-SSID path, so saved credentials survive).

## [v1.4.8] – Wrap-Guard Hardening + Dead-Code Removal

### Fixed
- Guard rewrite in VideoScaling.cpp: `(GBS::VDS_HB_ST::read() - 5) > 0`
  was changed to compare the operand before the subtraction:
  `read() > 5`. Note: the old form could not underflow. VDS_HB_ST is a
  12-bit register, read() returns uint16_t, and integer promotion makes
  the subtraction a signed int operation, so for read() < 5 the result
  was negative and the comparison false — the register write never
  happened. The rewrite is behavior-identical; it makes the guard
  explicit instead of relying on integer promotion rules.
- Same guard rewrite in gbs-control.ino command '6': the added
  `IF_HSYNC_RST::read() > 4` short-circuit before
  `(IF_HSYNC_RST::read() - 4) > ...` is likewise behavior-neutral
  (uint16_t read() promotes to signed int, so read() < 4 already made
  the old comparison false); it documents the guard explicitly.
- Per-target register width validation for serial command 'Y': the
  previous flat `value < 4096` check did not cover 11-bit targets (vt,
  vs*, vb*, if*, vsstc, vsspc) or the 5-bit sog register. Now uses
  per-target valueMax (2047 for 11-bit, 31 for sog, 4095 for 12-bit).
  The 'vt' target additionally uses valueMax = 2045 because
  set_vtotal() derives VDS_VSYN_SIZE1/2 = VDS_VSYNC_RST + 2 (both
  11-bit), so 2045 + 2 = 2047 is the exact safe maximum.

### Changed
- Scanlines.cpp: `MADPT_Y_MI_OFFSET::write(0xff)` changed to `0x7f`.
  The register is 7-bit, so the mask made 0xff into 0x7f anyway — the
  new literal matches the register width and is clearer.
- Serial commands Z/T (brightness), N/M (contrast), Q/H (Pb/U gain),
  P/S (Pr/V gain): added `< 255` / `> 0` bounds checks with "limit"
  feedback instead of silently wrapping at the 8-bit boundary.
- Serial command 'n' (pll_divider increment): added `>= 4095` check
  with "limit" feedback to prevent 12-bit overflow.
- Serial commands 'x'/'X' (IF_HBIN_SP ±1): added `< 4095` / `> 0`
  bounds checks with "limit" feedback.
- SyncWatcher.cpp: `VDS_VSCALE::write(read() - 57)` now guarded with
  `>= 57` check to prevent 10-bit underflow on pathological low values.
- osd.h menuMoveCursor(): changed from `(uint8_t)(menuIndex + delta) %
  OSD_ICON_COUNT` to int16_t arithmetic with negative-wrap handling.
  The old code would jump to icon 3 instead of the last icon when
  delta was negative at index 0 (255 % 8 = 3). Parked code, not
  user-reachable, but correct now if reactivated.

### Removed
- VideoShifting.cpp: shiftHorizontalLeftIF() and shiftHorizontalRightIF()
  removed (~130 lines). These were the only real 12→11-bit truncation
  sites in the codebase (PLLAD_MD written undiminished to IF_HSYNC_RST
  for progressive sources), but the functions were never called — the
  only call sites were commented out in gbs-control.ino. Marked as
  "unused but may become useful" since upstream ramapcsx2 approximately 2017.
- VideoShifting.h: corresponding declarations removed.
- gbs-control.ino: commented-out call sites for the removed functions
  cleaned up.

## [v1.4.7] – OSD Cleanup: Dead Code Removal, Default Initializers

### Changed
- OSDManager members now have default initializers (cursor = 0,
  state = OSDState::OFF, preset = 0) instead of relying on BSS
  zero-initialization of the single global instance. Protects against
  garbage reads if the class is ever instantiated on the stack or heap.
- Comment on the parked OSD handlers (osdScanlines, osdLineFilter)
  updated to document why they are kept but not registered.

### Removed
- osdAutoGain() OSD menu handler: dead code, never registered to any
  icon in any version. The WebUI Auto-Gain button uses a completely
  separate code path (command 'T') and is unaffected.
- Unused OSDManager members curVal, curMax, displayInColumn.

No functional changes.

## [v1.4.6] – Hardening: Async Safety, Crash Fixes, WiFi Retention

### Fixed
- OTA update could crash with division by zero when Content-Length was
  less than 100 bytes. Added a guard check before the modulo operation.
- OLED preset menu loaded the wrong preset slot for indices >= 26 (slots
  beyond 'Z' in the slot map). The old code used 'A' + item->tag which
  only works for the first 26 slots; now uses slotIndexMap[item->tag]
  with a bounds-check fallback to manager->unfreeze().
- ESP32 lost WiFi credentials when the router was offline for ~45
  seconds. PersWiFiManager::startApMode() called WiFi.disconnect(true,
  true) which erases NVS credentials. Changed to WiFi.disconnect(false)
  to preserve stored credentials.
- ESP8266 WebSocket heartbeat was ineffective in ASYNC mode. The
  library's enableHeartbeat() is compiled away in ASYNC builds, so two
  dead clients could block the WebUI indefinitely. Replaced with an
  app-level heartbeat that pings every 2s and disconnects zombies after
  6s without pong.
- /gbs/restore-filters performed I2C and LittleFS operations directly in
  the async web handler, racing with the main loop. Now queues a 'V'
  command and processes it in handleType2Command() in the loop() context.
- SerialMirror WebSocket broadcasts from async handlers could corrupt
  frames by racing with the loop()-context broadcast. Added an SPSC ring
  buffer (deferLog/processDeferred) with std::atomic_thread_fence memory
  barriers.
- /slot/save used positional URL parameters which are unreliable on ESP32
  (parameter ordering, cache-buster shifting). Changed to named
  parameters (index=, name=). Added empty-name validation and moved all
  parameter validation before any LittleFS access.
- WebUI did not URL-encode preset names in /slot/save requests. Names
  with special characters (&, =, spaces) could break URL parsing. Added
  encodeURIComponent() in the savePreset fetch call.
- Si5351 serial command "w f" without a number stored freqExtClockGen=0,
  causing a division by zero in externalClockGenSyncInOutRate() and a
  CPU exception. Added input validation (1-250 MHz range) and a
  library-level freq==0 guard in Si5351mcu::setFreq.
- syncLockFailIgnore (uint8_t) underflow: the postfix decrement
  syncLockFailIgnore-- wrapped 0 to 255, delaying the next
  FrameSync::reset by ~255 failures (~7 minutes). Added a guard
  consistent with the VideoTiming.cpp pattern.
- CommandQueue had no memory barrier, making it unsafe for ESP32
  dual-core access. Added std::atomic_thread_fence release/acquire
  fences with head/tail snapshots to prevent TOCTOU.
- OTA resolveRedirect could spin without yielding on stalled connections,
  risking a watchdog reset. Added delay(1) to feed the WDT and pump the
  WiFi/TCP stack.
- Web server started early in setup() before all state was initialized,
  so handlers could access uninitialized uopt/rto. Moved startWebserver()
  to the end of setup(), gated by rto->webServerEnabled.
- userCommand direct assignment from async handlers (/wifi/connect,
  /hostname/set) raced with loop()'s read. Changed to
  userCommandQueue.push() with deferLog warning on queue full.

### Removed
- /filesystem/format GET endpoint removed (destructive operation without
  authentication, CSRF-vulnerable).

## [v1.4.5] – OLED Preset Name Filter Widening

### Fixed
- Presets with names starting with non-alphanumeric characters (e.g.
  "(Test)", "-Mario", "_backup") were hidden from the OLED preset menu
  in v1.4.4 even though they existed in the WebUI. The isValidSlotName
  check now accepts any visible ASCII character (0x21-0x7E) as the first
  character, matching the WebUI's allowed character set documented in
  slot.h. Space and control characters at the start of a name are still
  rejected as before.

## [v1.4.4] – OLED Menu Crash Fix, Custom Preset Peaking, Validation Hardening

### Fixed
- OLED preset menu crash when more than 16 presets were saved. The device
  rebooted when opening the preset menu. Root causes: SlotMetaArray (2.3 KB)
  was allocated on the ESP8266 stack, and the menu registered more items
  than OLED_MENU_MAX_SUBITEMS_NUM allows. Now uses a static BSS buffer,
  counts valid slots before registration, and filters corrupted name fields.
- Peaking settings overwritten when loading a custom preset. The firmware
  now syncs wantPeaking and peakingLevel from the saved register values
  instead of overriding them with defaults.
- Serial peaking toggle (command 'f') did not write the gain registers
  when enabling peaking, so the change was not visible. Now writes
  VDS_PK_LB_GAIN, VDS_PK_LH_GAIN, and VDS_STEP_GAIN. Defaults peakingLevel
  to 1 if it was 0.
- Duplicate sourceGoneProbeCount assignment in SyncWatcher removed.
- WebServer input validation hardened: parameter count checks, nullptr
  guards, integer range validation before uint8_t cast, push() return
  value checks.
- SlotManager now falls back to the 240p preset when a preset file is
  truncated instead of returning a stale static buffer.

## [v1.4.3] – Peaking Visual Bar, Sync Fixes, WiFi Revert

### Added
- WebUI: visual fill bar on Peaking-Stufe button showing current
  level (0=empty, 1=33%, 2=66%, 3=100%), implemented via
  gbs-peaking-level HTML attribute and CSS ::after pseudo-element

### Changed
- WiFi output power reverted from 20.5 dBm back to 18.0 dBm.
  v1.3.0 raised the power to overcome GBS-C EMI, but the increase
  didn't measurably improve stability. Returns to kwakx's original
  value of 18.0 dBm.

### Fixed
- Peaking-Stufe button label was showing the saved level (e.g.
  "Normal") even when peaking was bypassed via the 'f' command
  (Kantenanhebung). Status-ping now sends 0 when wantPeaking is
  false, so the WebUI correctly shows "Aus" with empty fill bar
  when the chip is bypassed.
- peakingLevel was not explicitly set in loadDefaultUserOptions(),
  leaving it potentially uninitialized. Now set to 1 (Normal) to
  match wantPeaking = 1 and the old-prefs-file upgrade fallback.
- scanlineStrength bounds-check bug: the validation for
  scanlineStrength > 0x60 accidentally wrote the reset value to
  enableCalibrationADC instead of scanlineStrength (copy-paste
  error). With a corrupt preferences file, scanlineStrength
  stayed at the bad value while enableCalibrationADC was
  unintentionally set to 0x30.

## [v1.4.2] - Peaking Level Sync via Status-Ping

### Added
- WebSocket status-ping now includes peakingLevel (byte 6, bits 0-1)
  as a new field alongside the existing option bytes
- WebUI onmessage handler parses byte 6 and calls
  GBSControl.setPeakingLevel() to update the Peaking-Stufe button
  label directly from firmware-side state
- GBSControl.setPeakingLevel() exposed as internal setter function
  by the Peaking IIFE in initGBSButtons()

### Changed
- WebSocketHandler.cpp: MESSAGE_LEN increased from 6 to 7 to
  accommodate the new peakingLevel byte
- Peaking-Stufe button label now auto-syncs from status-ping
  within ~3s of page load (no manual click needed to refresh)

## [v1.4.1] – Hotfix: Transition Masking Fix, Peaking Button, Code Review Fixes

### Fixed
- Transition Masking: unfreezeVideo() added before early-return in
  Custom-Bypass preset paths (NTSC + PAL) — video no longer stays
  frozen when loading a custom bypass preset
- Peaking button: querySelector now uses [gbs-message-type="user"]
  to find the correct button (was finding a slot button instead)

### Changed
- Peaking level button now auto-enables/disables wantPeaking:
  level > 0 → wantPeaking = 1, level 0 → wantPeaking = 0
- WebUI: Peaking-Stufe button shows current level as text label
- WebUI: Help text added for Peaking-Stufe button
- tw.h: empty if-block replaced with explanatory comment

## [v1.4.0] – Peaking Levels, Transition Masking, PalForce60 Fix

### Added
- Peaking/Sharpness Level 0-3 (user command '8', WebUI button "Peaking-Stufe")
  - 0=off, 1=normal, 2=strong, 3=max
  - Persisted in preferencesv2.txt as field #21
  - Re-applied per preset load via doPostPresetLoadSteps()
  - WebUI button with "tune" icon on Filter page
- FIRMWARE_VERSION logged at boot (Serial output: "firmware: vX.Y.Z")
- Transition Masking: capture frozen during applyPresets() to prevent
  visual garbage during preset switches (upstream #332)
  - freezeVideo() at start of applyPresets()
  - unfreezeVideo() in doPostPresetLoadSteps(), bypassModeSwitch_RGBHV(),
    and HD-bypass early-return path

### Fixed
- PalForce60 no longer re-synced away by Si5351 external clock generator
  (upstream #237). Added !rto->presetIsPalForce60 to extClockGenDetected
  condition in gbs-control.ino loop()

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
