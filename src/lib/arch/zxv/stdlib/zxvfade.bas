' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxvfade.bas: palette fades (ZX-V SDK zxv_fade.h, D111): the colours to
' black, to white or to others in some frames, without touching the
' pixels.
'
'   FADECOLOUR RGB565(0, 0, 0), 30: FADEWAIT       ' to black in half a second
'   ... the next level
'   FADETO 0, 30: FADEWAIT                           ' to the colours of always
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXVFADE__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXVFADE__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Which palette tables: all (bit t, table t)
CONST FADEALL as UByte = 15

REM FADETO addr, frames[, tables]: from the colours now to the 16 colours
REM (RGB565) from addr (@pal(0) of a DIM pal(15) as UInteger; 0, the ones
REM of always) in that many frames: FADESTEP takes each step
sub FADETO(addr as ULong, frames as UInteger, tables as UByte = 15)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_fade_to
    end asm
end sub

REM FADECOLOUR rgb, frames[, tables]: all the colours to one (RGB565:
REM RGB565(0, 0, 0) is black)
sub FADECOLOUR(rgb as UInteger, frames as UInteger, tables as UByte = 15)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_fade_colour
    end asm
end sub

REM FADESTEP: one frame more of the fade (nothing if there is none): call it
REM once a frame while the game goes on
sub FADESTEP()
    asm
    zxb_ccall zxb_fade_step
    end asm
end sub

REM FADEBUSY: 1 while there is fade left
function FADEBUSY() as UByte
    asm
    zxb_ccall zxb_fade_busy
    end asm
end function

REM FADEWAIT: does the whole fade, a step every frame, and then goes on
sub FADEWAIT()
    asm
    zxb_ccall zxb_fade_wait
    end asm
end sub

REM RGBMIX(a, b, k, n): the colour k/n of the way from a to b (RGB565)
function RGBMIX(a as UInteger, b as UInteger, k as UInteger, n as UInteger) as UInteger
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_rgb_mix
    end asm
end function

REM RGB565(r, g, b): a colour of r, g, b from 0 to 255 (as PALETTE)
function RGB565(r as UByte, g as UByte, b as UByte) as UInteger
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_rgb565
    end asm
end function

#pragma pop(case_insensitive)

#endif
