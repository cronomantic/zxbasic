' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxv.bas: what is only in ZX-V, with the names and arguments of the ZX-V
' BASIC commands (its manual, "Modos de vídeo y efectos", "Sprites, mapas de
' tiles y gráficos rápidos", KEY, JOY, SOUND, MUSIC and SFX). The runtime
' (zxb_zxv.c) calls the engine of the ROM, and zxb_music.c the music library
' of the ZX-V SDK (also for SAMPLE).
'
' The fast graphics (SPRITE, BOX, DISC, POLY, PIXEL...) count pixels from the
' top left corner of the drawing page, with the y downwards; colours are
' 0-15 and 8 is transparent in sprites. PRINT, PLOT and CLS draw on the
' top left 256 x 192 pixels of the drawing page, in any mode.
'
' Not as in the ZX-V BASIC:
'  - The sprite of a string is GRABSTR n, w, a$ (GRAB n, w, a$ there).
'  - KEY, JOY and HIT are functions: KEY(9), not KEY 9.
'  - REMAP and SOUND take one pair at a time and PALETTE one colour, REMAP
'    and PALETTE only change table 0 (no #t), and REMAP alone is REMAPRESET
'    and PALETTE alone PALETTERESET.
'  - POLY takes up to 8 points.
'  - SCREEN can not be used with screen.bas (its SCREEN$ is also screen).
'  - The music and the effects: MUSICLOAD f$ and SFXLOAD f$ (LOAD f$ MUSIC
'    and SFX there), MUSICPLAY, MUSICSTOP, MUSICPAUSE and MUSICCONTINUE, and
'    the function SFXPLAYING (SFX alone there). MUSICLOAD does not take MOD,
'    S3M or XM modules: zxv-music turns them into a .zxm first.
'  - The recorded sounds: SAMPLELOAD f$, n (LOAD f$ SAMPLE n there), SAMPLE
'    n, tone, vol, pan, c (SAMPLE n, tone, vol, pan; c there), SAMPLESTOP
'    [c] and the function SAMPLEPLAYING (SAMPLE alone there).
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXV__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXV__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM MODE n[, w, h]: mode 0 (256 x 192, 32 x 24 characters) or 1 (512 x 384).
REM In mode 0 the page can be bigger than the screen (512 x 384, 1024 x 192
REM or 256 x 768), and SCROLL moves the screen over it. Both pages are
REM cleared, page 0 is shown and drawn on, and the scroll, REMAP, PALETTE
REM and CLIP go.
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

REM PALETTE c, r, g, b: colour c (0-15) is seen with that red, green and
REM blue (0-255 each; 215 is the level of the standard colours, 255 the
REM bright one), the nearest one ZX-V shows, at once and on all the screen
sub PALETTE(c as UByte, r as UInteger, g as UInteger, b as UInteger)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_zxv_palette
    end asm
end sub

REM PALETTERESET: the standard colours again (PALETTE with no arguments)
sub PALETTERESET
    asm
    li a0, -1
    zxb_ccall zxb_zxv_palette
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

REM TIME: the frames (60 a second) since the machine started
function TIME as ULong
    asm
    zxb_ccall zxb_zxv_time
    end asm
end function

REM MUSICLOAD f$: loads a module (without an extension, the first there is
REM of f$.zxm, f$.pt3, f$.vgm and f$.vgz): ZX-V's own .zxm (the two AY chips
REM and the sample channels: what zxv-music makes of a MOD, S3M or XM),
REM Pro Tracker 3 or Vortex Tracker II (.pt3, what zxv-tracker saves), a
REM TurboSound (two modules, one on each AY chip), Pro Tracker 2, Sound
REM Tracker, ASC Sound Master, a PSG dump or a VGM of AY chips (.vgm, or
REM compressed: .vgz, which takes 32 KB more to play). It goes in the heap.
sub MUSICLOAD(ByVal f$ as String)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_music_load
    end asm
end sub

REM MUSICPLAY: plays the module in the background while the program goes on,
REM and at its end goes back to its loop position, for ever. It plays on the
REM first AY chip (a TurboSound, on both), until MUSICSTOP or the end of the
REM program.
sub MUSICPLAY
    asm
    zxb_ccall zxb_music_play
    end asm
end sub

REM MUSICSTOP: stops the music
sub MUSICSTOP
    asm
    zxb_ccall zxb_music_stop
    end asm
end sub

REM MUSICPAUSE: silences the music; MUSICCONTINUE goes on where it was
sub MUSICPAUSE
    asm
    li a0, 1
    zxb_ccall zxb_music_pause
    end asm
end sub

sub MUSICCONTINUE
    asm
    li a0, 0
    zxb_ccall zxb_music_pause
    end asm
end sub

REM MUSIC: 1 while the music plays, 0 if not
function MUSIC as UByte
    asm
    zxb_ccall zxb_music_on
    end asm
end function

REM SFXLOAD f$: loads an AYFX bank of sound effects (without an extension,
REM f$.afb: what the effects tab of zxv-tracker or the AY Sound FX Editor
REM saves), or a single effect (.afx). It goes in the heap.
sub SFXLOAD(ByVal f$ as String)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_sfx_load
    end asm
end sub

REM SFX n[, c]: plays effect n of the bank (the first is 0) without waiting,
REM on a free channel of the chip the music does not use, or on channel c:
REM 0 to 2 are A, B and C of the first chip and 3 to 5 those of the second
sub SFX(n as UByte, c as Byte = -1)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_sfx
    end asm
end sub

REM SFXSTOP: stops all the effects
sub SFXSTOP
    asm
    zxb_ccall zxb_sfx_stop
    end asm
end sub

REM SFXPLAYING: 1 while any effect plays
function SFXPLAYING as UByte
    asm
    zxb_ccall zxb_sfx_on
    end asm
end function

REM SAMPLELOAD f$, n: loads a WAV (without an extension, f$.wav) as sound n
REM (0 to 31) in the sound memory: 8 or 16 bits, mono or stereo, at any
REM rate, and with its loop if it has one (a smpl chunk). Nothing stays in
REM the RAM.
sub SAMPLELOAD(ByVal f$ as String, n as UByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_sample_load
    end asm
end sub

REM SAMPLE n[, tone, vol, pan, c]: plays sound n without waiting, tone
REM semitones up (or down, negative: 12 is an octave), with its volume (0 to
REM 64) and pan (0 left, 128 centre, 255 right), on a sample channel the
REM music does not use, or on channel c (0 to 11). A sound with a loop goes
REM on until SAMPLESTOP.
sub SAMPLE(n as UByte, tone as Float = 0, vol as UByte = 64, pan as UByte = 128, c as Byte = -1)
    zxvSamplePlay(n, 65536 * 2 ^ (tone / 12) + 0.5, vol, pan, c)
end sub

sub zxvSamplePlay(n as UByte, mul as ULong, vol as UByte, pan as UByte, c as Byte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    zxb_ccall zxb_sample
    end asm
end sub

REM SAMPLESTOP [c]: stops the sound of channel c, or all of them
sub SAMPLESTOP(c as Byte = -1)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_sample_stop
    end asm
end sub

REM SAMPLEPLAYING: 1 while any sound of SAMPLE plays
function SAMPLEPLAYING as UByte
    asm
    zxb_ccall zxb_sample_on
    end asm
end function

#pragma pop(case_insensitive)

#endif
