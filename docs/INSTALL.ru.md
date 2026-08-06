# Установка — Русский

Прошивка **AxiomOS** на **M5Stack Cardputer ADV**.

## 1. Скачать

Раздел **[Releases](https://github.com/Wiper25/AxiomOS-Cardputer/releases)**:

- **`AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin`** — полный образ (рекомендуется)

## 2. Подключение

1. USB-C к Cardputer ADV  
2. Если порт не появился — зажми **G0/Boot** и переподключи USB  
3. Порт: Windows `COMx`, macOS `/dev/cu.usbmodem*`, Linux `/dev/ttyACM0`

## 3. Прошивка

### esptool

```bash
pip install esptool
esptool --chip esp32s3 --port PORT --baud 460800 write_flash 0x0 AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin
```

### Веб / GUI

Любой ESP32-S3 flasher (ESPConnect, ESP Flasher, M5Burner): файл `*-merged.bin`, адрес **`0x0`**.

После прошивки — **Reset**.

## Управление

| Клавиша | Действие |
|---------|----------|
| `;` / `.` | Вверх / вниз |
| Enter / BtnA | Выбор |
| `Del` / `` ` `` / `,` | Назад |
| `R` | Rescan / очистка |
| `1`–`4` | Быстрый переход по разделам |

## Сборка из исходников

```bash
git clone https://github.com/Wiper25/AxiomOS-Cardputer.git
cd AxiomOS-Cardputer
pio run -e m5stack-stamps3
python scripts/merge_firmware.py --version 0.1.0
```

## Проблемы

| Симптом | Что делать |
|---------|------------|
| Нет порта | G0 + драйверы USB-UART |
| Чёрный экран / bootloop | Перепрошить merged @ `0x0`, при необходимости `erase_flash` |
| nRF не найден | Модуль ADV / общая CS с SD |
| Другое железо | [DEVICES.md](DEVICES.md) |

Языки: [English](INSTALL.en.md) · [中文](INSTALL.zh.md) · [Español](INSTALL.es.md) · [Português](INSTALL.pt.md) · [Deutsch](INSTALL.de.md)
