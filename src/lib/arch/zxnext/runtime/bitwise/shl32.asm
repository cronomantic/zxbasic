
    push namespace core

__SHL32: ; Left Logical Shift 32 bits

    sla l
    rl h
    rl e
    rl d
    ret

    pop namespace
