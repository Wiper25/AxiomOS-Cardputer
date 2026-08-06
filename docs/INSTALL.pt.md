# Instalação — Português

Grave o **AxiomOS** no **M5Stack Cardputer ADV**.

## 1. Baixar

Em **[Releases](https://github.com/Wiper25/AxiomOS-Cardputer/releases)** baixe:

- **`AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin`** (recomendado)

## 2. Conectar

USB-C no Cardputer ADV. Se a porta não aparecer, segure **G0/Boot** ao conectar.

## 3. Gravacao

```bash
pip install esptool
esptool --chip esp32s3 --port PORT --baud 460800 write_flash 0x0 AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin
```

Flasher web/GUI: chip **ESP32-S3**, endereço **`0x0`**, arquivo `*-merged.bin`. Depois **Reset**.

## Controles

| Tecla | Ação |
|-------|------|
| `;` / `.` | Cima / baixo |
| Enter / BtnA | Selecionar |
| `Del` / `` ` `` / `,` | Voltar |
| `R` | Rescan / limpar |
| `1`–`4` | Atalhos |

Compatibilidade: [DEVICES.md](DEVICES.md) · Idiomas: [EN](INSTALL.en.md) · [RU](INSTALL.ru.md) · [ZH](INSTALL.zh.md) · [ES](INSTALL.es.md) · [DE](INSTALL.de.md)
