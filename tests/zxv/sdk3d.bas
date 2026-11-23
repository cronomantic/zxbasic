REM sdk3d.bas: zxv3d.bas (ZX-V SDK zxv_3d.h, D111), with a cube made by
REM obj2c.py --bas (sdk3d.inc/cubo.bas: the front face, zxv_2_10, red)
#include <zxv3d.bas>
#include "sdk3d.inc/cubo.bas"

cubo_model(0)
CAMERA3D 0, 0, -5
BEGIN3D: PRINT ADD3D(0, 0, 0, 0);
BEGIN3D: PRINT " "; ADD3D(0, 0, 0, 0, 8192);
BEGIN3D: PRINT " "; ADD3D(0, 0, 0, 0, 8192, 5461)

PRINT PROJECT3D(0, 0, 0); " "; PROJ3DX(); " "; PROJ3DY(); " "; PROJ3DSCALE(); " "; PROJECT3D(0, 0, -6)

SUB scene(scale AS Fixed)
    BOX 0, 0, 0, 256, 192
    BEGIN3D
    LET n = ADD3D(0, 0, 0, 0, 0, 0, 0, scale)
    DRAW3D
END SUB

LIGHT3D 0, 0, -1, 0
scene(1)
PRINT PIXEL(128, 96); " "; PIXEL(178, 96);
LIGHT3D 0, 0, 1, 0
scene(1)
PRINT " "; PIXEL(128, 96);
scene(2)
PRINT " "; PIXEL(178, 96)

WIREFRAME3D 7
scene(1)
DIM edge AS UByte = 0
FOR x = 150 TO 170
    IF PIXEL(x, 96) = 7 THEN LET edge = 1
NEXT x
PRINT PIXEL(128, 96); " "; edge
WIREFRAME3D -1
REM To look at: the cube turned, with the light by default
BOX 5, 0, 0, 256, 192: LIGHT3D 1, 2, -1, 1
BEGIN3D: LET n = ADD3D(0, 0, 0, 0, 8192, 5461): DRAW3D
