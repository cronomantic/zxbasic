' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of input.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_INPUT__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_INPUT__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM A line of up to MaxLen characters, with a cursor, until Enter.
REM As the ZX Spectrum one, it erases what was typed from the screen.
FUNCTION input(MaxLen AS UINTEGER) AS STRING
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_lib_input
    end asm
END FUNCTION

#pragma pop(case_insensitive)

#endif
