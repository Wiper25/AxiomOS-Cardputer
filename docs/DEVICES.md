# Supported devices

| Device | MCU | Flash | Support | Notes |
|--------|-----|-------|---------|-------|
| **M5Stack Cardputer ADV** | ESP32-S3 | 8 MB | **Full** | Official target. Built-in nRF24, IMU, keyboard, display. |
| M5Stack Cardputer (classic) | ESP32-S3 | 8 MB | Experimental | Boots UI/Wi‑Fi/I2C likely OK. **nRF24 & SD pins differ** — radio/SD may not work without hardware/config changes. |
| Other StampS3 / Cardputer clones | ESP32-S3 | 8 MB | Unsupported | Not tested. Fork and adjust `src/core/config.h` pins. |

## Hardware used by AxiomOS (ADV)

| Peripheral | Interface | Pins (ADV) |
|------------|-----------|------------|
| Display + keyboard | M5Cardputer / M5Unified | board defaults |
| nRF24L01 | SPI | CE=4, CSN=12, SCK=40, MOSI=14, MISO=39 |
| microSD (optional) | SPI (shared with EXT) | CS=12 (same as nRF CSN — don't use both at once carelessly) |
| Grove I2C | Wire | SDA=2, SCL=1 |
| IMU (BMI270 etc.) | Internal I2C | via M5Unified |

## Which binary to download

From [Releases](https://github.com/Wiper25/AxiomOS-Cardputer/releases):

| File | Flash address | Use when |
|------|---------------|----------|
| `AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin` | **0x0** | First install / full reflash (recommended) |
| `AxiomOS-Cardputer-ADV-vX.Y.Z-app.bin` | **0x10000** | OTA-style app update (keep bootloader/partitions) |

## Not supported (yet)

- Cardputer with external nRF on different CE/CSN (needs pin remap)
- Non–ESP32-S3 M5 devices
- Browser UI / desktop builds
