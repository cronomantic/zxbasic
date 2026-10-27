' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of screen.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_SCREEN__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_SCREEN__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM The character at (row, col), or "" if there is none
function screen(byval row as ubyte, byval col as ubyte) as string
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_lib_screen
    end asm
end function

#pragma pop(case_insensitive)

#endif
