' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxv.bas: what is only in ZX-V, with the names and arguments of the ZX-V
' BASIC commands (its manual, "Modos de vídeo y efectos", "Sprites, mapas de
' tiles y gráficos rápidos", KEY, JOY and SOUND). The runtime (zxb_zxv.c)
' calls the engine of the ROM.
'
' The fast graphics (SPRITE, BOX, DISC, POLY, PIXEL...) count pixels from the
' top left corner of the drawing page, with the y downwards; colours are
' 0-15 and 8 is transparent in sprites. PRINT, PLOT and CLS draw on the
' top left 256 x 192 pixels of the drawing page, in any mode.
'
' Not as in the ZX-V BASIC:
'  - The sprite of a string is GRABSTR n, w, a$ (GRAB n, w, a$ there).
'  - KEY, JOY and HIT are functions: KEY(9), not KEY 9.
'  - REMAP and SOUND take one pair at a time, and REMAP alone is REMAPRESET.
'  - POLY takes up to 8 points.
'  - SCREEN can not be used with screen.bas (its SCREEN$ is also screen).
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXV__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXV__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM MODE n[, w, h]: mode 0 (256 x 192, 32 x 24 characters) or 1 (512 x 384).
REM In mode 0 the page can be bigger than the screen (512 x 384, 1024 x 192
REM or 256 x 768), and SCROLL moves the screen over it. Both pages are
REM cleared, page 0 is shown and drawn on, and the scroll, REMAP and CLIP go.
sub MODE(m as UByte, w as UInteger = 0, h as UInteger = 0)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_zxv_mode
    end asm
end sub

REM PAGE n: the page that is shown (0 or 1)
sub PAGE(n as UByte)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_zxv_page
    end asm
end sub

REM SCREEN n: the page everything draws on. Drawing on the hidden one and
REM then showing it gives animations without flicker.
sub SCREEN(n as UByte)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_zxv_screen
    end asm
end sub

REM SCROLL x, y: the pixel (x, y) of the page is seen at the top left
REM corner of the screen; SCROLL alone puts it back
sub SCROLL(x as Integer = 0, y as Integer = 0)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_zxv_scroll
    end asm
end sub

REM REMAP c, n: colour c is seen as n, at once and on all the screen
sub REMAP(c as UByte, n as UByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_zxv_remap
    end asm
end sub

REM REMAPRESET: every colour as it was (REMAP with no arguments)
sub REMAPRESET
    asm
    li a0, -1
    li a1, 0
    zxb_ccall zxb_zxv_remap
    end asm
end sub

REM WAIT [n]: until the next frame, and until n frames have gone since the
REM last WAIT: a steady pace for games
sub WAIT(n as UInteger = 1)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_zxv_wait
    end asm
end sub

REM GRAB n, x, y, w, h[, t]: sprite n (0-255) is that rectangle of the
REM drawing page; colour t becomes transparent
sub GRAB(n as UByte, x as Integer, y as Integer, w as UInteger, h as UInteger, t as Integer = -1)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    lw a5, 28(s0)
    zxb_ccall zxb_zxv_grab
    end asm
end sub

REM GRABSTR n, w, a$: sprite n from a string, a character a pixel and w
REM pixels a row: the hex digits are the colours, anything else (".", " ")
REM is transparent
sub GRABSTR(n as UByte, w as UInteger, ByVal a$ as String)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_zxv_grab_str
    end asm
end sub

REM SPRITE n, x, y[, f[, c$[, w, h]]]: draws sprite n with its top left
REM corner at (x, y). f: 1 mirrored left to right, 2 upside down, 3 both.
REM c$ changes its colours: colour i is drawn as the hex digit i of c$ (any
REM other character leaves it). w, h: stretched or shrunk to that size.
sub SPRITE(n as UByte, x as Integer, y as Integer, f as UByte = 0, ByVal c$ as String = "", _
           w as UInteger = 0, h as UInteger = 0)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    lw a5, 28(s0)
    lw a6, 32(s0)
    zxb_ccall zxb_zxv_sprite
    end asm
end sub

REM HIT(a, ax, ay, b, bx, by): 1 if a solid pixel of sprite a at (ax, ay)
REM falls on a solid pixel of sprite b at (bx, by); it draws nothing
function HIT(a as UByte, ax as Integer, ay as Integer, b as UByte, bx as Integer, by as Integer) as UByte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    lw a5, 28(s0)
    zxb_ccall zxb_zxv_hit
    end asm
end function

REM BOX c, x, y, w, h: a filled rectangle of colour c
sub BOX(c as UByte, x as Integer, y as Integer, w as Integer, h as Integer)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    zxb_ccall zxb_zxv_box
    end asm
end sub

REM DISC c, x, y, r: a filled circle
sub DISC(c as UByte, x as Integer, y as Integer, r as Integer)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_zxv_disc
    end asm
end sub

REM POLY c, x1, y1[, x2, y2...]: a point, a line or a filled polygon (up to
REM 8 points)
sub POLY(c as UByte, x1 as Integer, y1 as Integer, x2 as Integer = -32768, y2 as Integer = 0, _
         x3 as Integer = -32768, y3 as Integer = 0, x4 as Integer = -32768, y4 as Integer = 0, _
         x5 as Integer = -32768, y5 as Integer = 0, x6 as Integer = -32768, y6 as Integer = 0, _
         x7 as Integer = -32768, y7 as Integer = 0, x8 as Integer = -32768, y8 as Integer = 0)
    asm
    lw a0, 8(s0)
    addi a1, s0, 12
    zxb_ccall zxb_zxv_poly
    end asm
end sub

REM CLIP x, y, w, h: the fast graphics only draw in that rectangle; CLIP
REM alone, everywhere
sub CLIP(x as Integer = 0, y as Integer = 0, w as Integer = -1, h as Integer = 0)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_zxv_clip
    end asm
end sub

REM PIXEL(x, y): the colour (0-15) of a pixel of the drawing page, whoever
REM drew it; -1 out of the page
function PIXEL(x as Integer, y as Integer) as Byte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_zxv_pixel
    end asm
end function

REM KEY(n): 1 if the key is down now (several at once). n: CODE "a" to
REM CODE "z", CODE "0" to CODE "9", 8 to 11 the cursor keys (left, right,
REM down, up), 12 Delete, 13 Enter, 32 Space, or 256 + a USB key code
function KEY(n as UInteger) as UByte
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_zxv_key
    end asm
end function

REM JOY(n): the buttons of joystick n (0 or 1); 0 if it is not connected.
REM 1 right, 2 left, 4 down, 8 up, 16 fire 1, 32 fire 2, 64 fire 3,
REM 128 start, 256 select
function JOY(n as UByte) as UInteger
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_zxv_joy
    end asm
end function

REM SOUND r, v: writes v in register r of the AY chips (0-15 the first,
REM 16-31 the second)
sub SOUND(r as UByte, v as UByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_zxv_sound
    end asm
end sub

REM TIME: the frames (50 a second) since the machine started
function TIME as ULong
    asm
    zxb_ccall zxb_zxv_time
    end asm
end function

#pragma pop(case_insensitive)

#endif
