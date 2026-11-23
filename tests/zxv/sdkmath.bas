REM sdkmath.bas: zxvmath.bas (ZX-V SDK zxv_math.h, D111)
#include <zxvmath.bas>

PRINT TSIN(16384); " "; TCOS(0); " "; TSIN(0); " "; TCOS(32768)
PRINT TATAN2(1, 0); " "; TATAN2(0, -5); " "; TATAN2(-3, 0); " "; TATAN2(0, 7)
PRINT ISQRT(10); " "; ISQRT(1000000); " "; HYPOT(3, -4); " "; HYPOT(-30000, 40000)
RNDSEED 1
PRINT RNDNEXT(); " "; RNDNEXT()
RNDSEED 1
PRINT RNDNEXT()
DIM bad as UByte = 0
FOR i = 1 TO 200
    LET r = RNDRANGE(-3, 3)
    IF r < -3 OR r > 3 THEN LET bad = 1
NEXT i
PRINT bad
