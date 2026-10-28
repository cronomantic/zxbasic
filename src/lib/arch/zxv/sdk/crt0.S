/*
 * crt0.S - Entrada de un programa .ZXV.
 *
 * La ROM ya ha cargado la imagen, borrado el BSS y preparado la pila; aquí
 * se apunta tp a las variables de hilo (las de picolibc, zxv.ld) y se salta
 * a main. Si main vuelve, su valor llega a la ROM (o a USR).
 */
    .section .text.start, "ax"
    .globl _start
_start:
    la tp, __tls_base
    tail main
