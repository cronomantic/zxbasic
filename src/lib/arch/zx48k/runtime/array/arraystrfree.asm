; This routine is in charge of freeing an array of strings from memory
; HL = Pointer to start of array in memory
; Top of the stack = Number of elements of the array

#include once <free.asm>

    push namespace core

__ARRAYSTR_FREE:
    PROC

    LOCAL __ARRAY_LOOP

    ex de, hl
    pop hl		; (ret address)
    ex (sp), hl	; Callee -> HL = Number of elements

    ex de, hl

__ARRAYSTR_FREE_FAST:	; Fastcall entry: DE = Number of elements
    ld a, h
    or l
    ret z		; ret if NULL

    ld b, d
    ld c, e

__ARRAY_LOOP:
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl		; DE = (HL) = String Pointer

    push hl
    push bc
    ex de, hl
    call __MEM_FREE ; Frees it from memory
    pop bc
    pop hl

    dec bc
    ld a, b
    or c
    jp nz, __ARRAY_LOOP

    ret		    ; Frees it and return

    ENDP


__ARRAYSTR_FREE_MEM: ; like the above, buf also frees the array itself
    ex de, hl
    pop hl		; (ret address)
    ex (sp), hl	; Callee -> HL = Number of elements
    ex de, hl

    push hl		; Saves array pointer for later
    call __ARRAYSTR_FREE_FAST
    pop hl		; recovers array block pointer

    jp __MEM_FREE	; Frees it and returns from __MEM_FREE

    pop namespace
