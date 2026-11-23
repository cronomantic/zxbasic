REM floorproj.bas: zxvm7.bas (ZX-V SDK zxv_m7.h, D111) with the camera of a FLOOR at the
REM origin, 10 high, looking at x, the horizon at row 50 and the focal length 128
#include <zxvm7.bas>

PAPER 0: CLS
BOX 4, 0, 0, 4, 4: TEXTURE 0, 0, 0, 4
FLOOR 0, 0, 0, 10, 0, 50
PRINT FLOORPROJECT(100, 0); " "; FLOORX(); " "; FLOORY(); " "; FLOORSCALE() > 1.27 AND FLOORSCALE() < 1.29
PRINT FLOORPROJECT(100, 0, 10); " "; FLOORY(); " "; FLOORPROJECT(-5, 0)
PRINT FLOORAT(128, 63); " "; INT(FLOORATX()); " "; INT(FLOORATY()); " "; FLOORAT(10, 40)
