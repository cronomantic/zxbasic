REM sdkfont.bas: zxvfont.bas (ZX-V SDK zxv_font.h, D111): the font of the ROM at
REM double size, its measures, and a font grabbed from the page in its colours
#include <zxv.bas>
#include <zxvfont.bas>

PAPER 0: CLS
FONTROM 0, 1
PRINT FONTDRAW(0, 100, 100, "A", 5, 2)
PRINT PIXEL(104, 102); " "; PIXEL(100, 100); " "; PIXEL(102, 108); " "; PIXEL(100, 108)
PRINT FONTWIDTH(0, "AB", 2); " "; FONTHEIGHT(0, "a" + CHR$(10) + "b", 3)
FONTROM 1
PRINT FONTWIDTH(1, "ii") < FONTWIDTH(1, "mm")
REM Two cells of 8 x 8: A, all of colour 3; B, the left half of colour 6
BOX 3, 0, 160, 8, 8
BOX 6, 8, 160, 4, 8
FONTGRAB 2, 0, 160, 16, 8, 8, 8, 65, 0
PRINT FONTDRAW(2, 120, 140, "AB", FONTOWN)
PRINT PIXEL(120, 140); " "; PIXEL(129, 140); " "; PIXEL(133, 140)
