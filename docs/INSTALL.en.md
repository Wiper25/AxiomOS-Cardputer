# Install — English

Flash **AxiomOS** onto **M5Stack Cardputer ADV** in a few minutes.

## 1. Download firmware

Open **[Releases](https://github.com/Wiper25/AxiomOS-Cardputer/releases)** and download:

- **`AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin`** ← recommended (full image)

Optional: `*-app.bin` only if you know you need an app-slot update at `0x10000`.

## 2. Put the device in download mode

1. Connect Cardputer ADV with **USB-C**
2. Hold **G0 / Boot** if the port does not appear, then plug USB (or use M5Burner’s reset sequence)
3. Note the serial port: Windows `COMx`, macOS `/dev/cu.usbmodem*`, Linux `/dev/ttyACM0`

## 3A. esptool (CLI)

```bash
pip install esptool

esptool --chip esp32s3 --port PORT --baud 460800 write_flash 0x0 AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin
```

Replace `PORT` with your port. After flash, press **Reset**.

## 3B. Web flasher

1. Chrome / Edge → [ESPConnect](https://thelastoutpostworkshop.github.io/ESPConnect/) or any ESP32-S3 web flasher
2. Connect → select `*-merged.bin` → flash address **`0x0`**
3. Wait until done → Reset

## 3C. Desktop ESP Flasher / M5Burner

1. Chip: **ESP32-S3**
2. File: `*-merged.bin`
3. Address: **`0x0`**
4. Flash → Reset

## Controls (Cardputer)

| Key | Action |
|-----|--------|
| `;` / `.` | Up / Down |
| Enter / BtnA | Select |
| `Del` / `` ` `` / `,` | Back |
| `R` | Rescan / clear (context) |
| `1`–`4` | Quick jump sections |

## Build from source

```bash
git clone https://github.com/Wiper25/AxiomOS-Cardputer.git
cd AxiomOS-Cardputer
pio run -e m5stack-stamps3
python scripts/merge_firmware.py --version 0.1.0
pio run -t upload   # or flash dist/*-merged.bin @ 0x0
```

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Port missing | Hold G0, reconnect USB, install CP210x/CH340/CDC drivers |
| Boot loop / blank | Reflash **merged** @ `0x0`, erase flash once: `esptool erase_flash` |
| `nRF24 не найден` | ADV with module seated; check EXT/SPI; SD and nRF share CS=12 |
| Wrong device | See [DEVICES.md](DEVICES.md) |

More languages: [Русский](INSTALL.ru.md) · [中文](INSTALL.zh.md) · [Español](INSTALL.es.md) · [Português](INSTALL.pt.md) · [Deutsch](INSTALL.de.md)
