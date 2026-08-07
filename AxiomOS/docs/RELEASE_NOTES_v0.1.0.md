# AxiomOS v0.1.0

First public release for **M5Stack Cardputer ADV**.

## Download & flash

| Asset | Flash address |
|-------|----------------|
| **`AxiomOS-Cardputer-ADV-v0.1.0-merged.bin`** | **`0x0`** (recommended) |
| `AxiomOS-Cardputer-ADV-v0.1.0-app.bin` | `0x10000` |

```bash
esptool --chip esp32s3 --port PORT --baud 460800 write_flash 0x0 AxiomOS-Cardputer-ADV-v0.1.0-merged.bin
```

Install guides: [English](https://github.com/Wiper25/AxiomOS-Cardputer/blob/main/docs/INSTALL.en.md) · [Русский](https://github.com/Wiper25/AxiomOS-Cardputer/blob/main/docs/INSTALL.ru.md) · [中文](https://github.com/Wiper25/AxiomOS-Cardputer/blob/main/docs/INSTALL.zh.md)

## What's in

- LVGL UI + status bar (Wi‑Fi / RF / BT / battery)
- nRF24 spectrum scanner, packet monitor, manager
- Wi‑Fi, MQTT, WebSocket, HTTP, TCP, Ping/DNS
- GPIO, async I2C, IMU graphs
- LittleFS browser, settings, About

UI language: Russian.

Full notes: [CHANGELOG.md](https://github.com/Wiper25/AxiomOS-Cardputer/blob/main/CHANGELOG.md)
