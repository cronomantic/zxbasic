# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""The loading screen of the zxv target: its option, the PNG and .gfx readers and the .zxv header"""

import os
import struct
import zlib

import pytest

from src.arch.zxv import screen
from src.arch.zxv.zxvfile import HDR_SIZE, ZxvError, zxv_from_elf
from src.zxbc import args_parser

HERE = os.path.dirname(os.path.abspath(__file__))
TEST_PNG = os.path.join(HERE, "..", "..", "zxv", "loadscreen.png")


def png(w: int, h: int, rows: list[bytes]) -> bytes:
    """An RGB PNG (8 bits, filter 0)"""

    def chunk(kind: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    raw = b"".join(b"\0" + r for r in rows)
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


def elf() -> bytes:
    """A minimal RISC-V ELF32 with one loadable segment of 8 bytes at the start of the user RAM"""
    load = 0x20034000
    code = b"\x13\x00\x00\x00\x67\x80\x00\x00"  # nop; ret
    hdr = bytearray(52)
    hdr[:6] = b"\x7fELF\x01\x01"
    struct.pack_into("<H", hdr, 18, 243)
    struct.pack_into("<II", hdr, 24, load, 52)
    struct.pack_into("<HH", hdr, 42, 32, 1)
    ph = struct.pack("<IIIIIIII", 1, 84, load, load, len(code), len(code), 5, 4)
    return bytes(hdr) + ph + code


def test_option_only_for_zxv():
    assert args_parser.get_target_arch(["--arch", "zxv", "x.bas"]) == "zxv"
    assert args_parser.get_target_arch(["--arch=zxv", "x.bas"]) == "zxv"
    assert args_parser.get_target_arch(["x.bas"]) == "zx48k"
    options = args_parser.parser("zxv").parse_args(["--arch", "zxv", "--loading-screen", "s.png", "x.bas"])
    assert options.loading_screen == "s.png"
    with pytest.raises(SystemExit):
        args_parser.parser("zx48k").parse_args(["--loading-screen", "s.png", "x.bas"])


def test_png_frame():
    """The frame with its border: the border colour is the one of the corner"""
    blue, red, white = (0, 0, 0xD7), (0xD7, 0, 0), (0xFF, 0xFF, 0xFF)
    rows = []
    for y in range(240):
        row = bytearray()
        for x in range(320):
            inside = 32 <= x < 288 and 24 <= y < 216
            row += bytes(red if inside and x % 2 == 0 else white if inside else blue)
        rows.append(bytes(row))
    data = screen.png_screen(png(320, 240, rows))
    assert len(data) == 4 + 24576
    assert data[:4] == bytes([0, 1, 0, 0])  # Mode 0, blue border
    assert data[4] == 2 | 15 << 4  # Red (even pixel), bright white


def test_rgb565():
    """The colours in RGB565 as ZXV_RGB and ZXV_STD_RGB565 of zxv.h: 215 is the normal level, 255 the bright"""
    assert screen.rgb565(0xD7, 0, 0) == 27 << 11
    assert screen.rgb565(0xFF, 0xFF, 0xFF) == 0xFFFF
    assert screen.rgb565(10, 200, 30) == 1 << 11 | 50 << 5 | 4
    assert screen.STD565[7] == 27 << 11 | 54 << 5 | 27
    assert screen.STD565[8] == screen.STD565[0] == 0
    assert screen.STD_INDEX[0] == 0  # Black is 0, not 8


def test_png_palette():
    """Colours that are not the standard ones: a palette, with the standard ones at their index and the others
    in the free ones, in the order they appear"""
    black, green, red, orange = (0, 0, 0), (10, 200, 30), (0xD7, 0, 0), (250, 128, 0)
    rows = [b"".join(bytes((black, green, red, orange)[x % 4]) for x in range(256))] * 192
    data = screen.png_screen(png(256, 192, rows))
    assert len(data) == 4 + 32 + 24576
    assert data[:4] == bytes([0, 0, 1, 0])  # Mode 0, border 0, with a palette
    pal = list(struct.unpack_from("<16H", data, 4))
    want = list(screen.STD565)
    want[1], want[3] = screen.rgb565(*green), screen.rgb565(*orange)
    assert pal == want
    assert data[36:38] == bytes([0 | 1 << 4, 2 | 3 << 4])


def test_png_palette_frame():
    """The colour of the border corner goes the last one, and 0 is free if black is not used"""
    grey, red, white = (99, 99, 99), (0xD7, 0, 0), (0xD7, 0xD7, 0xD7)
    rows = []
    for y in range(240):
        inside = [24 <= y < 216 and 32 <= x < 288 for x in range(320)]
        rows.append(b"".join(bytes((red if x % 2 else white) if inside[x] else grey) for x in range(320)))
    data = screen.png_screen(png(320, 240, rows))
    assert data[:4] == bytes([0, 0, 1, 0])  # The grey border, at index 0
    pal = struct.unpack_from("<16H", data, 4)
    assert pal[0] == screen.rgb565(*grey) and pal[2] == screen.STD565[2] and pal[7] == screen.STD565[7]
    assert data[36] == 7 | 2 << 4


def test_png_palette_8_last():
    """Index 8 (the transparent one of the sprites) is the last free one"""
    greys = [(i * 16, i * 16, i * 16) for i in range(1, 16)]  # 15 colours that are not standard, and black
    rows = [b"".join(bytes(([(0, 0, 0)] + greys)[x % 16]) for x in range(256))] * 192
    data = screen.png_screen(png(256, 192, rows))
    pal = struct.unpack_from("<16H", data, 4)
    assert pal[0] == 0
    assert [pal[i] for i in (1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 8)] == [screen.rgb565(*g) for g in greys]
    rows = [b"".join(bytes(greys[x % 14]) for x in range(256))] * 192  # 14 colours: 8 stays standard
    pal = struct.unpack_from("<16H", screen.png_screen(png(256, 192, rows)), 4)
    assert pal[8] == screen.STD565[8] and pal[0] == screen.rgb565(*greys[0])


def test_png_too_many_colours():
    rows = [b"".join(bytes((x % 17 * 10, 0, 0)) for x in range(256))] * 192
    with pytest.raises(ZxvError, match="16 colours"):
        screen.png_screen(png(256, 192, rows))


def test_png_size():
    with pytest.raises(ZxvError):
        screen.png_screen(png(2, 2, [b"\0" * 6, b"\0" * 6]))


def test_gfx_first_screen():
    sprite = struct.pack("<HH", 1, 1) + b"\x05"
    shot = bytes([1, 9, 0, 0]) + b"\x12" * 98304
    gfx = b"ZXVG" + struct.pack("<BBH", 1, 0, 2)
    gfx += struct.pack("<BBHI", 1, 0, 0, len(sprite)) + sprite
    gfx += struct.pack("<BBHI", 5, 0, 0, len(shot)) + shot
    assert screen.gfx_screen(gfx) == shot
    with pytest.raises(ZxvError):
        screen.gfx_screen(gfx[: 8 + 8 + len(sprite)].replace(b"\x02\x00", b"\x01\x00", 1))


def test_gfx_screen_palette():
    """A screen with its palette goes as it is; unknown flags are not valid"""
    pal = struct.pack("<16H", *range(0x1000, 0x1010))
    shot = bytes([0, 4, 1, 0]) + pal + b"\x34" * 24576

    def gfx(s: bytes) -> bytes:
        return b"ZXVG" + struct.pack("<BBH", 1, 0, 1) + struct.pack("<BBHI", 5, 0, 0, len(s)) + s

    assert screen.gfx_screen(gfx(shot)) == shot
    for bad in (
        bytes([0, 4, 3, 0]) + pal + b"\x34" * 24576,  # Flag bit 1
        bytes([0, 4, 0, 0]) + pal + b"\x34" * 24576,  # A palette without its flag
        bytes([0, 4, 1, 0]) + b"\x34" * 24576,  # The flag without a palette
        bytes([0, 4, 1, 1]) + pal + b"\x34" * 24576,  # Byte 3 is 0
    ):
        with pytest.raises(ZxvError):
            screen.gfx_screen(gfx(bad))


def test_header_with_screen():
    shot = bytes([0, 2, 0, 0]) + b"\x22" * 24576
    plain = zxv_from_elf(elf(), "prog")
    with_screen = zxv_from_elf(elf(), "prog", shot)
    assert HDR_SIZE == 72
    assert struct.unpack_from("<H", plain, 4)[0] == 72
    assert struct.unpack_from("<II", plain, 0x40) == (0, 0)
    assert struct.unpack_from("<II", with_screen, 0x40) == (len(shot), 0)
    assert with_screen[72 : 72 + len(shot)] == shot
    assert with_screen[72 + len(shot) :] == plain[72:]


def test_test_png():
    """The loading screen of tests/zxv/loadscreen.bas"""
    data = screen.loading_screen(TEST_PNG)
    assert data[:4] == bytes([0, 3, 0, 0])
