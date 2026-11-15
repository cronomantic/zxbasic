REM zxbc --embed: files inside the .zxv (embed.opt). LOAD, MUSICLOAD and
REM SFXLOAD take the one in the current folder if it is there (embed.sd has
REM otro.bin) and, if not, the embedded one with that name, without its
REM folder and without telling capitals apart. A name that is in neither place
REM is T File not found.
#include <zxv.bas>

DIM a(2) AS UBYTE
LOAD "nivel" CODE @a(0)
PRINT "nivel "; a(0); " "; a(1); " "; a(2)
LOAD "otro" CODE @a(0)
PRINT "otro (card) "; a(0); " "; a(1); " "; a(2)
LOAD "SD:/levels/NIVEL.BIN" CODE @a(0), 2
PRINT "path "; a(0); " "; a(1)
VERIFY "nivel" CODE @a(0), 2
PRINT "verify ok"
MUSICLOAD "tono"
MUSICPLAY
PRINT "music "; MUSIC
SFXLOAD "pitido"
SFX 0
PRINT "sfx "; SFXPLAYING
LOAD "nada" CODE @a(0)
