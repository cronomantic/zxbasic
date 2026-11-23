REM sdkinflate.bas: zxvinflate.bas (ZX-V SDK zxv_inflate.h, D111): a .gz in an array
#include <zxvinflate.bas>

REM gzip of "hola, ZX-V! hola, ZX-V! hola, ZX-V!" (Python gzip.compress, mtime 0)
DIM gz(34) as UByte => {31, 139, 8, 0, 0, 0, 0, 0, 2, 255, 203, 200, 207, 73, 212, 81, 136, 138, 208, 13, 83, 84, 200, 192, 206, 6, 0, 118, 177, 182, 218, 35, 0, 0, 0}
DIM dst(63) as UByte
PRINT GUNZIPSIZE(@gz(0), 35)
LET n = GUNZIP(@gz(0), 35, @dst(0), 64)
PRINT n
FOR i = 0 TO n - 1
    PRINT CHR$(dst(i));
NEXT i
PRINT
PRINT GUNZIP(@gz(0), 35, @dst(0), 10)
LET gz(20) = gz(20) BXOR 1
PRINT GUNZIP(@gz(0), 35, @dst(0), 64)
