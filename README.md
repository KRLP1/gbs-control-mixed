# gbs-control-mixed

## What is this?

`gbs-control-mixed` is alternative firmware for the **GBS-C** video
upscaler (TV5725 chip), found on GBS-8200 / GBS-8220 boards. It
turns the bare scaler into a feature-rich upscaler with a web
interface, OLED menu, automatic mode detection and OTA firmware
updates — controlled from an ESP8266 or ESP32 that replaces the
original control MCU.

**Key features:**
- Bilingual German / English OLED menu and WebUI
- OTA firmware updates via WebUI or OLED menu (no serial terminal
  needed after the initial flash)
- Automatic mode detection with plausibility watchdog
  (recovers from stuck-MD after format switches)
- PAL and NTSC support
- Output resolutions: 480i, 576i, 720p, 960p, 960p windowbox,
  1024×768, 1280×1024, 1920×1080
- PlatformIO build (no Arduino IDE needed)

## About This Fork

This fork is based on gbs-control-nx v1.4.0 with additional fixes and features for
the GBS-C (TV5725) upscaler (ESP8266/ESP32).

- **Base:** [gbs-control-nx](https://github.com/Kwakx/gbs-control-nx) v1.4.0 — the modular
  ESP8266/ESP32 codebase with web interface, OTA updates and OLED menu
- **Fixes:** For details, see [CHANGELOG.md](CHANGELOG.md)
- **UI:** Bilingual German / English OLED menu and web interface

**Latest releases available at:** https://github.com/KRLP1/gbs-control-mixed/releases

**Supported Platforms:**
- **ESP8266**
- **ESP32**: Only classic ESP32 and its variants (ESP32-WROOM-32, ESP32-DevKitC, and
  compatible boards)

**⚠️ Not Supported ESP32 Variants:**
- ESP32-S3-DevKitC
- ESP32-H2/H4/C2/C3/C5/C61/C6/S2/P4

For pin connections and wiring information, see **[PINOUT.md](PINOUT.md)**.

## Tested Hardware

Tested with the following source consoles:
- Nintendo NES
- Super Nintendo (SNES)
- Nintendo 64
- PlayStation 1
- Sega Saturn
- PC-Engine


## ⚠️ Backup & AI Notice

**Make a backup before flashing!** Presets and settings are stored on the device — a full
flash erase will delete them.

The porting work in this fork was done with AI assistance.
The firmware compiles and has been tested on real hardware  but it is provided
**as-is, without any warranty — use at your own risk.**

## Building and Installation

**Important Notes:**
- **Do not use the original Arduino IDE installation instructions** — this fork is adapted
  exclusively for PlatformIO
- **PlatformIO automatically downloads the required libraries** — no manual library
  installation needed

This project uses PlatformIO for building and flashing. Follow these steps to set up your
development environment:

1. **Install Visual Studio Code**
   - Download and install from: https://code.visualstudio.com/

2. **Install PlatformIO Extension**
   - Open VS Code
   - Go to Extensions (Ctrl+Shift+X)
   - Search for "PlatformIO IDE" and install it
   - Restart VS Code after installation

3. **Build and Upload**
   - Open the project folder in VS Code
   - Open the PlatformIO PROJECT TASKS sidebar (PlatformIO icon in the left sidebar)
   - Navigate to your project environment:
     - **ESP8266**: Select `gbsc` environment
     - **ESP32**: Select `gbsc_esp32` environment
   - Follow these steps in order:
     1. **General** → **Build** - Compile the firmware
     2. **Platform** → **Erase Flash** - Erase the flash (recommended for clean install)
     3. **General** → **Upload** - Flash the firmware to your board

Alternatively, download a prebuilt binary from the
[releases](https://github.com/KRLP1/gbs-control-mixed/releases): `firmware.bin` for ESP8266
boards, `firmware_esp32.bin` for ESP32 boards. Flash the file matching your board with
your preferred tool.

For detailed PlatformIO documentation, visit:
https://docs.platformio.org/en/latest/integration/ide/vscode.html#installation

## Firmware Updates (OTA)

Once the firmware is running, future updates can be installed
directly from the WebUI or OLED menu — no computer or serial
connection needed (except for the very first flash).

1. Connect the device to WiFi (initial setup via serial or AP mode)
2. Open the WebUI in a browser, or use the OLED menu
3. WebUI: System → Online-Update; OLED: navigate to the online
   update entry
4. Check for a newer release
5. If an update is available, start it
6. On ESP8266 the device reboots and flashes in boot mode (full
   heap, resume on power loss); on ESP32 the download runs directly
   in the running firmware.
7. The WebUI page auto-reloads after the reboot.

The device always verifies the SHA256 checksum before applying the
new firmware.

**Note:** the v1.7.1 release has a broken OTA implementation on
ESP8266 (OOM during download). Updating from v1.7.1 to v1.7.2
requires flashing via PC (PlatformIO). After v1.7.2 is installed,
future updates work via WebUI or OLED again.

## Credits & Thanks

- 🙏 **Kwakx** — [gbs-control-nx](https://github.com/Kwakx/gbs-control-nx): the modular
  ESP8266/ESP32 rewrite this fork is built on. Without this work, none of this would exist.
- 🙏 **JuergenLeber** — [gbs-control](https://github.com/JuergenLeber/gbs-control): the
  sync, clamp and preset fixes ported into this fork — painstakingly developed and tested
  on real hardware.
- 🙏 **ramapcsx2** — the original [gbs-control](https://github.com/ramapcsx2/gbs-control)
  project that started it all.

Thank you all for your work on open-source retro gaming hardware!

A detailed list of all ported changes: [CHANGELOG.md](CHANGELOG.md)

---

Original gbs-control documentation: https://ramapcsx2.github.io/gbs-control/,
original repo: https://github.com/ramapcsx2/gbs-control