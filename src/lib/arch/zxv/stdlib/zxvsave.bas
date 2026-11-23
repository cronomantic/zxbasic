' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxvsave.bas: saving the data of a game, scores, options, a game in
' progress (ZX-V SDK zxv_save.h, D111), so that a power cut or a card
' taken out does not leave it damaged. Without a card, from ROM: or with
' EXT: offline, an error and the game goes on.
'
'   DIM opt(3) as UByte
'   IF LOADDATA("game.sav", 1, @opt(0), 4) <> 0 THEN defaults()
'   ...
'   LET r = SAVEDATA("game.sav", 1, @opt(0), 4)
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXVSAVE__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXVSAVE__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM Errors of SAVEDATA and LOADDATA (and the ones of the files: 0 is fine)
CONST SAVEDAMAGED as Long = -64
CONST SAVEVERSION as Long = -65
CONST SAVENOFILE as Long = -1
CONST SAVENOCARD as Long = -12

REM SAVEDATA(f$, version, addr, size): saves size bytes from addr (@a(0) of an
REM array, @v of a variable) in file f$, with the version of your data and a
REM CRC; it writes f$~ and then changes it for f$, so a power cut leaves the
REM one before. 0, or an error (negative: SAVENOCARD...)
function SAVEDATA(ByVal f as String, version as UInteger, addr as ULong, size as ULong) as Long
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_save_data
    end asm
end function

REM LOADDATA(f$, version, addr, size): what SAVEDATA saved with that version
REM and size. 0, or SAVENOFILE, SAVENOCARD, SAVEDAMAGED, SAVEVERSION...; if
REM it fails, the memory does not change
function LOADDATA(ByVal f as String, version as UInteger, addr as ULong, size as ULong) as Long
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_load_data
    end asm
end function

REM CRC32(addr, size): the CRC-32 (the one of zip and PNG) of those bytes
function CRC32(addr as ULong, size as ULong) as ULong
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_crc32
    end asm
end function

REM SCORECLEAR [n]: an empty table of n scores (1-10; 5 at the start)
sub SCORECLEAR(n as UByte = 5)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_score_clear
    end asm
end sub

REM SCOREADD(score, name$): puts it in its place (from higher to lower;
REM with a draw, after the ones there) and drops the last one: its place
REM (0 the best), or -1 if it does not get in. Names, up to 7 characters
function SCOREADD(score as ULong, ByVal name as String) as Byte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_score_add
    end asm
end function

REM SCORE(i): the score of place i (0 the best)
function SCORE(i as UByte) as ULong
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_score_value
    end asm
end function

REM SCORENAME(i): its name
function SCORENAME(i as UByte) as String
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_score_name
    end asm
end function

REM SCORESAVE(f$[, version]): saves the table, as SAVEDATA
function SCORESAVE(ByVal f as String, version as UInteger = 1) as Long
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_score_save
    end asm
end function

REM SCORELOAD(f$[, version]): loads it, as LOADDATA
function SCORELOAD(ByVal f as String, version as UInteger = 1) as Long
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_score_load
    end asm
end function

#pragma pop(case_insensitive)

#endif
