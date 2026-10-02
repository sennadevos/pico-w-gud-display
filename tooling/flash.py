#!/usr/bin/env python3
"""Copy the validated firmware only to a mounted RP2040 BOOTSEL drive."""

import argparse
import json
import os
from pathlib import Path
import subprocess

from check_uf2 import validate_uf2


def mounted_boot_drives() -> list[Path]:
    result = subprocess.run(
        ["findmnt", "--json", "--output", "TARGET,LABEL"],
        check=True, capture_output=True, text=True,
    )
    drives = []

    def visit(entries):
        for entry in entries:
            if entry.get("label") == "RPI-RP2":
                drives.append(Path(entry["target"]))
            visit(entry.get("children", []))

    visit(json.loads(result.stdout).get("filesystems", []))
    return drives


def mount_boot_drive() -> Path:
    """Mount the RPI-RP2 volume via udisks; the desktop may not automount it."""
    device = Path("/dev/disk/by-label/RPI-RP2")
    if not device.exists():
        raise ValueError("No RPI-RP2 volume found; hold BOOTSEL while connecting USB")
    result = subprocess.run(
        ["udisksctl", "mount", "-b", str(device.resolve()), "--no-user-interaction"],
        capture_output=True, text=True,
    )
    mounted = mounted_boot_drives()
    if mounted:
        return mounted[0]
    raise ValueError(f"Could not mount {device}: {result.stderr.strip() or result.stdout.strip()}")


def validate_boot_drive(path: Path) -> None:
    if not path.is_dir() or not os.path.ismount(path):
        raise ValueError(f"Not a mounted BOOTSEL drive: {path}")
    info = (path / "INFO_UF2.TXT").read_text()
    if "Board-ID: RPI-RP2" not in info:
        raise ValueError("BOOTSEL board ID is not RPI-RP2; refusing to flash")


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uf2", type=Path, default=root / "dist/pico_w_screen.uf2")
    parser.add_argument("--mount", type=Path, help="Explicit mounted RPI-RP2 path")
    args = parser.parse_args()
    try:
        validate_uf2(args.uf2)
        if args.mount:
            mount = args.mount
        else:
            drives = mounted_boot_drives()
            mount = drives[0] if drives else mount_boot_drive()
        validate_boot_drive(mount)
        destination = mount / "pico_w_screen.uf2"
        data = args.uf2.read_bytes()
        print(f"Flashing {args.uf2} to {destination}", flush=True)
        with destination.open("wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Flash failed: {error}\n")
    print("UF2 copied. The Pico should reboot as USB 16d0:10a9 (GUD display).")


if __name__ == "__main__":
    main()
