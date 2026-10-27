' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of attr.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_ATTR__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ATTR__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM The attributes are in the Spectrum memory (22528), emulated in ZX-V

function attr(byval row as ubyte, byval col as ubyte) as ubyte
    return PEEK(22528 + 32 * CAST(UINTEGER, row) + col)
end function

sub setattr(byval row as ubyte, byval col as ubyte, byval value as ubyte)
    POKE 22528 + 32 * CAST(UINTEGER, row) + col, value
end sub

function attraddr(byval row as ubyte, byval col as ubyte) as uinteger
    return 22528 + 32 * CAST(UINTEGER, row) + col
end function

#pragma pop(case_insensitive)

#endif
