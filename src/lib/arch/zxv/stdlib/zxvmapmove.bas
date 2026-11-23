' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxvmapmove.bas: moving things through a tile map without going through
' the walls (ZX-V SDK zxv_mapmove.h, D111): the core of a platform game.
' The positions are Fixed, in pixels of the map.
'
'   TILEKIND 8, 1, TILESOLID: TILEKIND 8, 2, TILEPLATFORM
'   LET vy = vy + 0.25                              ' gravity
'   IF MAPMOVE(0, px, py, 12, 16, vx, vy) BAND HITDOWN THEN LET vy = 0
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXVMAPMOVE__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXVMAPMOVE__

#pragma push(case_insensitive)
#pragma case_insensitive = True

#include <zxv.bas>

REM The types of the tiles (3-255 are the game's: they stop nothing, MAPFIND finds them)
CONST TILEEMPTY as UByte = 0
CONST TILESOLID as UByte = 1
CONST TILEPLATFORM as UByte = 2

REM What MAPMOVE gives: the sides that hit
CONST HITLEFT as UByte = 1
CONST HITRIGHT as UByte = 2
CONST HITUP as UByte = 4
CONST HITDOWN as UByte = 8

REM MAPMOVE and MAPONGROUND: through the platforms (going down from one)
CONST MOVETHROUGH as UByte = 1

REM TILEKIND s, tile, kind: the type of a tile of the set of side s (8 or
REM 16): TILEEMPTY, TILESOLID (it stops on its four sides), TILEPLATFORM (it
REM only stops coming down from above) or one of the game (3-255). Until
REM the first TILEKIND of a set, tile 0 is empty and the rest solid
sub TILEKIND(s as UByte, tile as UInteger, kind as UByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_tile_kind
    end asm
end sub

REM MAPOUTSIDE m, kind: the type of what is out of map m when it does not
REM repeat (TILESOLID at the start: the edges are walls)
sub MAPOUTSIDE(m as UByte, kind as UByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_map_outside
    end asm
end sub

REM MAPMOVE(m, x, y, w, h, dx, dy[, flags]): moves the rectangle of w x h
REM pixels with its top left corner at (x, y) (Fixed variables, which
REM change) dx and then dy through map m, stopping at the first cell that
REM blocks each axis, touching it: it does not jump walls however fast it
REM goes. It gives the sides that hit (HITLEFT, HITRIGHT, HITUP, HITDOWN:
REM on the ground). With MOVETHROUGH it goes through the platforms
function MAPMOVE(m as UByte, ByRef x as Fixed, ByRef y as Fixed, w as UInteger, h as UInteger, dx as Fixed, dy as Fixed, flags as UByte = 0) as UByte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    lw a5, 28(s0)
    lw a6, 32(s0)
    lw a7, 36(s0)
    zxb_ccall zxb_map_move
    end asm
end function

REM MAPONGROUND(m, x, y, w, h[, flags]): 1 if it stands on something (solid,
REM or a platform without MOVETHROUGH)
function MAPONGROUND(m as UByte, x as Fixed, y as Fixed, w as UInteger, h as UInteger, flags as UByte = 0) as UByte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    lw a5, 28(s0)
    zxb_ccall zxb_map_on_ground
    end asm
end function

REM MAPFIND(m, x, y, w, h, kind): 1 if the rectangle touches a cell of that
REM type; MAPFOUNDX and MAPFOUNDY, the first one (to take a coin away)
function MAPFIND(m as UByte, x as Fixed, y as Fixed, w as UInteger, h as UInteger, kind as UByte) as UByte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    lw a5, 28(s0)
    zxb_ccall zxb_map_find
    end asm
end function

function MAPFOUNDX() as Integer
    asm
    zxb_ccall zxb_map_found_x
    end asm
end function

function MAPFOUNDY() as Integer
    asm
    zxb_ccall zxb_map_found_y
    end asm
end function

REM MAPKIND(m, px, py): the type of the cell at pixel (px, py) of map m
function MAPKIND(m as UByte, px as Integer, py as Integer) as UByte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_map_kind
    end asm
end function

#pragma pop(case_insensitive)

#endif
