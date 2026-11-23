' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxvfont.bas: text of any size and colour, proportional, with accents
' (ZX-V SDK zxv_font.h, D111). Four fonts (0-3): the one of the ROM, or one
' drawn on the page in cells (FONTGRAB).
'
'   FONTROM 0
'   LET w = FONTWIDTH(0, "GAME OVER", 3)
'   LET x = FONTDRAW(0, (256 - w) / 2, 80, "GAME OVER", 14, 3)
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXVFONT__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXVFONT__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM FONTDRAW with the colours of the font (FONTGRAB)
CONST FONTOWN as Integer = -1

REM FONTROM n[, fixedw]: font n (0-3) is the 8 x 8 of the ROM, proportional or,
REM with fixedw = 1, of fixed width
sub FONTROM(n as UByte, fixedw as UByte = 0)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_font_rom
    end asm
end sub

REM FONTGRAB n, x, y, w, h, cw, ch[, first, t, fixedw]: font n is the
REM rectangle of the drawing page, cut in cells of cw x ch with the
REM characters from first (32) on, left to right and top to bottom, in
REM their colours; colour t becomes transparent
sub FONTGRAB(n as UByte, x as Integer, y as Integer, w as UInteger, h as UInteger, cw as UByte, ch as UByte, first as UInteger = 32, t as Integer = -1, fixedw as UByte = 0)
    asm
    addi a0, s0, 8
    zxb_ccall zxb_font_grab
    end asm
end sub

REM FONTSPACING n, s: the pixels between letters (1 in the proportional ones)
sub FONTSPACING(n as UByte, s as UByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_font_spacing
    end asm
end sub

REM FONTDRAW(n, x, y, t$, colour[, scale]): draws t$ (\n, CHR$ 10, goes to
REM the next line) with its top left corner at (x, y), of colour (FONTOWN:
REM the colours of the font) and scale times bigger (1-8); with CLIP and
REM PATTERN. It gives the x where it ends
function FONTDRAW(n as UByte, x as Integer, y as Integer, ByVal text as String, colour as Integer, scale as UByte = 1) as Integer
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    lw a5, 28(s0)
    zxb_ccall zxb_font_draw
    end asm
end function

REM FONTWIDTH(n, t$[, scale]): the width of its longest line
function FONTWIDTH(n as UByte, ByVal text as String, scale as UByte = 1) as Integer
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_font_width
    end asm
end function

REM FONTHEIGHT(n, t$[, scale]): the height of all its lines
function FONTHEIGHT(n as UByte, ByVal text as String, scale as UByte = 1) as Integer
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_font_height
    end asm
end function

#pragma pop(case_insensitive)

#endif
