' Banco "uno" (zona 0): llamadas con parámetros, recursión dentro del banco,
' un Float, una cadena, un FASTCALL en ensamblador y una llamada a otra zona.
' Declaradas en el fichero del banco, pero residentes: las ve todo el
' programa y no vuelven a empezar cuando el banco se recarga.
DIM marcador AS INTEGER = 100
DIM ultimo AS STRING

SUB apunta(n AS INTEGER)
    marcador = marcador + n
    ultimo = "uno"
END SUB

FUNCTION suma(a AS INTEGER, b AS INTEGER) AS INTEGER
    RETURN a + b
END FUNCTION

FUNCTION factorial(n AS ULONG) AS ULONG
    IF n <= 1 THEN RETURN 1
    RETURN n * factorial(n - 1)
END FUNCTION

FUNCTION mitad(x AS FLOAT) AS FLOAT
    RETURN x / 2
END FUNCTION

FUNCTION saludo(n AS STRING) AS STRING
    RETURN "hola " + n
END FUNCTION

FUNCTION FASTCALL doble(x AS ULONG) AS ULONG
    ASM
    slli a0, a0, 1
    END ASM
END FUNCTION

SUB llamaTres()
    PRINT "tres: "; triple(5)
END SUB
