REM Strings
DIM a AS STRING = "ZX Spectrum"
DIM b AS STRING
DIM n AS UBYTE
b = a + " y ZX-V"
PRINT b; " "; LEN(b)
PRINT a(0 TO 1); "|"; a(3 TO); "|"; a(TO 4); "|"; a(5)
PRINT CHR$(72, 111, 108, 97); " "; CHR$(65); " "; CODE("Z"); " "; CODE("")
PRINT STR$(3.75); "|"; STR$(-12); "|"; VAL("  2.5E2"); " "; VAL("-7") + 1
DIM c AS STRING = "ab"
DIM d AS STRING = "abc"
PRINT c < "b"; " "; d = "abc"; " "; d <> "abd"; " "; "b" > d; " "; "" < c; " "; c > d
a(0 TO 1) = "xy"
PRINT a
a(3 TO 5) = "ABCDEFG"
PRINT a
a(8 TO) = "z"
PRINT a
b = ""
FOR n = 1 TO 5
    b = b + STR$(n)
NEXT n
PRINT b; " "; LEN(b)
IF b = "12345" THEN PRINT "igual"
b = a
a = "otra"
PRINT a; " "; b
PRINT "doble ""comilla"""
