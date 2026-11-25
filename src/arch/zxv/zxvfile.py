# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""A RISC-V ELF linked with sdk/zxv.ld as a .zxv program (ZX-V spec §9.2): a 72-byte header, the loading
screen (if any) and the contiguous image, and then the code banks (D109, banks.py) with their index. From
sdk/tools/elf2zxv.py of ZX-V (MIT)."""

import struct
import zlib

URAM_BASE, URAM_END = 0x20034000, 0x20070000
# The hidden VRAM of each page in mode 0 (zxv.ld, ZX-V D108): what a program puts there starts
# zeroed (crt0.S) and is not in the file.
VRAM_HIDDEN = ((0x2000A000, 0x2001C000), (0x20022000, 0x20034000))
MAGIC = b"ZXV1"
HDR_SIZE = 72
PROFILE = 1
# The banks (zxv.h ZXV_BANK_*): the code banks are in the ELF in sections .zxvbank.REGION.NAME, with their
# load address from BANK_LMA (not in the RAM)
BANK_LMA = 0x80000000
BANK_MAGIC = b"ZXVBANK1"
BANK_CODE, BANK_STORED, BANK_DEFLATE = 1, 0, 1
BANK_MAX = 4096


class ZxvError(Exception):
    pass


def load_segments(data: bytes):
    """The entry point and the loadable segments (address, contents, size in memory), in order"""
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise ZxvError("not a little-endian ELF32")
    if struct.unpack_from("<H", data, 18)[0] != 243:
        raise ZxvError("not a RISC-V ELF")
    entry, phoff = struct.unpack_from("<II", data, 24)
    phentsize, phnum = struct.unpack_from("<HH", data, 42)
    segs = []
    for i in range(phnum):
        p_type, off, vaddr, paddr, filesz, memsz = struct.unpack_from("<IIIIII", data, phoff + i * phentsize)
        if p_type == 1 and memsz:
            segs.append((paddr, data[off : off + filesz], memsz))
    segs.sort()
    return entry, segs


def code_banks(data: bytes) -> list[tuple[str, int, int, bytes]]:
    """The code banks of the ELF: (name, region, address, contents)"""
    shoff = struct.unpack_from("<I", data, 32)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 46)
    if not shoff or not shnum:
        return []

    def sh(i):
        return struct.unpack_from("<IIIIII", data, shoff + i * shentsize)

    names = sh(shstrndx)[4]
    out = []
    for i in range(shnum):
        name_off, typ, _flags, addr, off, size = sh(i)
        n = data[names + name_off : data.index(b"\0", names + name_off)].decode("ascii", "replace")
        if not n.startswith(".zxvbank."):
            continue
        region, _, bank = n[len(".zxvbank.") :].partition(".")
        if not region.isdigit() or not bank:
            raise ZxvError(f"section {n}: not a code bank")
        body = data[off : off + size] if typ != 8 else bytes(size)  # SHT_NOBITS: zeros
        out.append((bank, int(region), addr, body))
    return out


def bank_data(at: int, prog_crc: int, image_size: int, code: list[tuple[str, int, int, bytes]]) -> bytes:
    """The code banks (each one with deflate if it is smaller), their index and the tail, to go after the
    image, which ends at `at`"""
    if not code:
        return b""
    if len(code) > BANK_MAX:
        raise ZxvError(f"too many banks (at most {BANK_MAX})")
    data, entries = b"", b""
    for name, region, addr, raw in code:
        stored, method = raw, BANK_STORED
        c = zlib.compressobj(9, zlib.DEFLATED, -15)
        z = c.compress(raw) + c.flush()
        if len(z) < len(raw):
            stored, method = z, BANK_DEFLATE
        entries += struct.pack(
            "<16sBBBBIIIIIII",
            name.encode("ascii"),
            BANK_CODE,
            method,
            region,
            0,
            at + len(data),
            len(stored),
            len(raw),
            zlib.crc32(raw) & 0xFFFFFFFF,
            addr,
            0,
            0,
        )
        data += stored
    index = struct.pack("<IIII", len(code), prog_crc, image_size, zlib.crc32(entries) & 0xFFFFFFFF) + entries
    return data + index + BANK_MAGIC + struct.pack("<II", at + len(data), len(index))


def zxv_from_elf(elf: bytes, name: str, screen: bytes = b"") -> bytes:
    """The .zxv file of an ELF; name is the program's name (up to 27 characters) and screen the loading
    screen, the data of a .gfx screen object (src/arch/zxv/screen.py), or nothing"""
    entry, segs = load_segments(elf)
    code = code_banks(elf)
    segs = [s for s in segs if s[0] < BANK_LMA]  # The code banks go apart
    hidden = [s for s in segs if any(lo <= s[0] and s[0] + s[2] <= hi for lo, hi in VRAM_HIDDEN)]
    for addr, content, _ in hidden:
        if content:
            raise ZxvError(f"data with an initial value in the hidden VRAM ({addr:#x}): it is not loaded")
    segs = [s for s in segs if s not in hidden]
    if not segs:
        raise ZxvError("the ELF has no loadable segments")

    load = segs[0][0]
    image = bytearray()
    mem_end = load
    for addr, content, memsz in segs:
        if addr < load + len(image):
            raise ZxvError(f"overlapping segments at {addr:#x}")
        if content:
            image += b"\0" * (addr - (load + len(image)))
            image += content
        mem_end = max(mem_end, addr + memsz)
    for _, _, addr, body in code:  # Their regions are memory of the program too (after the BSS)
        mem_end = max(mem_end, addr + len(body))
    bss = mem_end - (load + len(image))

    if load < URAM_BASE or mem_end > URAM_END:
        raise ZxvError(f"the program ({load:#x}-{mem_end:#x}) does not fit in the user RAM")
    if load % 4 or not (load <= entry < load + len(image)):
        raise ZxvError("invalid load address or entry point")

    stack, mode, flags = 0, 0, 0  # The current stack, mode 0
    hdr = struct.pack(
        "<4sHHIIIIIBBHI28sII",
        MAGIC,
        HDR_SIZE,
        PROFILE,
        load,
        entry,
        len(image),
        bss,
        stack,
        mode,
        flags,
        0,
        zlib.crc32(image) & 0xFFFFFFFF,
        name.encode("ascii", "replace")[:27],
        len(screen),
        0,
    )
    assert len(hdr) == HDR_SIZE
    out = hdr + screen + bytes(image)
    return out + bank_data(len(out), zlib.crc32(image) & 0xFFFFFFFF, len(image), code)
