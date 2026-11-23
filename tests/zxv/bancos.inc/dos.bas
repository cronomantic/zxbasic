' Banco "dos" (zona 0, la misma que "uno"): se carga encima de él.
FUNCTION resta(a AS INTEGER, b AS INTEGER) AS INTEGER
    RETURN a - b
END FUNCTION

SUB cuenta()
    contador = contador + 1
END SUB

' Un array local, con sus límites (en la parte residente).
FUNCTION lista() AS INTEGER
    DIM v(4) AS INTEGER => {1, 2, 3, 4, 5}
    DIM t AS INTEGER = 0
    DIM i AS UBYTE
    FOR i = 0 TO 4
        t = t + v(i)
    NEXT i
    RETURN t * 2
END FUNCTION
