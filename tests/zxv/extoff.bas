REM EXT: without a server (zxv-cli without --ext): LOAD stops with
REM V Disk error (no server for EXT:), as the ZX-V BASIC says it.
#include <zxv.bas>

DIM a(2) AS UBYTE
PRINT "antes"
LOAD "EXT:nada" CODE @a(0)
PRINT "no llega"
