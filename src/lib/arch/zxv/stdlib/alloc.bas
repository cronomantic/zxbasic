' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of alloc.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_ALLOC__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ALLOC__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Dynamic memory: the heap of the strings. Addresses are 32 bits in ZX-V.
REM NULL (0) if there is not enough memory.

function allocate(byval n as ulong) as ulong
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_try_alloc
    end asm
end function

function callocate(byval n as ulong) as ulong
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_lib_callocate
    end asm
end function

sub deallocate(byval addr as ulong)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_free
    end asm
end sub

function reallocate(byval addr as ulong, byval n as ulong) as ulong
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_realloc
    end asm
end function

REM The free memory, and the largest block
function FASTCALL memavail as ulong
    asm
    li a0, 0
    zxb_ccall zxb_mem_avail
    end asm
end function

function FASTCALL maxavail as ulong
    asm
    li a0, 1
    zxb_ccall zxb_mem_avail
    end asm
end function

#pragma pop(case_insensitive)

#endif
