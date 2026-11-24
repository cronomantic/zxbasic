REM draw3dpat.bas: DRAW3D does not use the PATTERN of the program (its
REM shading is its own patterns), and the PATTERN is there again after it
#include <zxv3d.bas>
#include "sdk3d.inc/cubo.bas"

DIM n AS Integer
cubo_model(0)
CAMERA3D 0, 0, -5
LIGHT3D 0, 0, -1, 0
BOX 0, 0, 0, 256, 192
PATTERN 1, 6
REM The front face, all lit: plain bright red (10), not with the chessboard
BEGIN3D: LET n = ADD3D(0, 0, 0, 0): DRAW3D
PRINT PIXEL(128, 96); " "; PIXEL(129, 96); " "; PIXEL(128, 97)
REM The pattern again: blue and yellow
BOX 1, 16, 160, 16, 16
PRINT PIXEL(16, 160); " "; PIXEL(17, 160)
REM Turned, with the light by default: plain, shaded and plain faces
LIGHT3D 1, 2, -1, 1
CLIP 160, 0, 96, 192
BEGIN3D: LET n = ADD3D(0, 2, 0, 1, 8192, 5461): DRAW3D
CLIP
PATTERN
