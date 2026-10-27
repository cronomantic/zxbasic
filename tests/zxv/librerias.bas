REM The standard library of ZX-V
#include <string.bas>
#include <hex.bas>
#include <pos.bas>
#include <csrlin.bas>
#include <attr.bas>
#include <point.bas>
#include <screen.bas>
#include <alloc.bas>
#include <memcopy.bas>
#include <random.bas>
#include <round.bas>
#include <asc.bas>
#include <keys.bas>
#include <fmath.bas>
#include <hmirror.bas>
#include <clearbox.bas>
#include <putchars.bas>
#include <retrace.bas>

DIM s AS STRING = "  Hola Mundo  "
PRINT "["; ucase(s); "] ["; lcase(s); "] ["; trim(s, " "); "]"
PRINT mid$("ZX Spectrum", 3, 4); " "; left$("ZX-V", 2); " "; right$("ZX-V", 1); " "; strpos("abcdef", "cd"); " "; inStr("abc", "z")
PRINT hex(3735928559); " "; hex16(4660); " "; hex8(10)
PRINT AT 5, 7; "x";
PRINT pos(); " "; csrlin()
setattr(5, 7, 79)
PRINT attr(5, 7); " "; attraddr(5, 7)
PLOT 10, 20
PRINT point(10, 20); " "; point(11, 20); " "; point(0, 200)
PRINT AT 8, 3; "Q"; AT 9, 3; INVERSE 1; "R"; INVERSE 0
PRINT AT 10, 0; screen(8, 3); screen(9, 3); "["; screen(10, 31); "]"
DIM p AS ULONG
DIM free0 AS ULONG
free0 = memavail()
p = allocate(1000)
PRINT p > 65535; " "; memavail() < free0; " "; maxavail() > 1000
MemSet(p, 65, 10)
POKE p + 10, 0
MemCopy(p, p + 20, 11)
PRINT PEEK(p + 25); " "; PEEK(p + 30)
p = reallocate(p, 5000)
PRINT PEEK(p + 5)
deallocate(p)
PRINT memavail() = free0
MemCopy(16384 + 8 * 256 + 32 * 3 + 3 - 2048, 30000, 1)
PRINT randomLimit(0); " "; randomLimit(5) <= 5; " "; randInt() <> randInt()
PRINT Round(3.14159, 2); " "; Round(-2.5); " "; asc("ABC", 1)
DIM w AS UBYTE
FOR w = 1 TO 20: waitretrace: NEXT w
PRINT MultiKeys(KEYA); " "; GetKeyScanCode()
PRINT fSin(30); " "; fCos(60); " "; fSqrt(16)
PRINT hMirror(1); " "; hMirror(160)
DIM gfx(7) AS UBYTE => {255, 129, 129, 129, 129, 129, 129, 255}
DIM back(7) AS UBYTE
putChars(20, 2, 1, 1, @gfx(0))
getChars(20, 2, 1, 1, @back(0))
PRINT back(0); " "; back(1); " "; back(7)
paint(20, 2, 2, 1, 22)
PRINT attr(2, 20); " "; attr(2, 21)
clearBox(20, 2, 1, 1)
getChars(20, 2, 1, 1, @back(0))
PRINT back(0)
waitretrace
PRINT "fin"
