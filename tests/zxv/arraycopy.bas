REM a = b with arrays: numbers, floats, strings, global and local
DIM a(3) AS INTEGER
DIM b(3) AS INTEGER => {1, 2, 3, 4}
DIM f(1, 2) AS FLOAT => {{1.5, 2.5, 3.5}, {4.5, 5.5, 6.5}}
DIM g(1, 2) AS FLOAT
DIM s$(2)
DIM t$(2)

t$(0) = "uno": t$(1) = "dos": t$(2) = "tres"
a = b
g = f
s$ = t$
t$(1) = "otro"
PRINT a(0); " "; a(3); " "; g(0, 0); " "; g(1, 2)
PRINT s$(0); " "; s$(1); " "; s$(2); " "; t$(1)

SUB copies()
    DIM la(3) AS INTEGER
    DIM ls$(2)
    la = b
    ls$ = s$
    la(0) = 9
    PRINT la(0); " "; la(3); " "; b(0); " "; ls$(2)
    b = la
END SUB

copies()
PRINT b(0)
