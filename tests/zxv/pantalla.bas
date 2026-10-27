REM Colours, graphics and the screen memory
BORDER 1
PAPER 6: INK 1: CLS
PRINT AT 0, 0; "ZX Basic en ZX-V"
PRINT AT 2, 0; INK 2; "rojo "; PAPER 0; INK 7; BRIGHT 1; "brillante"; BRIGHT 0; PAPER 6; INK 4; " verde"
PRINT AT 4, 0; INVERSE 1; "inverso"
PRINT AT 6, 0; CHR$(144); CHR$(145); CHR$(128 + 15); CHR$(128 + 6)
PLOT 0, 0: DRAW 255, 0: DRAW 0, 100: DRAW -255, 0: DRAW 0, -100
CIRCLE 200, 60, 30
PLOT 20, 20: DRAW 60, 60, PI
INK 3
PLOT 100, 50
DIM i AS UBYTE
FOR i = 0 TO 7
    POKE 22528 + 32 * 10 + i, i * 8 + 7 - i
NEXT i
POKE 16384 + 2048 + 1, 255
PRINT AT 12, 0; "fin"
