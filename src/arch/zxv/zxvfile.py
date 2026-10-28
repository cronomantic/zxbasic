# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""A RISC-V ELF linked with sdk/zxv.ld as a .zxv program (ZX-V spec §9.2): a 64-byte header and the
contiguous image. From sdk/tools/elf2zxv.py of ZX-V (MIT)."""

import struct
import zlib

URAM_BASE, URAM_END = 0x20034000, 0x20072000
MAGIC = b"ZXV1"
HDR_SIZE = 64
PROFILE = 1


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


def zxv_from_elf(elf: bytes, name: str) -> bytes:
    """The .zxv file of an ELF; name is the program's name (up to 27 characters)"""
    entry, segs = load_segments(elf)
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
    bss = mem_end - (load + len(image))

    if load < URAM_BASE or mem_end > URAM_END:
        raise ZxvError(f"the program ({load:#x}-{mem_end:#x}) does not fit in the user RAM")
    if load % 4 or not (load <= entry < load + len(image)):
        raise ZxvError("invalid load address or entry point")

    stack, mode, flags = 0, 0, 0  # The current stack, mode 0
    hdr = struct.pack(
        "<4sHHIIIIIBBHI28s",
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
    )
    assert len(hdr) == HDR_SIZE
    return hdr + bytes(image)
