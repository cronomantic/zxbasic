' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of random.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_RANDOM__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_RANDOM__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Random numbers (xorshift32)

function FASTCALL randInt() as ULong
    asm
    zxb_ccall zxb_lib_rand
    end asm
end function

function FASTCALL randFixed() as Fixed
    asm
    zxb_ccall zxb_lib_rand
    end asm
end function

REM From 0 to limit (both included)
function randomLimit(limit as uByte) as uByte
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_lib_rand_limit
    end asm
end function

#pragma pop(case_insensitive)

#endif
