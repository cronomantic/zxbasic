' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxvm7.bas: things on the mode 7 floor (ZX-V SDK zxv_m7.h, D111): where a
' point of the floor of the last FLOOR is seen and how big, to draw cars,
' trees or cones on it with SPRITE scaled.
'
'   FLOORMAP 0, cx, cy, 10, a, 95
'   IF FLOORPROJECT(tx, ty) THEN
'       LET s = 32 * FLOORSCALE()                    ' the tree is 32 texels high
'       SPRITE 1, FLOORX() - s / 4, FLOORY() - s, 0, "", s / 2, s
'   END IF
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXVM7__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXVM7__

#pragma push(case_insensitive)
#pragma case_insensitive = True

#include <zxv.bas>

REM FLOORPROJECT(x, y[, h]): where the point (x, y) of the floor (or at height
REM h over it) is seen with the camera of the last FLOOR or FLOORMAP: 1 if it
REM is in front of it, and then FLOORX, FLOORY (the pixel), FLOORSCALE
REM (pixels per texel there) and FLOORDEPTH (how far: draw from far to near)
function FLOORPROJECT(x as Fixed, y as Fixed, h as Fixed = 0) as UByte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_floor_project
    end asm
end function

function FLOORX() as Integer
    asm
    zxb_ccall zxb_floor_x
    end asm
end function

function FLOORY() as Integer
    asm
    zxb_ccall zxb_floor_y
    end asm
end function

function FLOORSCALE() as Fixed
    asm
    zxb_ccall zxb_floor_scale
    end asm
end function

function FLOORDEPTH() as Fixed
    asm
    zxb_ccall zxb_floor_depth
    end asm
end function

REM FLOORAT(sx, sy): the point of the floor seen at pixel (sx, sy) (the texel
REM the engine draws there): 1 and FLOORATX, FLOORATY; 0 at the horizon or
REM above it
function FLOORAT(sx as Integer, sy as Integer) as UByte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_floor_at
    end asm
end function

function FLOORATX() as Fixed
    asm
    zxb_ccall zxb_floor_at_x
    end asm
end function

function FLOORATY() as Fixed
    asm
    zxb_ccall zxb_floor_at_y
    end asm
end function

#pragma pop(case_insensitive)

#endif
