' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxvinflate.bas: data compressed with gzip, uncompressed in the memory
' (ZX-V SDK zxv_inflate.h, D111). Compress them on the PC (gzip -9) and
' load them with LOAD ... CODE or put them in an array.
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXVINFLATE__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXVINFLATE__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Errors (negative)
CONST INFLATEDATA as Long = -66
CONST INFLATESPACE as Long = -67

REM GUNZIP(src, n, dst, max): the .gz of n bytes at src, uncompressed at dst
REM (max bytes at most), checked with its CRC: the bytes, or INFLATEDATA
REM (damaged) or INFLATESPACE (it does not fit)
function GUNZIP(src as ULong, n as ULong, dst as ULong, max as ULong) as Long
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_gunzip
    end asm
end function

REM GUNZIPSIZE(src, n): how much the .gz takes uncompressed
function GUNZIPSIZE(src as ULong, n as ULong) as Long
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_gunzip_size
    end asm
end function

REM INFLATE(src, n, dst, max): the same with deflate alone (no .gz)
function INFLATE(src as ULong, n as ULong, dst as ULong, max as ULong) as Long
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_inflate
    end asm
end function

#pragma pop(case_insensitive)

#endif
