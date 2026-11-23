REM sdkinput.bas: zxvinput.bas (ZX-V SDK zxv_input.h, D111). The program starts with
REM Enter held (the one typed to load it): START, until it is released. Then the
REM names of the keys and a second player
#include <zxv.bas>
#include <zxvinput.bas>

CTRLUPDATE
PRINT CTRLHELD(); " "; CTRLPRESSED()
DIM n as UInteger = 0
DO
    WAIT
    CTRLUPDATE
    LET n = n + 1
LOOP UNTIL CTRLHELD() = 0 OR n > 200
PRINT CTRLHELD(); " "; CTRLRELEASED(); " "; CTRLFRAMES(0, CTRLSTART)
PRINT KEYNAME(KEYSPACE); ","; KEYNAME(KEYQ); ","; KEYNAME(KEYUPKEY); ","; KEYNAME(51)
PRINT NEWKEY(); " "; NEWKEY()
PRINT CTRLMAPSIZE()
CTRLKEY 1, CTRLFIRE, 0, KEYSPACE
CTRLPAD 1, -1
CTRLUPDATE 1
PRINT CTRLHELD(1); " "; CTRLREPEAT(0, CTRLDOWN)
