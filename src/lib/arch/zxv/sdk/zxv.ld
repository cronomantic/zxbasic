/* Programas .ZXV: todo contiguo en la RAM de usuario (spec §9.2). */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    URAM (rwx) : ORIGIN = 0x20034000, LENGTH = 0x3E000
}

SECTIONS
{
    .text : {
        KEEP(*(.text.start))
        *(.text*)
    } > URAM
    .rodata : { *(.rodata*) *(.srodata*) } > URAM
    .data : { *(.data*) *(.sdata*) . = ALIGN(4); } > URAM
    /* Las variables de hilo (el errno de picolibc): crt0.S pone tp en
     * __tls_base. .tdata va en la imagen; .tbss, a cero con el BSS, en su
     * propio sitio (el enlazador no le reserva memoria). */
    .tdata : { __tls_base = .; *(.tdata .tdata.*) . = ALIGN(4); } > URAM
    __image_end = .;
    .tbss (NOLOAD) : { *(.tbss .tbss.*) *(.tcommon) } > URAM
    .tbss_space (NOLOAD) : { . = . + SIZEOF(.tbss); . = ALIGN(4); } > URAM
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*) *(.sbss*) *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > URAM
    /* El montón de malloc (picolibc, ver zxvc.c): hasta 32 KB por debajo del
     * final de la RAM de usuario, que son para la pila. */
    __heap_start = ALIGN(__bss_end, 8);
    __heap_end = ORIGIN(URAM) + LENGTH(URAM) - 0x8000;
}
