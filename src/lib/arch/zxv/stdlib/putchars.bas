' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of putchars.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_PUTCHARS__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_PUTCHARS__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Boxes of character cells: x, y (cell), width, height (cells). The data
REM are 8 bytes per cell, column by column (each column from top to bottom);
REM the attributes, one byte per cell, row by row.

SUB putChars(x as uByte, y as uByte, width as uByte, height as uByte, dataAddress as ULong)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    li a5, 0
    zxb_ccall zxb_lib_putchars
    end asm
END SUB

SUB getChars(x as uByte, y as uByte, width as uByte, height as uByte, dataAddress as ULong)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    li a5, 1
    zxb_ccall zxb_lib_putchars
    end asm
END SUB

SUB paint(x as uByte, y as uByte, width as uByte, height as uByte, attribute as uByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    zxb_ccall zxb_lib_paint
    end asm
END SUB

SUB paintData(x as uByte, y as uByte, width as uByte, height as uByte, address as ULong)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    li a5, 0
    zxb_ccall zxb_lib_paintdata
    end asm
END SUB

SUB getPaintData(x as uByte, y as uByte, width as uByte, height as uByte, address as ULong)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    li a5, 1
    zxb_ccall zxb_lib_paintdata
    end asm
END SUB

#pragma pop(case_insensitive)

#endif
