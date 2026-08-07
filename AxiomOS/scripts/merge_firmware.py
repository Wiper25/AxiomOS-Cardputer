#!/usr/bin/env python3
"""Merge bootloader + partitions + boot_app0 + app into one flashable .bin @ 0x0."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


OFFSETS = {
    "bootloader": 0x0000,
    "partitions": 0x8000,
    "boot_app0": 0xE000,
    "firmware": 0x10000,
}


def find_boot_app0() -> Path:
    home = Path.home() / ".platformio" / "packages"
    matches = list(home.glob("framework-arduinoespressif32*/tools/partitions/boot_app0.bin"))
    if not matches:
        raise FileNotFoundError("boot_app0.bin not found under ~/.platformio/packages")
    return matches[0]


def resolve_build_dir(root: Path) -> Path:
    env_build = os.environ.get("PLATFORMIO_BUILD_DIR")
    candidates = []
    if env_build:
        candidates.append(Path(env_build) / "m5stack-stamps3")
    candidates.extend(
        [
            Path("C:/pio_build/cardputer_adv/m5stack-stamps3"),
            root / ".pio" / "build" / "m5stack-stamps3",
        ]
    )
    for c in candidates:
        if c and (c / "firmware.bin").is_file():
            return c
    raise FileNotFoundError(
        "firmware.bin not found. Run: pio run -e m5stack-stamps3"
    )


def which_esptool() -> list[str]:
    if shutil.which("esptool"):
        return ["esptool"]
    if shutil.which("esptool.py"):
        return ["esptool.py"]
    return [sys.executable, "-m", "esptool"]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", default=os.environ.get("AXIOMOS_VERSION", "0.1.0"))
    parser.add_argument("--out-dir", type=Path, default=Path("dist"))
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    build = resolve_build_dir(root)
    boot_app0 = find_boot_app0()

    files = {
        "bootloader": build / "bootloader.bin",
        "partitions": build / "partitions.bin",
        "boot_app0": boot_app0,
        "firmware": build / "firmware.bin",
    }
    for name, path in files.items():
        if not path.is_file():
            raise FileNotFoundError(f"missing {name}: {path}")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    merged = args.out_dir / f"AxiomOS-Cardputer-ADV-v{args.version}-merged.bin"
    app_only = args.out_dir / f"AxiomOS-Cardputer-ADV-v{args.version}-app.bin"
    shutil.copy2(files["firmware"], app_only)

    cmd = which_esptool() + [
        "--chip",
        "esp32s3",
        "merge-bin",
        "-o",
        str(merged),
        "--flash-mode",
        "dio",
        "--flash-freq",
        "80m",
        "--flash-size",
        "8MB",
        hex(OFFSETS["bootloader"]),
        str(files["bootloader"]),
        hex(OFFSETS["partitions"]),
        str(files["partitions"]),
        hex(OFFSETS["boot_app0"]),
        str(files["boot_app0"]),
        hex(OFFSETS["firmware"]),
        str(files["firmware"]),
    ]
    print("+", " ".join(cmd))
    subprocess.check_call(cmd)

    # Convenience copies without version for scripts
    shutil.copy2(merged, args.out_dir / "AxiomOS-Cardputer-ADV-merged.bin")
    shutil.copy2(app_only, args.out_dir / "AxiomOS-Cardputer-ADV-app.bin")

    print(f"OK  merged : {merged} ({merged.stat().st_size} bytes)")
    print(f"OK  app    : {app_only} ({app_only.stat().st_size} bytes)")
    print("Flash merged @ 0x0  |  app-only @ 0x10000")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # noqa: BLE001
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
