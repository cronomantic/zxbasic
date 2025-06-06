	org 32768
.core.__START_PROGRAM:
	di
	push iy
	ld iy, 0x5C3A  ; ZX Spectrum ROM variables address
	ld (.core.__CALL_BACK__), sp
	ei
	jp .core.__MAIN_PROGRAM__
.core.__CALL_BACK__:
	DEFW 0
.core.ZXBASIC_USER_DATA:
	; Defines USER DATA Length in bytes
.core.ZXBASIC_USER_DATA_LEN EQU .core.ZXBASIC_USER_DATA_END - .core.ZXBASIC_USER_DATA
	.core.__LABEL__.ZXBASIC_USER_DATA_LEN EQU .core.ZXBASIC_USER_DATA_LEN
	.core.__LABEL__.ZXBASIC_USER_DATA EQU .core.ZXBASIC_USER_DATA
_a:
	DEFB 00, 00, 00, 00
_b:
	DEFB 00
.core.ZXBASIC_USER_DATA_END:
.core.__MAIN_PROGRAM__:
	ld a, (_b)
	ld b, a
	ld hl, (_a)
	ld de, (_a + 2)
	or a
	jr z, .LABEL.__LABEL1
.LABEL.__LABEL0:
	call .core.__SHRL32
	djnz .LABEL.__LABEL0
.LABEL.__LABEL1:
	ld (_a), hl
	ld (_a + 2), de
	ld hl, (_a)
	ld de, (_a + 2)
	call .core.__SHRL32
	ld (_a), hl
	ld (_a + 2), de
	ld hl, (_a)
	ld de, (_a + 2)
	ld (_a), hl
	ld (_a + 2), de
	ld a, (_b)
	xor a
	ld l, a
	ld h, 0
	ld e, h
	ld d, h
	ld (_a), hl
	ld (_a + 2), de
	ld hl, 0
	ld b, h
	ld c, l
.core.__END_PROGRAM:
	di
	ld hl, (.core.__CALL_BACK__)
	ld sp, hl
	pop iy
	ei
	ret
	;; --- end of user code ---
#line 1 "/zxbasic/src/lib/arch/zxnext/runtime/bitwise/shrl32.asm"
	    push namespace core
__SHRL32: ; Right Logical Shift 32 bits
	    srl d
	    rr e
	    rr h
	    rr l
	    ret
	    pop namespace
#line 42 "arch/zxnext/shru32.bas"
	END
