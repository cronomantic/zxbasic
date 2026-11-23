REM mapas.bas: TILE, TILESTR, MAPDIM, MAPPOKE, MAP, CELL, TEXTURE, ROTATE, FLOORMAP and
REM ROTATEMAP (zxb_maps.c), with the same cases as rom/tests/basic/cases/mode7.bas of
REM ZX-V: their pixels, worked out by hand. Drawn from row 96 on, away from PRINT
#include <zxv.bas>

PAPER 0: CLS
REM Texture 0: 4 x 4, top left 2, bottom right 5 (transparent), the rest 1
BOX 1, 0, 0, 4, 4: BOX 2, 0, 0, 2, 2: BOX 5, 2, 2, 2, 2: TEXTURE 0, 0, 0, 4, -1, 5
CLS: BOX 3, 96, 96, 16, 16: CLIP 96, 96, 16, 16
ROTATE 0, 0, 0, 0, 1, 100, 100
PRINT PIXEL(100, 100); " "; PIXEL(102, 100); " "; PIXEL(102, 102); " "; PIXEL(104, 104); " "; PIXEL(99, 99)
BOX 3, 96, 96, 16, 16: ROTATE 0, 0, 0, PI / 2, 1, 100, 100
PRINT PIXEL(101, 100); " "; PIXEL(100, 101); " "; PIXEL(100, 103); " "; PIXEL(98, 98)
BOX 3, 96, 96, 16, 16: ROTATE 0, 0, 0, 0, 2, 100, 100
PRINT PIXEL(100, 100); " "; PIXEL(103, 100); " "; PIXEL(104, 100); " "; PIXEL(104, 104)
CLIP
REM Map 1: 2 x 1 cells of 8, cell 0 tile 1 (all 4), cell 1 tile 2 (transparent); it does not repeat
CLS: BOX 4, 0, 0, 8, 8: TILE 8, 1, 0, 0
TILESTR 8, 2, "................................................................"
MAPDIM 1, 2, 1: MAPPOKE 1, 0, 0, 1: MAPPOKE 1, 1, 0, 2 + 16384
CLS: BOX 3, 110, 110, 40, 40: CLIP 110, 110, 40, 40: ROTATEMAP 1, 0, 0, 0, 1, 120, 120: CLIP
PRINT PIXEL(120, 120); " "; PIXEL(127, 127); " "; PIXEL(128, 120); " "; PIXEL(119, 120); " "; PIXEL(120, 128)
REM MAP and CELL
BOX 3, 40, 150, 32, 8: MAP 1, 0, 0, 40, 150, 32, 8
PRINT PIXEL(40, 150); " "; PIXEL(48, 150); " "; PIXEL(56, 150); " "; CELL(1, 1, 0); " "; CELL(1, 5, 0)
REM Map 2 repeats: all the floor is tile 1; above the horizon nothing changes
MAPDIM 2, 1, 1, 8, 1: MAPPOKE 2, 0, 0, 1: BOX 3, 0, 96, 256, 96
FLOORMAP 2, 0, 0, 10, 0.3, 120
PRINT PIXEL(10, 190); " "; PIXEL(200, 121); " "; PIXEL(10, 120); " "; PIXEL(10, 100)
REM A strip of tiles cut from the page: tiles 10 and 11
BOX 6, 0, 0, 8, 8: BOX 7, 8, 0, 8, 8: TILE 8, 10, 0, 0, 16, 8
MAPDIM 3, 2, 1: MAPPOKESTR 3, 0, 0, CHR$(10) + CHR$(11)
MAP 3, 0, 0, 200, 150, 16, 8
PRINT PIXEL(200, 150); " "; PIXEL(208, 150)
