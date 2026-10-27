REM Local arrays and array parameters
FUNCTION suma(BYREF v() AS INTEGER) AS LONG
    DIM i AS UBYTE
    DIM s AS LONG
    FOR i = LBOUND(v, 1) TO UBOUND(v, 1)
        s = s + v(i)
    NEXT i
    RETURN s
END FUNCTION

FUNCTION cuadrados(n AS UBYTE) AS LONG
    DIM t(1 TO 20) AS INTEGER
    DIM i AS UBYTE
    FOR i = 1 TO n
        t(i) = i * i
    NEXT i
    RETURN suma(t)
END FUNCTION

SUB nombres(p AS UBYTE)
    DIM n$(2)
    DIM m(2, 299) AS UBYTE
    DIM k(3) AS FLOAT => {1.5, 2.5, 3.5, 4.5}
    n$(0) = "uno": n$(1) = "dos": n$(2) = n$(0) + n$(1)
    m(1, 298) = 7: m(2, 1) = 9
    IF p THEN PRINT n$(2); " "; m(1, 298); " "; m(2, 1); " "; m(0, 298); " "; k(1) + k(3)
    IF p THEN PRINT LBOUND(n$, 0); " "; UBOUND(m, 2)
END SUB

PRINT cuadrados(3); " "; cuadrados(20)
DIM i AS UBYTE
FOR i = 1 TO 500
    nombres(i = 1)
NEXT i
PRINT "memoria ok"
