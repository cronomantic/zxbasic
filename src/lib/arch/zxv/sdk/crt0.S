/*
 * crt0.S - Entrada de un programa .ZXV.
 *
 * La ROM ya ha cargado la imagen, borrado el BSS y preparado la pila; aquí
 * se borra lo que el programa tenga en la VRAM oculta (ZXV_VRAM0_DATA y
 * ZXV_VRAM1_DATA, D108: no va en el fichero), se apunta tp a las variables
 * de hilo (las de picolibc, zxv.ld) y se salta a main. Si main vuelve, su
 * valor llega a la ROM (o a USR).
 */
    .section .text.start, "ax"
    .globl _start
_start:
    la t0, __vram0_start
    la t1, __vram0_end
1:  bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:  la t0, __vram1_start
    la t1, __vram1_end
3:  bgeu t0, t1, 4f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 3b
4:  la tp, __tls_base
    tail main
