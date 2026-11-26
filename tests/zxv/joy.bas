REM joy.bas: four joysticks (ZX-V D122): JOY(0) to JOY(3), with fire 4, L and R
REM (D118), and the players 2 and 3 of zxvinput.bas with their joystick
#include <zxv.bas>
#include <zxvinput.bas>

DIM n as UByte
FOR n = 1 TO 50
    WAIT
NEXT n
PRINT JOY(0); " "; JOY(1); " "; JOY(2); " "; JOY(3)
CTRLUPDATE 2
CTRLUPDATE 3
PRINT CTRLHELD(2); " "; CTRLHELD(3)
