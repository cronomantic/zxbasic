REM PALETTE and PALETTERESET (zxv.bas), and SAVE, LOAD and VERIFY SCREEN$ with
REM the colours of the screen (ZX-V decision D90)
#include <zxv.bas>

REM MODE puts the standard colours back: the screen is saved without a palette
PALETTE 1, 255, 0, 255
MODE 0
SAVE "std" SCREEN$

BORDER 1: PAPER 7: INK 0: CLS
BOX 2, 0, 0, 64, 64
BOX 4, 64, 0, 64, 64
BOX 5, 128, 0, 64, 64
PALETTE 2, 0, 128, 255
PALETTE 4, 255, 128, 0
REMAP 5, 4

REM The colours as they are seen: 5 is orange too
SAVE "pal" SCREEN$
PALETTERESET
REMAPRESET
LOAD "pal" SCREEN$
VERIFY "pal" SCREEN$

REM The flags (byte 18 of the file) and the colours 2 and 5 of the palette
LOAD "std.gfx" CODE 40000, 64
PRINT AT 20, 0; PEEK 40018
LOAD "pal.gfx" CODE 40000, 64
PRINT PEEK 40018; " "; PEEK 40024 + 256 * PEEK 40025; " "; PEEK 40030 + 256 * PEEK 40031

REM With another colour it is not the same screen, though 15 is not on it
PALETTE 15, 1, 2, 3
VERIFY "pal" SCREEN$
PRINT "not here"
