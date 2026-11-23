REM sdkfade.bas: zxvfade.bas (ZX-V SDK zxv_fade.h, D111): the palette registers
REM (ZXV_PALETTES, 536872064) while it fades
#include <zxvfade.bas>

CONST PAL as ULong = 536872064
PRINT PEEK(UInteger, PAL + 4)
FADECOLOUR RGB565(0, 0, 0), 10
FOR i = 1 TO 5: FADESTEP: NEXT i
PRINT PEEK(UInteger, PAL + 4); " "; FADEBUSY()
FADEWAIT
PRINT PEEK(UInteger, PAL + 4); " "; FADEBUSY()
DIM pal(15) as UInteger
FOR i = 0 TO 15: LET pal(i) = RGB565(255, 255, 255): NEXT i
FADETO @pal(0), 2
FADEWAIT
PRINT PEEK(UInteger, PAL + 4); " "; PEEK(UInteger, PAL + 36)
FADETO 0, 0, 1
PRINT PEEK(UInteger, PAL + 4); " "; PEEK(UInteger, PAL + 36)
PRINT RGBMIX(0, 65535, 1, 2)
FADETO 0, 0
