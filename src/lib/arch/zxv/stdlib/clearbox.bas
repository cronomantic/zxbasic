' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of clearbox.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_CLEARBOX__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_CLEARBOX__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Blanks the pixels of a box of character cells (the attributes do not change)
SUB clearBox(x as uByte, y as uByte, width as uByte, height as uByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_lib_clearbox
    end asm
END SUB

#pragma pop(case_insensitive)

#endif
