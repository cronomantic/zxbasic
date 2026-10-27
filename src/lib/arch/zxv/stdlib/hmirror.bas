' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of hmirror.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_HMIRROR__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_HMIRROR__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM The byte with its bits in the opposite order
function hMirror(number as uByte) as uByte
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_lib_hmirror
    end asm
end function

#pragma pop(case_insensitive)

#endif
