# Instalación — Español

Flashea **AxiomOS** en **M5Stack Cardputer ADV**.

## 1. Descargar

En **[Releases](https://github.com/Wiper25/AxiomOS-Cardputer/releases)** descarga:

- **`AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin`** (recomendado)

## 2. Conectar

USB-C → Cardputer ADV. Si no aparece el puerto, mantén **G0/Boot** al conectar.

## 3. Flashear

```bash
pip install esptool
esptool --chip esp32s3 --port PORT --baud 460800 write_flash 0x0 AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin
```

Herramientas web/GUI: chip **ESP32-S3**, dirección **`0x0`**, archivo `*-merged.bin`. Luego **Reset**.

## Controles

| Tecla | Acción |
|-------|--------|
| `;` / `.` | Arriba / abajo |
| Enter / BtnA | Seleccionar |
| `Del` / `` ` `` / `,` | Atrás |
| `R` | Rescan / limpiar |
| `1`–`4` | Acceso rápido |

Compatibilidad: [DEVICES.md](DEVICES.md) · Idiomas: [EN](INSTALL.en.md) · [RU](INSTALL.ru.md) · [ZH](INSTALL.zh.md) · [PT](INSTALL.pt.md) · [DE](INSTALL.de.md)
