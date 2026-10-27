' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of keys.bas (the ZX Spectrum one is Copyleft (k) by
' Jose Rodriguez-Rosa, a.k.a. Boriel, and its authors). Its ASM blocks
' are RISC-V: parameters at 8(s0), 12(s0)... (by value, 4 bytes each),
' the result in a0, and zxb_ccall calls the runtime in C.
' ----------------------------------------------------------------

#ifndef __LIBRARY_IO_KEYS__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_IO_KEYS__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM The keyboard matrix of ZX-V is the one of the Spectrum: the scan codes
REM are the high byte of the port and the bits of the keys.

function GetKey AS UByte
    Dim k AS UByte
    do
        k = CODE INKEY$
    loop UNTIL k
    return k
end function

REM Not 0 if the key(s) of the scan code are pressed
function MultiKeys(scancode as UInteger) AS UByte
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_lib_multikeys
    end asm
end function

REM The scan code of a pressed key, or 0
function FASTCALL GetKeyScanCode AS UInteger
    asm
    zxb_ccall zxb_lib_scancode
    end asm
end function

#pragma pop(case_insensitive)

REM Scan Codes

REM 1st ROW
const KEYB        AS UInteger = 07F10h
const KEYN        AS UInteger = 07F08h
const KEYM        AS UInteger = 07F04h
const KEYSYMBOL   AS UInteger = 07F02h
const KEYSPACE    AS UInteger = 07F01h

REM 2nd ROW
const KEYH        AS UInteger = 0BF10h
const KEYJ        AS UInteger = 0BF08h
const KEYK        AS UInteger = 0BF04h
const KEYL        AS UInteger = 0BF02h
const KEYENTER    AS UInteger = 0BF01h

REM 3rd ROW
const KEYY        AS UInteger = 0DF10h
const KEYU        AS UInteger = 0DF08h
const KEYI        AS UInteger = 0DF04h
const KEYO        AS UInteger = 0DF02h
const KEYP        AS UInteger = 0DF01h

REM 4th ROW
const KEY6        AS UInteger = 0EF10h
const KEY7        AS UInteger = 0EF08h
const KEY8        AS UInteger = 0EF04h
const KEY9        AS UInteger = 0EF02h
const KEY0        AS UInteger = 0EF01h

REM 5th ROW
const KEY5        AS UInteger = 0F710h
const KEY4        AS UInteger = 0F708h
const KEY3        AS UInteger = 0F704h
const KEY2        AS UInteger = 0F702h
const KEY1        AS UInteger = 0F701h

REM 6th ROW
const KEYT        AS UInteger = 0FB10h
const KEYR        AS UInteger = 0FB08h
const KEYE        AS UInteger = 0FB04h
const KEYW        AS UInteger = 0FB02h
const KEYQ        AS UInteger = 0FB01h

REM 7th ROW
const KEYG        AS UInteger = 0FD10h
const KEYF        AS UInteger = 0FD08h
const KEYD        AS UInteger = 0FD04h
const KEYS        AS UInteger = 0FD02h
const KEYA        AS UInteger = 0FD01h

REM 8th ROW
const KEYV        AS UInteger = 0FE10h
const KEYC        AS UInteger = 0FE08h
const KEYX        AS UInteger = 0FE04h
const KEYZ        AS UInteger = 0FE02h
const KEYCAPS     AS UInteger = 0FE01h

#endif
