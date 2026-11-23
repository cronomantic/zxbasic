' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxvmath.bas: whole numbers maths for games (ZX-V SDK zxv_math.h, D111):
' sine and cosine as Fixed with the table of the graphics engine, atan2,
' square roots and random numbers. Angles in turns of 65536 (the ones of
' the engine; fmath.bas has fSin and fCos in degrees).
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXVMATH__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXVMATH__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM TSIN(a), TCOS(a): sine and cosine of an angle in turns of 65536 (16384 is
REM a quarter: 90 degrees), the same as the graphics engine of the ROM
function TSIN(a as Long) as Fixed
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_tsin
    end asm
end function

function TCOS(a as Long) as Fixed
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_tcos
    end asm
end function

REM TATAN2(y, x): the angle of (x, y) in turns of 65536: 0 towards the
REM growing x, 16384 towards the growing y (down on the screen)
function TATAN2(y as Long, x as Long) as UInteger
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_tatan2
    end asm
end function

REM ISQRT(v): the square root, rounded down
function ISQRT(v as ULong) as ULong
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_isqrt
    end asm
end function

REM HYPOT(x, y): the length of (x, y), rounded down (a distance)
function HYPOT(x as Long, y as Long) as ULong
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_hypot
    end asm
end function

REM RNDSEED s: the seed of RNDNEXT and RNDRANGE (the same seed, the same
REM numbers: for levels that are always the same)
sub RNDSEED(s as ULong)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_rnd_seed
    end asm
end sub

REM RNDNEXT: a random number of 32 bits
function RNDNEXT() as ULong
    asm
    zxb_ccall zxb_rnd_next
    end asm
end function

REM RNDRANGE(lo, hi): a random number from lo to hi, both included
function RNDRANGE(lo as Long, hi as Long) as Long
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_rnd_range
    end asm
end function

#pragma pop(case_insensitive)

#endif
