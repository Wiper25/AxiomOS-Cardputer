# 安装说明 — 中文

将 **AxiomOS** 刷入 **M5Stack Cardputer ADV**。

## 1. 下载固件

打开 **[Releases](https://github.com/Wiper25/AxiomOS-Cardputer/releases)**，下载：

- **`AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin`**（推荐，完整镜像）

## 2. 连接设备

1. 使用 USB-C 连接 Cardputer ADV  
2. 若无串口，按住 **G0/Boot** 再插线  
3. 端口示例：Windows `COMx`，macOS `/dev/cu.usbmodem*`，Linux `/dev/ttyACM0`

## 3. 烧录

```bash
pip install esptool
esptool --chip esp32s3 --port PORT --baud 460800 write_flash 0x0 AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin
```

也可用网页/桌面烧录工具：芯片 **ESP32-S3**，地址 **`0x0`**，选择 `*-merged.bin`。完成后按 **Reset**。

## 按键

| 按键 | 功能 |
|------|------|
| `;` / `.` | 上 / 下 |
| Enter / BtnA | 确认 |
| `Del` / `` ` `` / `,` | 返回 |
| `R` | 重新扫描 / 清空 |
| `1`–`4` | 快捷分区 |

## 从源码编译

```bash
git clone https://github.com/Wiper25/AxiomOS-Cardputer.git
cd AxiomOS-Cardputer
pio run -e m5stack-stamps3
python scripts/merge_firmware.py --version 0.1.0
```

设备兼容性见 [DEVICES.md](DEVICES.md)。

其他语言：[English](INSTALL.en.md) · [Русский](INSTALL.ru.md) · [Español](INSTALL.es.md) · [Português](INSTALL.pt.md) · [Deutsch](INSTALL.de.md)
