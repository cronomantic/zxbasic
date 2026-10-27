REM Arrays
DIM a(9) AS UBYTE
DIM m(2, 3) AS INTEGER
DIM f(4) AS FLOAT
DIM s$(3)
DIM t(2) AS UINTEGER => {100, 200, 300}
DIM w(1 TO 3, 2 TO 4) AS LONG
DIM i, j AS UBYTE
FOR i = 0 TO 9: a(i) = i * i: NEXT i
FOR i = 0 TO 9: PRINT a(i); " ";: NEXT i
PRINT
FOR i = 0 TO 2: FOR j = 0 TO 3: m(i, j) = i * 10 - j: NEXT j: NEXT i
PRINT m(0, 0); " "; m(1, 2); " "; m(2, 3); " "; m(2, 0)
FOR i = 0 TO 4: f(i) = i / 4: NEXT i
PRINT f(1) + f(3); " "; f(4) * 10
s$(0) = "cero": s$(1) = "uno": s$(3) = s$(0) + s$(1)
PRINT s$(0); " "; s$(1); " ["; s$(2); "] "; s$(3); " "; LEN(s$(3))
PRINT t(0) + t(1) + t(2)
FOR i = 1 TO 3: FOR j = 2 TO 4: w(i, j) = i * 1000 + j: NEXT j: NEXT i
PRINT w(1, 2); " "; w(2, 3); " "; w(3, 4)
PRINT LBOUND(w, 1); " "; UBOUND(w, 1); " "; LBOUND(w, 2); " "; UBOUND(w, 2)
