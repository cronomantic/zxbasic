/* Programas .ZXV: todo contiguo en la RAM de usuario (spec §9.2). */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    URAM (rwx) : ORIGIN = 0x20034000, LENGTH = 0x3D000
    /* La VRAM oculta (spec §3.1, D108): en modo 0 y con VSIZE 0, cada página
     * solo enseña sus primeros 24 KB; los 72 KB de detrás son del programa. */
    VRAM0X (rw) : ORIGIN = 0x2000A000, LENGTH = 0x12000
    VRAM1X (rw) : ORIGIN = 0x20022000, LENGTH = 0x12000
}

SECTIONS
{
    /* Lo de los bancos de código no lo cogen estas reglas: tools/zxvbanks.py
     * cambia el nombre de sus secciones (.zxvbank_in.BANCO.text...) y las
     * pone en sus zonas (ZXVBANKS, abajo). */
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
    /* Lo declarado con ZXV_VRAM0_DATA y ZXV_VRAM1_DATA (zxv.h): antes que
     * .bss, que si no se lo quedaría (.bss.*). No va en el fichero: crt0.S
     * lo pone a cero al empezar. */
    .zxv_vram0 (NOLOAD) : {
        __vram0_start = .;
        *(.bss.zxv_vram0 .bss.zxv_vram0.*)
        . = ALIGN(4);
        __vram0_end = .;
    } > VRAM0X
    .zxv_vram1 (NOLOAD) : {
        __vram1_start = .;
        *(.bss.zxv_vram1 .bss.zxv_vram1.*)
        . = ALIGN(4);
        __vram1_end = .;
    } > VRAM1X
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*) *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > URAM
    /* Las zonas de los bancos de código (D109): tools/zxvbanks.py las pone
     * aquí, detrás del BSS, y así cuentan en lo que ocupa el programa. */
    /* ZXVBANKS */
    __zxv_end = .;
    /* El montón de malloc (picolibc, ver zxvc.c): hasta 32 KB por debajo del
     * final de la RAM de usuario, que son para la pila. */
    __heap_start = ALIGN(__zxv_end, 8);
    __heap_end = ORIGIN(URAM) + LENGTH(URAM) - 0x8000;
    /* La pila del programa sigue a la de la ROM, que al lanzarlo usa unos
     * 1,2 KB, y el cargador pide 4 KB libres por debajo (STACK_RESERVE en
     * rom/src/files.c): lo que pase de aquí lo rechaza ("No cabe en
     * memoria"). */
    ASSERT(__zxv_end <= ORIGIN(URAM) + LENGTH(URAM) - 0x1800,
           "el programa no cabe: la imagen, el BSS y las zonas de los bancos, 6 KB por debajo del final de la RAM de usuario (la pila)")
}
