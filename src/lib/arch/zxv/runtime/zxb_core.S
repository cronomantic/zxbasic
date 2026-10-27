/*
 * zxb_core.S - Memory accesses of the compiled code that the runtime
 * emulates: below 64K (the ZX Spectrum memory map) or misaligned.
 *
 * They keep every register, except the result, because the compiled code
 * may have the other operand of an instruction in one.
 */
    .text

/* __zxb_rd: t0 = address, t1 = size (1, 2, 4 or 8) -> a0 (a1 too if size is 8) */
    .globl __zxb_rd
__zxb_rd:
    addi sp, sp, -64
    sw ra, 0(sp)
    sw a1, 4(sp)
    sw a2, 8(sp)
    sw a3, 12(sp)
    sw a4, 16(sp)
    sw a5, 20(sp)
    sw a6, 24(sp)
    sw a7, 28(sp)
    sw t0, 32(sp)
    sw t1, 36(sp)
    sw t2, 40(sp)
    sw t3, 44(sp)
    sw t4, 48(sp)
    sw t5, 52(sp)
    sw t6, 56(sp)
    sw s1, 60(sp)
    mv s1, sp
    andi sp, sp, -16
    mv a0, t0
    mv a1, t1
    call zxb_vread
    mv sp, s1
    lw t1, 36(sp)
    li t2, 8
    beq t1, t2, 1f
    lw a1, 4(sp)
1:
    lw ra, 0(sp)
    lw a2, 8(sp)
    lw a3, 12(sp)
    lw a4, 16(sp)
    lw a5, 20(sp)
    lw a6, 24(sp)
    lw a7, 28(sp)
    lw t0, 32(sp)
    lw t2, 40(sp)
    lw t3, 44(sp)
    lw t4, 48(sp)
    lw t5, 52(sp)
    lw t6, 56(sp)
    lw s1, 60(sp)
    addi sp, sp, 64
    ret

/* __zxb_wr: t0 = address, t1 = size, a0 = value (a1 = high word if size is 8) */
    .globl __zxb_wr
__zxb_wr:
    addi sp, sp, -64
    sw ra, 0(sp)
    sw a0, 4(sp)
    sw a1, 8(sp)
    sw a2, 12(sp)
    sw a3, 16(sp)
    sw a4, 20(sp)
    sw a5, 24(sp)
    sw a6, 28(sp)
    sw a7, 32(sp)
    sw t0, 36(sp)
    sw t1, 40(sp)
    sw t2, 44(sp)
    sw t3, 48(sp)
    sw t4, 52(sp)
    sw t5, 56(sp)
    sw t6, 60(sp)
    addi sp, sp, -16
    sw s1, 0(sp)
    mv s1, sp
    andi sp, sp, -16
    mv a2, a1
    mv a1, a0
    mv a0, t0
    mv a3, t1
    call zxb_vwrite
    mv sp, s1
    lw s1, 0(sp)
    addi sp, sp, 16
    lw ra, 0(sp)
    lw a0, 4(sp)
    lw a1, 8(sp)
    lw a2, 12(sp)
    lw a3, 16(sp)
    lw a4, 20(sp)
    lw a5, 24(sp)
    lw a6, 28(sp)
    lw a7, 32(sp)
    lw t0, 36(sp)
    lw t1, 40(sp)
    lw t2, 44(sp)
    lw t3, 48(sp)
    lw t4, 52(sp)
    lw t5, 56(sp)
    lw t6, 60(sp)
    addi sp, sp, 64
    ret

/* __zxb_call_basic: calls a compiled BASIC function without parameters
 * (a0 = its address), which takes its return address from the stack.
 * Returns what it returns (a0, a1). */
    .globl __zxb_call_basic
__zxb_call_basic:
    addi sp, sp, -16
    sw ra, 12(sp)
    sw s0, 8(sp)
    sw s1, 4(sp)
    la t0, 1f
    addi sp, sp, -4
    sw t0, 0(sp)
    jr a0
1:
    lw ra, 12(sp)
    lw s0, 8(sp)
    lw s1, 4(sp)
    addi sp, sp, 16
    ret
