	org 32768
.core.__START_PROGRAM:
	di
	push ix
	push iy
	exx
	push hl
	exx
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
	DEFB 00
_bPtr:
	DEFB 00, 00
.core.ZXBASIC_USER_DATA_END:
.core.__MAIN_PROGRAM__:
	ld hl, 32768
	ld (_bPtr), hl
	jp .LABEL.__LABEL0
.LABEL.__LABEL3:
	ld hl, (_bPtr)
	ld a, (hl)
	cpl
	ld (_a), a
	ld (hl), a
	inc hl
	ld (_bPtr), hl
.LABEL.__LABEL0:
	ld hl, 32793
	ld de, (_bPtr)
	or a
	sbc hl, de
	jp nc, .LABEL.__LABEL3
	ld bc, 0
.core.__END_PROGRAM:
	di
	ld hl, (.core.__CALL_BACK__)
	ld sp, hl
	exx
	pop hl
	pop iy
	pop ix
	exx
	ei
	ret
	;; --- end of user code ---
	END
