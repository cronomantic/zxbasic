' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of sinclair.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_SINCLAIR__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_SINCLAIR__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM The functions of Sinclair BASIC that ZX Basic has as libraries.
REM (The UDG are in the emulated Spectrum memory, at 65368, as in the Spectrum.)
#include once <attr.bas>
#include once <point.bas>
#include once <screen.bas>
#include once <input.bas>

#pragma pop(case_insensitive)

#endif
