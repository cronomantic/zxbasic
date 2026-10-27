' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of point.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_POINT__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_POINT__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM 1 if the pixel (x, y) is set (y = 0 at the bottom, as PLOT), -1 out of the screen
function point(x as ubyte, y as ubyte) as byte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_lib_point
    end asm
end function

#pragma pop(case_insensitive)

#endif
