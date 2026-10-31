# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""The loading screen of a .zxv program (zxbc --arch=zxv --loading-screen FILE): the data of a .gfx screen
object (ZX-V decision D89, dev/basic.md §9.8): the mode (0 or 1), the border colour (0-15), 0 (u16) and the
pixels of the page as in the VRAM, 4 bits each (the low nibble is the even pixel), 192 lines of 128 bytes in
mode 0 or 384 lines of 256 bytes in mode 1.

From a PNG (256x192 is mode 0 and 512x384 mode 1, with border 0; the frame with its border, 320x240 or
640x480, takes the border colour of its top left corner) or the first screen of a .gfx. From
sdk/tools/png2gfx.py of ZX-V (MIT)."""

import struct
import zlib

from src.arch.zxv.zxvfile import ZxvError

HEAD = 4  # Mode, border, 0 (u16)
TRANSPARENT = 8  # Colour 8 is the transparent one in sprites; in a screen it is black (0)

# The active area inside the image, by size: mode, x, y (ZX-V spec §4.4)
FRAMES = {(256, 192): (0, 0, 0), (512, 384): (1, 0, 0), (320, 240): (0, 32, 24), (640, 480): (1, 64, 48)}


def pixels_size(mode: int) -> int:
    return 98304 if mode else 24576


def palette() -> list[tuple[int, int, int]]:
    """The 16 colours of ZX-V (spec §4.3): BRIGHT-G-R-B"""
    pal = []
    for i in range(16):
        lvl = 0xFF if i & 8 else 0xD7
        pal.append(((lvl if i & 2 else 0), (lvl if i & 4 else 0), (lvl if i & 1 else 0)))
    return pal


PALETTE = palette()
EXACT = {c: i for i, c in enumerate(PALETTE) if i != TRANSPARENT}


def colour(r: int, g: int, b: int, a: int) -> int:
    """The ZX-V colour of a pixel: the exact one, or the nearest one; transparent pixels are black"""
    if a < 128:
        return 0
    if (r, g, b) in EXACT:
        return EXACT[(r, g, b)]
    return min(
        (i for i in range(16) if i != TRANSPARENT),
        key=lambda i: (PALETTE[i][0] - r) ** 2 + (PALETTE[i][1] - g) ** 2 + (PALETTE[i][2] - b) ** 2,
    )


def read_png(data: bytes) -> tuple[int, int, list[list[tuple[int, int, int, int]]]]:
    """The width, the height and the rows of pixels (r, g, b, a) of a PNG"""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ZxvError("not a PNG")
    pos, ihdr, plte, trns, idat = 8, None, None, None, []
    while pos + 8 <= len(data):
        n, kind = struct.unpack(">I4s", data[pos : pos + 8])
        body = data[pos + 8 : pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            ihdr = body
        elif kind == b"PLTE":
            plte = body
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
    if not ihdr or not idat:
        raise ZxvError("incomplete PNG")
    w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", ihdr)
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(ctype)
    if channels is None or depth not in (1, 2, 4, 8, 16) or (ctype in (2, 4, 6) and depth < 8):
        raise ZxvError("invalid PNG type")
    if interlace:
        raise ZxvError("interlaced PNGs are not supported")
    if ctype == 3 and not plte:
        raise ZxvError("the PNG palette is missing")
    try:
        raw = zlib.decompress(b"".join(idat))
    except zlib.error:
        raise ZxvError("corrupt PNG data")
    bits = channels * depth
    stride = (w * bits + 7) // 8
    bpp = max(1, bits // 8)
    if len(raw) < h * (stride + 1):
        raise ZxvError("incomplete PNG data")

    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1 : (y + 1) * (stride + 1)])
        if f == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif f == 3:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif f == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        elif f != 0:
            raise ZxvError(f"invalid PNG filter {f}")
        rows.append(line)
        prev = line

    def samples(line: bytearray) -> list[int]:
        if depth == 8:
            return list(line)
        if depth == 16:
            return [(line[i] << 8) | line[i + 1] for i in range(0, len(line), 2)]
        per = 8 // depth
        mask = (1 << depth) - 1
        return [(line[i // per] >> (8 - depth * (i % per + 1))) & mask for i in range(w * channels)]

    top = (1 << depth) - 1
    alpha_pal = list(trns) if (ctype == 3 and trns) else []
    key = None
    if trns and ctype == 0:
        key = struct.unpack(">H", trns[:2])[0]
    elif trns and ctype == 2:
        key = struct.unpack(">HHH", trns[:6])
    out = []
    for line in rows:
        s = samples(line)
        px = []
        for x in range(w):
            v = s[x * channels : (x + 1) * channels]
            if ctype == 3:
                i = v[0]
                if 3 * i + 2 >= len(plte):
                    raise ZxvError("PNG palette index outside the palette")
                a = alpha_pal[i] if i < len(alpha_pal) else 255
                px.append((plte[3 * i], plte[3 * i + 1], plte[3 * i + 2], a))
                continue
            sc = [c * 255 // top for c in v]
            if ctype == 0:
                px.append((sc[0], sc[0], sc[0], 0 if key == v[0] else 255))
            elif ctype == 2:
                px.append((sc[0], sc[1], sc[2], 0 if key == tuple(v) else 255))
            elif ctype == 4:
                px.append((sc[0], sc[0], sc[0], sc[1]))
            else:
                px.append((sc[0], sc[1], sc[2], sc[3]))
        out.append(px)
    return w, h, out


def png_screen(data: bytes) -> bytes:
    """The screen of a PNG"""
    w, h, rgba = read_png(data)
    if (w, h) not in FRAMES:
        raise ZxvError(
            "a loading screen is 256x192 or 512x384 (or the frame with its border, 320x240 or 640x480), "
            f"not {w}x{h}"
        )
    mode, x0, y0 = FRAMES[(w, h)]
    cache: dict[tuple[int, int, int, int], int] = {}

    def at(x: int, y: int) -> int:
        p = rgba[y][x]
        if p not in cache:
            cache[p] = colour(*p)
        return cache[p]

    border = at(0, 0) if x0 else 0
    pw, ph = (512, 384) if mode else (256, 192)
    pixels = bytearray()
    for y in range(y0, y0 + ph):
        for x in range(x0, x0 + pw, 2):
            pixels.append(at(x, y) | at(x + 1, y) << 4)
    return bytes([mode, border, 0, 0]) + bytes(pixels)


def check_screen(data: bytes) -> bool:
    """Is it the data of a valid screen object?"""
    return len(data) >= HEAD and data[0] <= 1 and data[1] <= 15 and len(data) == HEAD + pixels_size(data[0])


def gfx_screen(data: bytes) -> bytes:
    """The first screen of a .gfx"""
    if data[:4] != b"ZXVG" or len(data) < 8 or data[4] != 1:
        raise ZxvError("not a .gfx graphics file")
    (count,) = struct.unpack_from("<H", data, 6)
    pos = 8
    for _ in range(count):
        if pos + 8 > len(data):
            break
        kind, _, num, length = struct.unpack_from("<BBHI", data, pos)
        if kind == 5:
            screen = data[pos + 8 : pos + 8 + length]
            if not check_screen(screen):
                raise ZxvError(f"the screen {num} of the .gfx file is not valid")
            return bytes(screen)
        pos += 8 + length
    raise ZxvError("the .gfx file has no screen")


def loading_screen(path: str) -> bytes:
    """The loading screen of a PNG or a .gfx file"""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as e:
        raise ZxvError(f"{path}: {e.strerror}")
    try:
        return gfx_screen(data) if data[:4] == b"ZXVG" else png_screen(data)
    except ZxvError as e:
        raise ZxvError(f"{path}: {e}")
