' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of hex.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_HEX__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_HEX__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Hexadecimal: 8, 4 or 2 digits
function hex(num as ULong) as String
    asm
    lw a0, 8(s0)
    li a1, 8
    zxb_ccall zxb_lib_hex
    end asm
end function

function hex16(n as UInteger) as String
    asm
    lw a0, 8(s0)
    li a1, 4
    zxb_ccall zxb_lib_hex
    end asm
end function

function hex8(n as UByte) as String
    asm
    lw a0, 8(s0)
    li a1, 2
    zxb_ccall zxb_lib_hex
    end asm
end function

#pragma pop(case_insensitive)

#endif
