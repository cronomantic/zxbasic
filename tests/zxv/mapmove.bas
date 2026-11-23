REM mapmove.bas: zxvmapmove.bas (ZX-V SDK zxv_mapmove.h, D111) on a map of 4 x 3 cells
REM of 8 pixels: a solid floor in row 2, a platform at (2, 1) and a coin at (1, 1)
#include <zxvmapmove.bas>

TILEKIND 8, 1, TILESOLID: TILEKIND 8, 2, TILEPLATFORM: TILEKIND 8, 3, 3
MAPDIM 0, 4, 3
FOR c = 0 TO 3: MAPPOKE 0, c, 2, 1: NEXT c
MAPPOKE 0, 2, 1, 2: MAPPOKE 0, 1, 1, 3
DIM px, py as Fixed
REM It falls 20 pixels from (0, 0): onto the floor, at y 8
LET px = 0: LET py = 0
PRINT MAPMOVE(0, px, py, 8, 8, 0, 20); " "; px; " "; py; " "; MAPONGROUND(0, px, py, 8, 8)
REM 30 to the right: the edge of the map is a wall, at x 24
PRINT MAPMOVE(0, px, py, 8, 8, 30, 0); " "; px
REM The coin, at cell (1, 1)
PRINT MAPFIND(0, 8, 8, 8, 8, 3); " "; MAPFOUNDX(); " "; MAPFOUNDY(); " "; MAPFIND(0, 24, 8, 8, 8, 3)
REM Onto the platform from above, and through it with MOVETHROUGH
LET px = 16: LET py = -10
PRINT MAPMOVE(0, px, py, 8, 8, 0, 20); " "; py
PRINT MAPMOVE(0, px, py, 8, 8, 0, 20, MOVETHROUGH); " "; py
PRINT MAPKIND(0, 20, 12); " "; MAPKIND(0, 5, 20); " "; MAPKIND(0, -1, 0)
REM Out of the map, empty: it goes on
MAPOUTSIDE 0, TILEEMPTY
LET px = 24
PRINT MAPMOVE(0, px, py, 8, 8, 30, 0); " "; px
