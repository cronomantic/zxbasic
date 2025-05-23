#include once <stackf.asm>

    push namespace core

ATAN: ; Computes ATAN using ROM FP-CALC
    call __FPSTACK_PUSH

    rst 28h	; ROM CALC
    defb 24h ; ATAN
    defb 38h ; END CALC

    jp __FPSTACK_POP

    pop namespace
