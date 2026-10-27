REM Functions and subs: parameters by value and by reference, locals, recursion
FUNCTION fact(n AS UBYTE) AS ULONG
    IF n <= 1 THEN RETURN 1
    RETURN n * fact(n - 1)
END FUNCTION

FUNCTION fib(n AS INTEGER) AS INTEGER
    IF n < 2 THEN RETURN n
    RETURN fib(n - 1) + fib(n - 2)
END FUNCTION

FUNCTION media(a AS FLOAT, b AS FLOAT) AS FLOAT
    DIM m AS FLOAT
    m = (a + b) / 2
    RETURN m
END FUNCTION

FUNCTION saludo(nombre AS STRING, n AS UBYTE) AS STRING
    DIM r AS STRING
    DIM k AS UBYTE
    FOR k = 1 TO n
        r = r + "hola "
    NEXT k
    RETURN r + nombre
END FUNCTION

SUB doble(BYREF v AS INTEGER)
    v = v * 2
END SUB

SUB cambia(BYREF t AS STRING)
    t = t + "!"
END SUB

SUB mezcla(a AS UBYTE, b AS LONG, c AS FIXED, d AS BYTE)
    DIM loc1 AS UBYTE = 7
    DIM loc2 AS LONG = 100000
    DIM loc3 AS INTEGER
    loc3 = a + d
    PRINT a; " "; b; " "; c; " "; d; " "; loc1; " "; loc2; " "; loc3
END SUB

DIM v AS INTEGER = 21
DIM t AS STRING = "eh"
PRINT fact(10); " "; fact(12)
PRINT fib(15)
PRINT media(1, 2); " "; media(-3.5, 10)
PRINT saludo("mundo", 2)
doble(v)
doble(v)
PRINT v
cambia(t)
cambia(t)
PRINT t
mezcla(200, -123456, 2.5, -7)
PRINT LEN(saludo("x", 3))
