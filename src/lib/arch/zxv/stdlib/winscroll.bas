' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of winscroll.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and Conrado Badenas). The runtime
' (zxb_screen.c) does the work.
' ----------------------------------------------------------------

#ifndef __LIBRARY_WINSCROLL__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_WINSCROLL__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Scrolls 1 cell the window of width x height cells at (row, col), with
REM its attributes. The cells that are left blank keep their attributes.
REM Nothing is done if width or height are 0.

sub WinScrollRight(row as uByte, col as uByte, width as Ubyte, height as Ubyte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    li a4, 0
    zxb_ccall zxb_lib_winscroll
    end asm
end sub

sub WinScrollLeft(row as uByte, col as uByte, width as Ubyte, height as Ubyte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    li a4, 1
    zxb_ccall zxb_lib_winscroll
    end asm
end sub

sub WinScrollUp(row as uByte, col as uByte, width as Ubyte, height as Ubyte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    li a4, 2
    zxb_ccall zxb_lib_winscroll
    end asm
end sub

sub WinScrollDown(row as uByte, col as uByte, width as Ubyte, height as Ubyte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    li a4, 3
    zxb_ccall zxb_lib_winscroll
    end asm
end sub

#pragma pop(case_insensitive)

#endif
