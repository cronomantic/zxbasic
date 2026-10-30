REM --enable-break: the functions whose body is an ASM block keep their result
REM (a0, a1) after the BREAK check of their last line
#include <zxv.bas>

function big as ULong
    asm
    li a0, 0x12345678
    end asm
end function

function half as Float
    asm
    li a0, 0
    li a1, 0x3FF80000
    end asm
end function

DIM t AS ULONG
PRINT KEY(32); " "; JOY(0); " "; PIXEL(-1, 0); " "; MUSIC; " "; SFXPLAYING
PRINT big; " "; half; " "; half = 1.5
WAIT
t = TIME
WAIT 10
PRINT TIME - t; " "; t > 60
