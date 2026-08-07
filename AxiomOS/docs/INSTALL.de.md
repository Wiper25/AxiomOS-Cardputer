# Installation — Deutsch

**AxiomOS** auf dem **M5Stack Cardputer ADV** flashen.

## 1. Download

Unter **[Releases](https://github.com/Wiper25/AxiomOS-Cardputer/releases)**:

- **`AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin`** (empfohlen)

## 2. Anschließen

USB-C → Cardputer ADV. Kein Port? **G0/Boot** gedrückt halten und neu einstecken.

## 3. Flashen

```bash
pip install esptool
esptool --chip esp32s3 --port PORT --baud 460800 write_flash 0x0 AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin
```

Web/GUI-Flasher: Chip **ESP32-S3**, Adresse **`0x0`**, Datei `*-merged.bin`. Danach **Reset**.

## Steuerung

| Taste | Aktion |
|-------|--------|
| `;` / `.` | Hoch / runter |
| Enter / BtnA | Auswählen |
| `Del` / `` ` `` / `,` | Zurück |
| `R` | Rescan / löschen |
| `1`–`4` | Schnellmenü |

Geräte: [DEVICES.md](DEVICES.md) · Sprachen: [EN](INSTALL.en.md) · [RU](INSTALL.ru.md) · [ZH](INSTALL.zh.md) · [ES](INSTALL.es.md) · [PT](INSTALL.pt.md)
