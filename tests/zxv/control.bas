REM Loops, conditions, jumps and subroutines
DIM i AS INTEGER
DIM s AS LONG
FOR i = 1 TO 10
    s = s + i
NEXT i
PRINT "FOR "; s; " "; i
FOR i = 10 TO 1 STEP -3: PRINT i; " ";: NEXT i
PRINT
DIM x AS FLOAT
FOR x = 0 TO 1 STEP 0.25: PRINT x; " ";: NEXT x
PRINT
i = 0
WHILE i < 5
    i = i + 1
    IF i = 3 THEN CONTINUE WHILE
    PRINT i;
END WHILE
PRINT
i = 0
DO
    i = i + 2
LOOP UNTIL i >= 7
PRINT "DO "; i
i = 100
DO WHILE i > 1
    i = i / 2
    IF i = 6 THEN EXIT DO
LOOP
PRINT "EXIT "; i
FOR i = 1 TO 4
    IF i = 1 THEN
        PRINT "one ";
    ELSEIF i = 2 THEN
        PRINT "two ";
    ELSE
        PRINT "many ";
    END IF
NEXT i
PRINT
GOSUB 1000
GOSUB 1000
FOR i = 0 TO 3
    ON i GOTO 2000, 2010, 2020
    PRINT "out ";
    GOTO 2030
2000 PRINT "a ";: GOTO 2030
2010 PRINT "b ";: GOTO 2030
2020 PRINT "c ";
2030 NEXT i
PRINT
FOR i = 0 TO 2
    ON i GOSUB 3000, 3010
NEXT i
PRINT
GOTO 9000
1000 PRINT "sub "; i
RETURN
3000 PRINT "g0 ";: RETURN
3010 PRINT "g1 ";: RETURN
9000 PRINT "end"
END
