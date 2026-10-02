import ctypes
from pathlib import Path
import random
import struct

import lz4.block
import pytest

ROOT = Path(__file__).resolve().parents[1]
PROTOCOL_ERROR = -3
INVALID_PARAMETER = -4
GET_DESCRIPTOR = 0x01
GET_EDID = 0x56
SET_BUFFER = 0x60
STATE_CHECK = 0x61
STATE_COMMIT = 0x62


@pytest.fixture()
def library():
    dll = ctypes.CDLL(str(ROOT / "build/tests/libprotocol.so"))
    for function in (dll.protocol_get, dll.protocol_set):
        function.argtypes = [ctypes.c_uint8, ctypes.c_uint16, ctypes.c_void_p, ctypes.c_size_t]
        function.restype = ctypes.c_int
    dll.LZ4_decompress_safe.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
    dll.LZ4_decompress_safe.restype = ctypes.c_int
    dll.protocol_reset()
    return dll


def get(dll, request, size=128, index=0):
    buffer = ctypes.create_string_buffer(size)
    length = dll.protocol_get(request, index, buffer, size)
    return length, buffer.raw[:max(length, 0)]


def set_request(dll, request, payload=b"", index=0):
    buffer = ctypes.create_string_buffer(payload)
    return dll.protocol_set(request, index, buffer, len(payload))


def state(width=240, height=320, format=0x80, connector=0, properties=((12, 100),)):
    mode = struct.pack("<I8HI", 4608, width, width, width, width,
                       height, height, height, height, 1 << 10)
    return mode + bytes([format, connector]) + b"".join(struct.pack("<HQ", *p) for p in properties)


def activate(dll):
    assert set_request(dll, STATE_CHECK, state()) == 0
    assert set_request(dll, STATE_COMMIT) == 0


def buffer_request(x=0, y=0, width=240, height=1, length=960,
                   compression=0, compressed_length=0):
    return struct.pack("<5IBI", x, y, width, height, length, compression, compressed_length)


def test_descriptor_wire_layout(library):
    length, data = get(library, GET_DESCRIPTOR)
    assert length == 30
    assert struct.unpack("<IBIB5I", data) == (0x1D50614D, 1, 1, 1, 65536, 240, 240, 320, 320)


def test_empty_properties_and_backlight(library):
    assert get(library, 0x41) == (0, b"")
    assert get(library, 0x51) == (10, struct.pack("<HQ", 12, 100))
    assert get(library, 0x40) == (1, b"\x80")
    assert get(library, 0x50) == (5, struct.pack("<BI", 0, 0))
    assert get(library, 0x54) == (1, b"\x01")


def test_edid_checksum_and_name(library):
    length, data = get(library, GET_EDID)
    assert length == 128
    assert data[:8] == b"\x00\xff\xff\xff\xff\xff\xff\x00"
    assert sum(data) % 256 == 0
    assert b"PicoW-TFT" in data


def test_malformed_state_lengths(library):
    payload = state()
    for size in range(26):
        assert set_request(library, STATE_CHECK, payload[:size]) == PROTOCOL_ERROR
    assert set_request(library, STATE_CHECK, payload + b"x") == PROTOCOL_ERROR
    assert set_request(library, STATE_CHECK, state(properties=((12, 100),) * 9)) == PROTOCOL_ERROR


@pytest.mark.parametrize("payload", [
    state(width=320, height=240), state(format=0x40), state(connector=1),
    state(properties=((12, 101),)), state(properties=((99, 0),)),
])
def test_invalid_state(library, payload):
    assert set_request(library, STATE_CHECK, payload) == INVALID_PARAMETER


def test_state_commit_and_reset(library):
    assert set_request(library, STATE_COMMIT) == INVALID_PARAMETER
    assert set_request(library, STATE_CHECK, state()) == 0
    assert set_request(library, SET_BUFFER, buffer_request()) == INVALID_PARAMETER
    assert set_request(library, STATE_COMMIT, b"x") == PROTOCOL_ERROR
    assert set_request(library, STATE_COMMIT) == 0
    assert set_request(library, SET_BUFFER, buffer_request()) == 0
    library.protocol_reset()
    assert set_request(library, SET_BUFFER, buffer_request()) == INVALID_PARAMETER


def test_valid_rectangles(library):
    activate(library)
    assert set_request(library, SET_BUFFER, buffer_request()) == 0
    assert set_request(library, SET_BUFFER, buffer_request(x=239, y=319, width=1, height=1, length=4)) == 0
    assert set_request(library, SET_BUFFER, buffer_request(width=240, height=68, length=65280)) == 0
    assert set_request(library, SET_BUFFER, buffer_request(compression=1, compressed_length=20)) == 0
    # Linux caps LZ4 output at the raw length; exactly-equal output is legal.
    assert set_request(library, SET_BUFFER, buffer_request(compression=1, compressed_length=960)) == 0


@pytest.mark.parametrize("params", [
    {"width": 0}, {"height": 0}, {"x": 240}, {"y": 320}, {"x": 1},
    {"x": 1, "width": 0xFFFFFFFF}, {"y": 1, "height": 0xFFFFFFFF},
    {"height": 69, "length": 66240}, {"length": 961},
    {"compression": 2, "compressed_length": 20},
    {"compression": 1, "compressed_length": 0},
    {"compression": 1, "compressed_length": 65537},
    {"compression": 1, "compressed_length": 961},
    {"compressed_length": 20},
])
def test_invalid_rectangles(library, params):
    activate(library)
    assert set_request(library, SET_BUFFER, buffer_request(**params)) == INVALID_PARAMETER


def test_boolean_payloads(library):
    for request in (0x63, 0x64):
        assert set_request(library, request, b"\x00") == 0
        assert set_request(library, request, b"\x01") == 0
        assert set_request(library, request, b"\x02") == INVALID_PARAMETER
        assert set_request(library, request) == PROTOCOL_ERROR
        assert set_request(library, request, b"\x00\x00") == PROTOCOL_ERROR


def test_unsupported_and_connector_index(library):
    assert get(library, 0xFE)[0] == -2
    assert set_request(library, 0xFE) == -2
    assert get(library, 0x54, index=1)[0] == PROTOCOL_ERROR
    assert set_request(library, 0x53, index=1) == PROTOCOL_ERROR
    assert set_request(library, 0x53, b"x") == PROTOCOL_ERROR


@pytest.mark.parametrize("payload", [
    b"\x00" * 32768,
    b"\x00\xf8\xe0\x07\x1f\x00" * 5000,
    random.Random(42).randbytes(32000),
])
def test_lz4_linux_compatible_blocks(library, payload):
    compressed = lz4.block.compress(payload, store_size=False)
    source = ctypes.create_string_buffer(compressed)
    output = ctypes.create_string_buffer(len(payload))
    size = library.LZ4_decompress_safe(source, output, len(compressed), len(payload))
    assert size == len(payload)
    assert output.raw == payload


def test_lz4_truncated_payload(library):
    compressed = lz4.block.compress(b"A" * 30000, store_size=False)
    source = ctypes.create_string_buffer(compressed[:3])
    output = ctypes.create_string_buffer(30000)
    assert library.LZ4_decompress_safe(source, output, 3, len(output)) < 0


def test_xrgb8888_to_rgb565(library):
    library.screen_xrgb8888_to_rgb565.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    # XRGB8888 little-endian bytes: B, G, R, X. Red, green, blue, white, plus
    # one mid colour to catch bit-shift errors.
    source = bytes([0, 0, 255, 0,  0, 255, 0, 0,  255, 0, 0, 0,
                    255, 255, 255, 0,  16, 32, 64, 0])
    buffer = ctypes.create_string_buffer(source, len(source))
    library.screen_xrgb8888_to_rgb565(buffer, 5)
    expected = struct.pack("<5H", 0xF800, 0x07E0, 0x001F, 0xFFFF,
                           ((64 >> 3) << 11) | ((32 >> 2) << 5) | (16 >> 3))
    assert buffer.raw[:10] == expected
