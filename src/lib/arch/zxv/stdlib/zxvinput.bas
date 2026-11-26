' ----------------------------------------------------------------
' This file is released under the MIT License
'
' zxvinput.bas: the keyboard and the joysticks at once, as actions (ZX-V
' SDK zxv_input.h, D111). Two players (p 0 and 1), each one with up to 16
' actions (a bit each: CTRLLEFT, CTRLFIRE...) done by up to 3 keys and by
' buttons of a joystick.
'
'   CTRLUPDATE                    ' once a frame
'   IF CTRLHELD() BAND CTRLLEFT THEN LET x = x - 1
'   IF CTRLPRESSED() BAND CTRLFIRE THEN shoot()
' ----------------------------------------------------------------

#ifndef __LIBRARY_ZXVINPUT__
REM Avoid recursive / multiple inclusion
#define __LIBRARY_ZXVINPUT__

#pragma push(case_insensitive)
#pragma case_insensitive = True

REM The actions of the map of always: the first nine, the bits of the joystick
REM buttons; then ESC, and fire 4, L and R (only on the joystick)
CONST CTRLRIGHT as UInteger = 1
CONST CTRLLEFT as UInteger = 2
CONST CTRLDOWN as UInteger = 4
CONST CTRLUP as UInteger = 8
CONST CTRLFIRE as UInteger = 16
CONST CTRLFIRE2 as UInteger = 32
CONST CTRLFIRE3 as UInteger = 64
CONST CTRLSTART as UInteger = 128
CONST CTRLSELECT as UInteger = 256
CONST CTRLESC as UInteger = 512
CONST CTRLFIRE4 as UInteger = 1024
CONST CTRLL as UInteger = 2048
CONST CTRLR as UInteger = 4096

REM The codes of some keys (USB HID, by their position on a US keyboard)
CONST KEYA as UByte = 4
CONST KEYB as UByte = 5
CONST KEYC as UByte = 6
CONST KEYD as UByte = 7
CONST KEYE as UByte = 8
CONST KEYF as UByte = 9
CONST KEYG as UByte = 10
CONST KEYH as UByte = 11
CONST KEYI as UByte = 12
CONST KEYJ as UByte = 13
CONST KEYK as UByte = 14
CONST KEYL as UByte = 15
CONST KEYM as UByte = 16
CONST KEYN as UByte = 17
CONST KEYO as UByte = 18
CONST KEYP as UByte = 19
CONST KEYQ as UByte = 20
CONST KEYR as UByte = 21
CONST KEYS as UByte = 22
CONST KEYT as UByte = 23
CONST KEYU as UByte = 24
CONST KEYV as UByte = 25
CONST KEYW as UByte = 26
CONST KEYX as UByte = 27
CONST KEYY as UByte = 28
CONST KEYZ as UByte = 29
CONST KEY1 as UByte = 30
CONST KEY2 as UByte = 31
CONST KEY3 as UByte = 32
CONST KEY4 as UByte = 33
CONST KEY5 as UByte = 34
CONST KEY6 as UByte = 35
CONST KEY7 as UByte = 36
CONST KEY8 as UByte = 37
CONST KEY9 as UByte = 38
CONST KEY0 as UByte = 39
CONST KEYENTER as UByte = 40
CONST KEYESCAPE as UByte = 41
CONST KEYBACKSPACE as UByte = 42
CONST KEYTAB as UByte = 43
CONST KEYSPACE as UByte = 44
CONST KEYRIGHTKEY as UByte = 79
CONST KEYLEFTKEY as UByte = 80
CONST KEYDOWNKEY as UByte = 81
CONST KEYUPKEY as UByte = 82
CONST KEYLCTRL as UByte = 224
CONST KEYLSHIFT as UByte = 225
CONST KEYLALT as UByte = 226
CONST KEYRCTRL as UByte = 228
CONST KEYRSHIFT as UByte = 229

REM CTRLRESET [p]: the keys of player p (0 to 3) as at the start: player 0,
REM cursors or Q A O P, fire with Space or M (and the buttons 1 of joystick
REM 0), fire 2 with N or Z, fire 3 with B or X, Enter START, Tab SELECT,
REM Esc, and fire 4, L and R only on the joystick; players 1 to 3, no keys
REM and their joystick (1 to 3)
sub CTRLRESET(p as UByte = 0)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_ctrl_reset
    end asm
end sub

REM CTRLKEY p, action, slot, k: key k (KEYA...; 0, none) is the slot (0-2)
REM of that action (CTRLLEFT...) for player p
sub CTRLKEY(p as UByte, action as UInteger, slot as UByte, k as UByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_ctrl_key
    end asm
end sub

REM CTRLBUTTON p, action, buttons: the joystick buttons (the bits of JOY)
REM that do that action
sub CTRLBUTTON(p as UByte, action as UInteger, buttons as UInteger)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    zxb_ccall zxb_ctrl_button
    end asm
end sub

REM CTRLPAD p, pad: the joystick of player p: 0 to 3, or -1 (none)
sub CTRLPAD(p as UByte, pad as Byte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_ctrl_pad
    end asm
end sub

REM CTRLKEEPKEYS p, 1: CTRLUPDATE does not empty the queue of keys (INKEY$
REM and INPUT still get them); with 0, it does (what is typed in the game
REM does not reach the BASIC when it ends)
sub CTRLKEEPKEYS(p as UByte, onoff as UByte)
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_ctrl_keep_keys
    end asm
end sub

REM CTRLUPDATE [p]: once a frame, reads the keys and the joystick
sub CTRLUPDATE(p as UByte = 0)
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_ctrl_update
    end asm
end sub

REM CTRLHELD([p]): the actions held now (CTRLLEFT + CTRLFIRE...)
function CTRLHELD(p as UByte = 0) as UInteger
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_ctrl_held
    end asm
end function

REM CTRLPRESSED([p]): the actions pressed in this frame
function CTRLPRESSED(p as UByte = 0) as UInteger
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_ctrl_pressed
    end asm
end function

REM CTRLRELEASED([p]): the actions released in this frame
function CTRLRELEASED(p as UByte = 0) as UInteger
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_ctrl_released
    end asm
end function

REM CTRLFRAMES(p, action): the frames it has been held (0: it is not)
function CTRLFRAMES(p as UByte, action as UInteger) as UInteger
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    zxb_ccall zxb_ctrl_frames
    end asm
end function

REM CTRLREPEAT(p, actions[, delay, rate]): 1 when one of them is pressed
REM and, held, every rate frames from frame delay on: for menus
function CTRLREPEAT(p as UByte, actions as UInteger, delay as UInteger = 10, rate as UInteger = 3) as UByte
    asm
    lw a0, 8(s0)
    lw a1, 12(s0)
    lw a2, 16(s0)
    lw a3, 20(s0)
    zxb_ccall zxb_ctrl_repeat
    end asm
end function

REM CTRLMAP([p]): the address of the keys of player p, CTRLMAPSIZE bytes:
REM SAVEDATA and LOADDATA (zxvsave.bas) keep them
function CTRLMAP(p as UByte = 0) as ULong
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_ctrl_map
    end asm
end function

REM CTRLMAPSIZE: the bytes of the keys of a player
function CTRLMAPSIZE() as UInteger
    asm
    zxb_ccall zxb_ctrl_map_size
    end asm
end function

REM NEWKEY: the key pressed since the call before (its code, KEYA...), or
REM -1; the first call only takes note of what is held. To change the keys
function NEWKEY() as Integer
    asm
    zxb_ccall zxb_new_key
    end asm
end function

REM KEYNAME(k): the name of key k in the language of the machine ("Space",
REM "Up", "F12"; the keys of symbols, "Key 33")
function KEYNAME(k as UByte) as String
    asm
    lw a0, 8(s0)
    zxb_ccall zxb_key_name
    end asm
end function

#pragma pop(case_insensitive)

#endif
