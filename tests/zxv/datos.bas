REM READ, DATA and RESTORE
DIM a AS UBYTE
DIM b AS INTEGER
DIM c AS FLOAT
DIM d AS STRING
DIM e AS FIXED
DIM i AS UBYTE
READ a, b, c, d, e
PRINT a; " "; b; " "; c; " "; d; " "; e
FOR i = 1 TO 3
    READ b
    PRINT b; " ";
NEXT i
PRINT
RESTORE 100
READ c, d
PRINT c; " "; d
READ c, d
PRINT c; " "; d
DATA 7, -300, 123456.5, "texto", 1.25
DATA 1, 2, 3
100 DATA 0.75, "primero"
DATA 99, "ultimo" + "!"
