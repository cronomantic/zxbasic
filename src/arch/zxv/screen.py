# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""The loading screen of a .zxv program (zxbc --arch=zxv --loading-screen FILE): the data of a .gfx screen
object (ZX-V decisions D89 and D90, dev/basic.md §9.8): the mode (0 or 1), the border colour (0-15), the flags
(bit 0: it has a palette), 0, the palette if it has one (16 RGB565 colours, u16) and the pixels of the page as
in the VRAM, 4 bits each (the low nibble is the even pixel), 192 lines of 128 bytes in mode 0 or 384 lines of
256 bytes in mode 1.

From a PNG (256x192 is mode 0 and 512x384 mode 1, with border 0; the frame with its border, 320x240 or
640x480, takes the border colour of its top left corner) or the first screen of a .gfx, with its palette. A
PNG keeps its colours (in RGB565): if they are not the standard ones, the screen has a palette with them (16
at most). From sdk/tools/png2gfx.py of ZX-V (MIT)."""

import struct
import zlib

from src.arch.zxv.zxvfile import ZxvError

HEAD = 4  # Mode, border, flags, 0
F_PALETTE = 0x01  # The flags: it has a palette, 16 RGB565 colours (32 bytes) before the pixels
PALETTE_SIZE = 32
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


def to5(v: int) -> int:
    """A channel of 8 bits in 5, with the curve of the output (ZXV_TO5 in zxv.h, spec §4.1)"""
    return (v * 27 + 107) // 215 if v <= 215 else 27 + ((v - 215) * 4 + 20) // 40


def to6(v: int) -> int:
    """The green of 8 bits in 6 (ZXV_TO6)"""
    return (v * 54 + 107) // 215 if v <= 215 else 54 + ((v - 215) * 9 + 20) // 40


def rgb565(r: int, g: int, b: int) -> int:
    """A colour of 8 bits a channel in RGB565, as ZXV_RGB"""
    return to5(r) << 11 | to6(g) << 5 | to5(b)


# The standard colours in RGB565 (ZXV_STD_RGB565) and the index of each one (black is 0)
STD565 = [rgb565(*c) for c in PALETTE]
STD_INDEX = {c: i for i, c in reversed(list(enumerate(STD565)))}


def screen_palette(colours: list[int]) -> list[int] | None:
    """The palette of a screen with these RGB565 colours (different, in the order they appear): None if they are
    all standard ones; otherwise the 16 colours, each standard one at its own index (black at 0) and the others
    in the free ones, in order (8 the last: it is the transparent one of the sprites), and the standard colours
    in those not used. An error if they are more than 16."""
    if all(c in STD_INDEX for c in colours):
        return None
    if len(colours) > 16:
        raise ZxvError(
            f"a screen has up to 16 colours and this one has {len(colours)} "
            "(reduce them to 16, or use the ZX-V standard ones)"
        )
    pal, used = list(STD565), [False] * 16
    for c in colours:
        if c in STD_INDEX:
            used[STD_INDEX[c]] = True
    free = [i for i in range(16) if not used[i] and i != TRANSPARENT] + ([] if used[TRANSPARENT] else [TRANSPARENT])
    for c, i in zip([c for c in colours if c not in STD_INDEX], free):
        pal[i] = c
    return pal


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
    """The screen of a PNG. Each pixel keeps its colour in RGB565 (transparent pixels are black): if they are
    all standard ones, no palette; otherwise 16 at most, with the palette of screen_palette (the colours in the
    order they appear, row by row, and the border corner the last)"""
    w, h, rgba = read_png(data)
    if (w, h) not in FRAMES:
        raise ZxvError(
            "a loading screen is 256x192 or 512x384 (or the frame with its border, 320x240 or 640x480), "
            f"not {w}x{h}"
        )
    mode, x0, y0 = FRAMES[(w, h)]
    pw, ph = (512, 384) if mode else (256, 192)
    cache: dict[tuple[int, int, int, int], int] = {}

    def c565(p: tuple[int, int, int, int]) -> int:
        if p not in cache:
            r, g, b, a = p
            cache[p] = 0 if a < 128 else rgb565(r, g, b)
        return cache[p]

    area = [[c565(p) for p in row[x0 : x0 + pw]] for row in rgba[y0 : y0 + ph]]
    corner = c565(rgba[0][0]) if x0 else None
    seen = dict.fromkeys(c for row in area for c in row)
    if corner is not None:
        seen.setdefault(corner)
    pal = screen_palette(list(seen))
    index = STD_INDEX if pal is None else {c: i for i, c in reversed(list(enumerate(pal)))}
    border = 0 if corner is None else index[corner]
    head = bytes([mode, border, 0 if pal is None else F_PALETTE, 0])
    if pal is not None:
        head += struct.pack("<16H", *pal)
    pixels = bytearray()
    for row in area:
        for x in range(0, pw, 2):
            pixels.append(index[row[x]] | index[row[x + 1]] << 4)
    return head + bytes(pixels)


def screen_size(data: bytes) -> int:
    """The size of a screen object by its head, or -1 if it is not valid (unknown flags, too)"""
    if len(data) < HEAD or data[0] > 1 or data[1] > 15 or data[2] & ~F_PALETTE or data[3]:
        return -1
    return HEAD + (PALETTE_SIZE if data[2] & F_PALETTE else 0) + pixels_size(data[0])


def check_screen(data: bytes) -> bool:
    """Is it the data of a valid screen object?"""
    return len(data) == screen_size(data)


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
