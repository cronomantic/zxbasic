REM zxv.bas: sprites, shapes, pages and the rest of what is only in ZX-V
#include <zxv.bas>

PAPER 7: INK 0: CLS
GRABSTR 0, 4, "1111122112211111"
SPRITE 0, 10, 10
SPRITE 0, 30, 10, 0, "0a"
SPRITE 0, 50, 10, 0, "", 32, 32
BOX 4, 100, 20, 20, 20
DISC 2, 200, 100, 20
POLY 1, 10, 150, 60, 150, 35, 180
POLY 0, 90, 120, 140, 120
PRINT AT 20, 0; PIXEL(110, 30); " "; PIXEL(5, 5); " "; PIXEL(-1, 0); " "; PIXEL(31, 11)
PRINT HIT(0, 0, 0, 0, 3, 3); HIT(0, 0, 0, 0, 4, 0)
PRINT KEY(32); " "; JOY(0); " "; TIME > 0

REM The hidden page, then the shown one
SCREEN 1
BOX 5, 0, 0, 256, 192
PRINT AT 0, 0; PIXEL(1, 1)
GRAB 1, 100, 20, 8, 8
SCREEN 0
SPRITE 1, 140, 20, 1
CLIP 0, 0, 230, 192
DISC 3, 240, 160, 20
CLIP
PRINT PIXEL(144, 24); " "; PIXEL(225, 160); " "; PIXEL(235, 160)
WAIT
WAIT 2
