REM --debug-array: an index out of its bounds gives 3 Subscript wrong;
REM inside them (also with lower bounds and in a local array) it works
DIM a(3) AS UBYTE
DIM b(2 TO 4, 1 TO 2) AS INTEGER
DIM i AS INTEGER
a(3) = 7
b(4, 2) = -5
PRINT a(3); " "; b(4, 2)
SUB local()
    DIM c(1 TO 2) AS UBYTE
    c(2) = 9
    PRINT c(2)
END SUB
local()
i = 4
PRINT "b(1,1)"
PRINT b(i - 3, 1)
PRINT "no llega"
