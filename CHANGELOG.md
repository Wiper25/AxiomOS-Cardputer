# Changelog

All notable changes to **AxiomOS Cardputer Edition** are documented here.

Format based on [Keep a Changelog](https://keepachangelog.com/).
Versioning follows [SemVer](https://semver.org/).

## [Unreleased]

### Added
- **AxiomOS AI** layer (`src/modules/ai/`): Chat, Agents, Knowledge, Memory, History, Settings
- AI Dashboard + Device Doctor + action confirm gate
- Local knowledge base (ESP32 / nRF / WiFi / MQTT / …) with server fallback (REST / WebSocket streaming)
- FreeRTOS `axiom_ai` task; AI disabled via Settings leaves original firmware behavior

## [0.1.0] — 2026-08-06

First public release.

### Added
- LVGL dark UI with Russian font (`font_ru_14`) and status bar (Wi‑Fi / RF / BT / battery %)
- **Radio**
  - nRF24 spectrum scanner (async channel hop, live bars)
  - Packet monitor (RX hex dump)
  - nRF24 manager (channel / PA / data rate / TX test)
- **Network**
  - Wi‑Fi scanner + connect
  - MQTT client
  - WebSocket / HTTP / TCP clients
  - Ping / DNS
  - Network / IP info
- **Hardware**
  - GPIO monitor
  - Async I2C scanner (non-blocking)
  - IMU sensors + live accel/gyro graph
- **System**
  - Settings persistence
  - LittleFS browser + size stats (optional microSD)
  - About screen
- Ready-to-flash release binaries (`*-merged.bin` @ `0x0`)

### Notes
- Primary target: **M5Stack Cardputer ADV** (built-in nRF24 wiring)
- UI language in this build: **Russian**

[0.1.0]: https://github.com/Wiper25/AxiomOS-Cardputer/releases/tag/v0.1.0
