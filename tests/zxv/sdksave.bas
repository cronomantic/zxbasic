REM sdksave.bas: zxvsave.bas (ZX-V SDK zxv_save.h, D111): saving and loading, the
REM versions, a file that is not there, and the table of scores
#include <zxvsave.bas>

DIM a(3) as UByte => {1, 2, 3, 4}
DIM b(3) as UByte
PRINT SAVEDATA("prueba.sav", 1, @a(0), 4)
PRINT LOADDATA("prueba.sav", 1, @b(0), 4); " "; b(0); " "; b(1); " "; b(2); " "; b(3)
LET b(0) = 9
PRINT LOADDATA("prueba.sav", 2, @b(0), 4); " "; b(0)
PRINT LOADDATA("no.sav", 1, @b(0), 4)
PRINT CRC32(@a(0), 4)
SCORECLEAR 3
PRINT SCOREADD(500, "ANA"); " "; SCOREADD(900, "BEATRIZLARGO"); " "; SCOREADD(500, "CRIS"); " "; SCOREADD(100, "DANI")
PRINT SCORE(0); " "; SCORENAME(0); " "; SCORE(2); " "; SCORENAME(2)
PRINT SCORESAVE("tabla.sav")
SCORECLEAR 3
PRINT SCORE(0)
PRINT SCORELOAD("tabla.sav"); " "; SCORE(0); " "; SCORENAME(1)
