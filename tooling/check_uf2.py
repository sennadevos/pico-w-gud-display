#!/usr/bin/env python3
"""Validate UF2 sectors, target family and flash range before flashing a Pico W."""

import argparse
from pathlib import Path
import struct

MAGIC_START = (0x0A324655, 0x9E5D5157)
MAGIC_END = 0x0AB16F30
FAMILY_RP2040 = 0xE48BFF56
FLASH_START = 0x10000000
FLASH_END = FLASH_START + 2 * 1024 * 1024


def validate_uf2(path: Path) -> dict:
    data = path.read_bytes()
    if not data or len(data) % 512:
        raise ValueError("UF2 must contain complete 512-byte sectors")
    count = len(data) // 512
    numbers = set()
    spans = []
    for offset in range(0, len(data), 512):
        fields = struct.unpack_from("<8I", data, offset)
        magic0, magic1, flags, address, size, number, total, family = fields
        if (magic0, magic1) != MAGIC_START or struct.unpack_from("<I", data, offset + 508)[0] != MAGIC_END:
            raise ValueError("Invalid UF2 magic")
        if flags != 0x2000 or family != FAMILY_RP2040:
            raise ValueError("Not a normal RP2040 flash UF2 (Pico W required)")
        if not 0 < size <= 476 or address % 256:
            raise ValueError("Invalid UF2 payload or target alignment")
        if not FLASH_START <= address < address + size <= FLASH_END:
            raise ValueError("UF2 writes outside the Pico W's 2 MiB flash")
        if total != count or number >= count or number in numbers:
            raise ValueError("Missing, duplicate or inconsistent UF2 block numbers")
        numbers.add(number)
        spans.append((address, address + size))
    spans.sort()
    if spans[0][0] != FLASH_START:
        raise ValueError("UF2 does not begin with the RP2040 boot stage")
    if any(left[1] > right[0] for left, right in zip(spans, spans[1:])):
        raise ValueError("Overlapping UF2 payloads")
    return {"blocks": count, "file_bytes": len(data), "flash_bytes": sum(end - start for start, end in spans)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("uf2", type=Path)
    args = parser.parse_args()
    try:
        info = validate_uf2(args.uf2)
    except (OSError, ValueError) as error:
        parser.exit(1, f"UF2 validation failed: {error}\n")
    print(f"Valid Pico W / RP2040 UF2: {info['blocks']} blocks, {info['flash_bytes']} flash bytes")


if __name__ == "__main__":
    main()
