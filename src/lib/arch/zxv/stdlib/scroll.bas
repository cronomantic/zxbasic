' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of scroll.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and Conrado Badenas). The runtime
' (zxb_screen.c) does the work.
' ----------------------------------------------------------------

#ifndef __LIBRARY_SCROLL__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_SCROLL__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Pixel by pixel scrolls of the window from (x1, y1) to (x2, y2), both
REM included, with the PLOT coordinates (y = 0 at the bottom). What comes in
REM is blank; the attributes do not change.

sub ScrollRight(x1 as uByte, y1 as uByte, x2 as Ubyte, y2 as Ubyte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    li a4, 0
    zxb_ccall zxb_lib_scroll
    end asm
end sub

sub ScrollLeft(x1 as uByte, y1 as uByte, x2 as Ubyte, y2 as Ubyte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    li a4, 1
    zxb_ccall zxb_lib_scroll
    end asm
end sub

sub ScrollUp(x1 as uByte, y1 as uByte, x2 as Ubyte, y2 as Ubyte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    li a4, 2
    zxb_ccall zxb_lib_scroll
    end asm
end sub

sub ScrollDown(x1 as uByte, y1 as uByte, x2 as Ubyte, y2 as Ubyte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    li a4, 3
    zxb_ccall zxb_lib_scroll
    end asm
end sub

#pragma pop(case_insensitive)

#endif
