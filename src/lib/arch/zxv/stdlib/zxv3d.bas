' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxv3d.bas: simple 3D (ZX-V SDK zxv_3d.h, D111): models of flat faces
' with light and pattern shading, like the 3D games of the 8 and 16 bits.
' obj2c.py --bas (in the SDK) turns a .obj of Blender into BASIC.
'
'   #include "house.bas"                          ' obj2c.py house.obj house.bas house --bas
'   house_model(0)
'   CAMERA3D -3, 2, -7, 3640, -1820
'   FOR a = 0 TO 65535 STEP 256
'       BOX 5, 0, 0, 256, 80: BOX 4, 0, 80, 256, 112
'       BEGIN3D: LET n = ADD3D(0, 0, 1, 0, a): DRAW3D: WAIT
'   NEXT a
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXV3D__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXV3D__

#pragma push(case_insensitive)
#pragma case_insensitive = True

#include <zxv.bas>

REM The options of a face (MAT3D), as in obj2c.py: seen from both sides,
REM always in the dark colour (no light), stuck on top of another face
CONST TWOSIDED3D as UInteger = 256
CONST FLAT3D as UInteger = 512
CONST DECAL3D as UInteger = 1024

REM MAT3D(dark, light[, flags]): the material of a face, for a model made by
REM hand (obj2c.py writes them already done)
function MAT3D(dark as UByte, light as UByte, flags as UInteger = 0) as UInteger
    return (dark band 15) bor ((light band 15) << 4) bor flags
end function

REM MODEL3D n, verts, nverts, faces, nfaces, scale: model n (0-15) from
REM its arrays (@NAME_v(0), @NAME_f(0): obj2c.py --bas writes them and a
REM SUB NAME_model(n) that calls this). They must stay where they are
REM (global arrays): they are not copied. scale: what a unit of the model
REM measures, as a raw 16.16 number
sub MODEL3D(n as UByte, verts as ULong, nverts as UInteger, faces as ULong, nfaces as UInteger, scale as ULong)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    lw a5, 28(s0)
    zxb_ccall zxb_model3d
    end asm
end sub

REM CAMERA3D x, y, z[, yaw, pitch, roll[, focal[, cx, cy, w, h]]]: where the
REM camera is (x right, y up, z forwards) and where it looks (angles in
REM turns of 65536, as TSIN: yaw turns right, pitch looks up, roll leans
REM right). focal in pixels (0: half the view, about 90 degrees across);
REM the view, centred at (cx, cy) and w x h (by default, the whole page)
sub CAMERA3D(x as Fixed, y as Fixed, z as Fixed, yaw as Long = 0, pitch as Long = 0, roll as Long = 0, focal as Integer = 0, cx as Integer = -1, cy as Integer = -1, w as UInteger = 0, h as UInteger = 0)
    asm
    addi a0, s0, 8
    zxb_ccall zxb_camera3d
    end asm
end sub

REM LIGHT3D x, y, z[, ambient]: where the light comes from (a vector towards
REM it) and the tone of the faces it does not reach, 0 (the dark colour)
REM to 4 (the light one). By default, from above, in front and left, 1
sub LIGHT3D(x as Fixed, y as Fixed, z as Fixed, ambient as UByte = 1)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_light3d
    end asm
end sub

REM WIREFRAME3D colour: only the edges, in that colour; -1, faces again
sub WIREFRAME3D(colour as Integer)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_wireframe3d
    end asm
end sub

REM BEGIN3D: starts a frame with the camera of CAMERA3D
sub BEGIN3D()
    asm
    zxb_ccall zxb_begin3d
    end asm
end sub

REM ADD3D(m, x, y, z[, yaw, pitch, roll[, scale]]): model m at (x, y, z),
REM turned and scaled: the faces of it that are seen, or -1 if they do
REM not fit (400 in all a frame)
function ADD3D(m as UByte, x as Fixed, y as Fixed, z as Fixed, yaw as Long = 0, pitch as Long = 0, roll as Long = 0, scale as Fixed = 1) as Integer
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    lw a4, 24(s0)
    lw a5, 28(s0)
    lw a6, 32(s0)
    lw a7, 36(s0)
    zxb_ccall zxb_add3d
    end asm
end function

REM DRAW3D: draws all that was added, from back to front, on the drawing
REM page (with CLIP; the shading uses the patterns)
sub DRAW3D()
    asm
    zxb_ccall zxb_draw3d
    end asm
end sub

REM PROJECT3D(x, y, z): 1 if the point of the world is in front of the
REM camera, and then PROJ3DX, PROJ3DY (the pixel) and PROJ3DSCALE (the
REM pixels a unit measures there): to put sprites in the 3D world
function PROJECT3D(x as Fixed, y as Fixed, z as Fixed) as UByte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_project3d
    end asm
end function

function PROJ3DX() as Integer
    asm
    zxb_ccall zxb_proj3d_x
    end asm
end function

function PROJ3DY() as Integer
    asm
    zxb_ccall zxb_proj3d_y
    end asm
end function

function PROJ3DSCALE() as Fixed
    asm
    zxb_ccall zxb_proj3d_scale
    end asm
end function

#pragma pop(case_insensitive)

#endif
