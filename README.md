# gbs-control-mixed

## About This Fork

This fork combines [Kwakx/gbs-control-nx](https://github.com/Kwakx/gbs-control-nx) v1.4.0
with fixes from [JuergenLeber/gbs-control](https://github.com/JuergenLeber/gbs-control),
ported **as far as possible**, plus a German user interface.

- **Base:** [gbs-control-nx](https://github.com/Kwakx/gbs-control-nx) v1.4.0 — the modular
  ESP8266/ESP32 codebase with web interface, OTA updates and OLED menu
- **Fixes:** sync/clamp stability, preset slot handling, Atari ST high-res monochrome
  support and more from [JuergenLeber/gbs-control](https://github.com/JuergenLeber/gbs-control)
  (see [CHANGELOG.md](CHANGELOG.md))
- **UI:** German OLED menu and German web interface

Note: the user interface (OLED menu and web interface) is in German.

**Latest releases available at:** https://github.com/KRLP1/gbs-control-mixed/releases

**Supported Platforms:**
- **ESP8266**
- **ESP32**: Only classic ESP32 and its variants (ESP32-WROOM-32, ESP32-DevKitC, and
  compatible boards)

**⚠️ Not Supported ESP32 Variants:**
- ESP32-S3-DevKitC
- ESP32-H2/H4/C2/C3/C5/C61/C6/S2/P4

For pin connections and wiring information, see **[PINOUT.md](PINOUT.md)**.

## ⚠️ Backup & AI Notice

**Make a backup before flashing!** Presets and settings are stored on the device — a full
flash erase will delete them.

The porting work in this fork was done with AI assistance. The firmware compiles and has
been tested on real hardware, but it is provided **as-is, without any warranty — use at
your own risk.**

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
your preferred tool. Online updates from the device check this repository.

For detailed PlatformIO documentation, visit:
https://docs.platformio.org/en/latest/integration/ide/vscode.html#installation

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
