REM SAVE, LOAD and VERIFY on the SD card: DATA, SCREEN$, CODE, raw bytes
DIM a(9) AS UBYTE
DIM big AS ULONG
DIM i AS UBYTE

FOR i = 0 TO 9: a(i) = i * 3: NEXT i
SAVE "numbers" CODE @a(0), 10
FOR i = 0 TO 9: a(i) = 0: NEXT i
LOAD "numbers" CODE
PRINT a(0); " "; a(5); " "; a(9)

big = 4000000000
SAVE "big" DATA big
big = 0
LOAD "big" DATA big
PRINT big

REM The screen, as a tape
CLS
PRINT AT 5, 3; PAPER 2; INK 7; "saved screen"
SAVE "screen" SCREEN$
CLS
LOAD "screen" SCREEN$

REM Raw bytes (with an extension) and VERIFY
FOR i = 0 TO 9: POKE 23296 + i, 200 + i: NEXT i
SAVE "low.bin" CODE 23296, 10
VERIFY "low.bin" CODE 23296, 10
FOR i = 0 TO 9: POKE 23296 + i, 0: NEXT i
LOAD "low.bin" CODE 23296
PRINT AT 10, 0; PEEK 23296; " "; PEEK 23305

REM A tape without the address: back where it was saved
SAVE "low" CODE 23296, 10
POKE 23300, 0
LOAD "low" CODE
PRINT PEEK 23300

REM All the variables
big = 77
SAVE "all" DATA
big = 0
LOAD "all" DATA
PRINT big

LOAD "nothing" CODE
PRINT "not here"
