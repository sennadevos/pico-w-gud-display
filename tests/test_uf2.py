from pathlib import Path
import struct
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tooling"))
from check_uf2 import validate_uf2


def block(address=0x10000000, size=256, number=0, total=1, family=0xE48BFF56, flags=0x2000):
    return struct.pack("<8I", 0x0A324655, 0x9E5D5157, flags, address,
                       size, number, total, family) + bytes(476) + struct.pack("<I", 0x0AB16F30)


def test_valid_uf2(tmp_path):
    path = tmp_path / "valid.uf2"
    path.write_bytes(block())
    assert validate_uf2(path) == {"blocks": 1, "file_bytes": 512, "flash_bytes": 256}


@pytest.mark.parametrize("data", [
    b"", b"bad", block()[:-1],
    block(family=0xE48BFF59), block(flags=1), block(size=477), block(size=0),
    block(address=0x20000000), block(address=0x10000001), block(total=2),
    block(number=1), block(total=2) + block(total=2),
    block(total=2) + block(address=0x10000000, number=1, total=2),
])
def test_refuse_unsafe_uf2(tmp_path, data):
    path = tmp_path / "invalid.uf2"
    path.write_bytes(data)
    with pytest.raises(ValueError):
        validate_uf2(path)


def test_actual_firmware_if_built():
    path = Path(__file__).resolve().parents[1] / "dist/pico_w_screen.uf2"
    if not path.exists():
        pytest.skip("Build firmware to validate the real UF2")
    assert validate_uf2(path)["flash_bytes"] < 2 * 1024 * 1024
