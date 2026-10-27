' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of memcopy.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_MEMCOPY__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_MEMCOPY__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Copies or fills memory (32-bit addresses; below 64K, the emulated Spectrum memory)

sub MemMove(source as ulong, dest as ulong, length as ulong)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_lib_memmove
    end asm
end sub

sub MemCopy(source as ulong, dest as ulong, length as ulong)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_lib_memmove
    end asm
end sub

sub MemSet(dest as ulong, value as ubyte, length as ulong)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_lib_memset
    end asm
end sub

#pragma pop(case_insensitive)

#endif
