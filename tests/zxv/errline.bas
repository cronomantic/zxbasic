REM --enable-break: the error reports say the line
PRINT "uno"
DIM i AS UBYTE
FOR i = 1 TO 2
    PRINT i
NEXT i
GOSUB there
PRINT "no llega"
END
there:
    PRINT "en there"
    STOP
