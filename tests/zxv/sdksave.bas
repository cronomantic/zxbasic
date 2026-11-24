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
REM A table made by hand is damaged (and the table does not change) if it
REM has not 1 to 10 places or a name has no 0 in its 8 bytes
DIM h(123) AS UByte
LET h(0) = 200
PRINT SAVEDATA("mano.sav", 1, @h(0), 124); " "; SCORELOAD("mano.sav"); " "; SCORE(0)
LET h(0) = 0
PRINT SAVEDATA("mano.sav", 1, @h(0), 124); " "; SCORELOAD("mano.sav"); " "; SCORE(0)
LET h(0) = 2: LET h(4) = 7
FOR i = 8 TO 15: LET h(i) = 65: NEXT i
PRINT SAVEDATA("mano.sav", 1, @h(0), 124); " "; SCORELOAD("mano.sav"); " "; SCORE(0)
LET h(15) = 0
FOR i = 116 TO 123: LET h(i) = 66: NEXT i
PRINT SAVEDATA("mano.sav", 1, @h(0), 124); " "; SCORELOAD("mano.sav"); " "; SCORE(0)
LET h(116) = 0
PRINT SAVEDATA("mano.sav", 1, @h(0), 124); " "; SCORELOAD("mano.sav"); " "; SCORE(0); " "; SCORENAME(0)
PRINT SCOREADD(950, "EVA"); " "; SCOREADD(1, "FE"); " "; SCORENAME(1)
