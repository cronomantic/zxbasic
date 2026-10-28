/*
 * zxv.h - Direcciones y constantes de la máquina ZX-V (spec v0.1).
 *
 * Única fuente de verdad compartida por la ROM, el SDK y el emulador.
 * Solo contiene #define, así que sirve tanto en C (anfitrión y RISC-V) como
 * en ensamblador (preprocesado con cpp).
 */
#ifndef ZXV_H
#define ZXV_H

#define ZXV_SPEC_VERSION        0x00000001u     /* 0.1 */

/* --- Mapa de memoria (spec §3) ------------------------------------------ */
#define ZXV_FLASH_BASE          0x10000000u
#define ZXV_FLASH_SIZE          0x00400000u     /* 4 MB (Pico 2) */
#define ZXV_PLATFORM_BASE       0x10000000u     /* firmware de plataforma */
#define ZXV_ROM_BASE            0x10040000u     /* ROM ZX-V */
#define ZXV_ROMDISK_BASE        0x100C0000u     /* disco ROM (la unidad ROM:), hasta el final */
#define ZXV_ROM_SIZE            0x00080000u

#define ZXV_SRAM_BASE           0x20000000u
#define ZXV_SRAM_SIZE           0x00082000u     /* 520 KB */
#define ZXV_SYS_BASE            0x20000000u     /* área de sistema, 16 KB */
#define ZXV_VRAM0_BASE          0x20004000u
#define ZXV_VRAM1_BASE          0x2001C000u
#define ZXV_VRAM_PAGE_SIZE      0x00018000u     /* 96 KB */
#define ZXV_URAM_BASE           0x20034000u     /* RAM de usuario */
#define ZXV_URAM_END            0x20072000u     /* exclusivo */
#define ZXV_RESERVED_BASE       0x20072000u     /* reservada (PMP) */
#define ZXV_RESERVED_END        0x20082000u

/* --- Área de sistema (spec §3.2) ---------------------------------------- */
#define ZXV_API_TABLE           (ZXV_SYS_BASE + 0x0000u)
#define ZXV_VIDEO_REGS          (ZXV_SYS_BASE + 0x0400u)
#define ZXV_INPUT_REGS          (ZXV_SYS_BASE + 0x0500u)
#define ZXV_SOUND_REGS          (ZXV_SYS_BASE + 0x0600u)
#define ZXV_SYSVARS             (ZXV_SYS_BASE + 0x0700u)
#define ZXV_AY_FIFO             (ZXV_SYS_BASE + 0x0800u)
#define ZXV_BEEP_QUEUE          (ZXV_SYS_BASE + 0x0A00u)
#define ZXV_REMAP_TABLES        (ZXV_SYS_BASE + 0x1200u)
#define ZXV_PLATFORM_IF         (ZXV_SYS_BASE + 0x1240u)
#define ZXV_LINE_TABLE          (ZXV_SYS_BASE + 0x1800u)
#define ZXV_ROM_WORKSPACE       (ZXV_SYS_BASE + 0x2A00u)
#define ZXV_ROM_WORKSPACE_END   (ZXV_SYS_BASE + 0x4000u)

/* --- Registros de vídeo (spec §4.5) ------------------------------------- */
#define ZXV_V_MODE              (ZXV_VIDEO_REGS + 0x00u)   /* u8  */
#define ZXV_V_PAGE              (ZXV_VIDEO_REGS + 0x01u)   /* u8  */
#define ZXV_V_BORDER            (ZXV_VIDEO_REGS + 0x02u)   /* u8  */
#define ZXV_V_FLAGS             (ZXV_VIDEO_REGS + 0x03u)   /* u8  */
#define ZXV_V_SCROLL_X          (ZXV_VIDEO_REGS + 0x04u)   /* u16 */
#define ZXV_V_SCROLL_Y          (ZXV_VIDEO_REGS + 0x06u)   /* u16 */
#define ZXV_V_REMAP             (ZXV_VIDEO_REGS + 0x08u)   /* u8  */
#define ZXV_V_VSIZE             (ZXV_VIDEO_REGS + 0x09u)   /* u8, campo virtual (modo 0) */
#define ZXV_V_OUT_INFO          (ZXV_VIDEO_REGS + 0x0Cu)   /* u32, solo lectura */

#define ZXV_VFLAG_LINETAB       0x01u
#define ZXV_VFLAG_BLANK         0x02u

#define ZXV_OUT_DVI             1u      /* OUT_INFO: DVI, sin audio */
#define ZXV_OUT_HDMI            2u      /* HDMI, con audio (los demás tipos, reservados) */
#define ZXV_OUT_SCREEN_SHIFT    16      /* OUT_INFO, bits 16-17: el monitor (por el HPD) */
#define ZXV_OUT_SCREEN_MASK     (3u << 16)
#define ZXV_OUT_SCREEN_UNKNOWN  (0u << 16)  /* no se sabe (la protoboard, o aún no se ha conectado) */
#define ZXV_OUT_SCREEN_NONE     (1u << 16)  /* estaba conectado y se ha quitado */
#define ZXV_OUT_SCREEN_ON       (2u << 16)  /* conectado */

#define ZXV_MODE0_W             256
#define ZXV_MODE0_H             192
#define ZXV_MODE1_W             512
#define ZXV_MODE1_H             384

/* Campo de juego virtual (spec §4.9): tamaño de la página en modo 0. */
#define ZXV_VSIZE_SCREEN        0   /* 256 x 192 */
#define ZXV_VSIZE_2X2           1   /* 512 x 384 */
#define ZXV_VSIZE_WIDE          2   /* 1024 x 192 */
#define ZXV_VSIZE_TALL          3   /* 256 x 768 */

#define ZXV_LINE_ENTRY_SIZE     8
#define ZXV_LT_BORDER           0   /* u8,  0xFF = global   */
#define ZXV_LT_REMAP            1   /* u8,  0xFF = global   */
#define ZXV_LT_SCROLL_X         2   /* u16, 0xFFFF = global */
#define ZXV_LT_SRC_LINE         4   /* u16, 0xFFFF = global */

/* --- Registros de entrada (spec §6.1) ----------------------------------- */
#define ZXV_I_KEYROWS           (ZXV_INPUT_REGS + 0x00u)   /* 8 x u8, 0 = pulsada */
#define ZXV_I_KEY_HEAD          (ZXV_INPUT_REGS + 0x08u)   /* u8, sistema   */
#define ZXV_I_KEY_TAIL          (ZXV_INPUT_REGS + 0x09u)   /* u8, programa  */
#define ZXV_I_KEYMAP            (ZXV_INPUT_REGS + 0x10u)   /* 32 x u8       */
#define ZXV_I_KEY_MODS          (ZXV_INPUT_REGS + 0x30u)   /* u8            */
#define ZXV_I_JOY0              (ZXV_INPUT_REGS + 0x40u)   /* u16           */
#define ZXV_I_JOY1              (ZXV_INPUT_REGS + 0x42u)   /* u16           */
#define ZXV_I_MOUSE_X           (ZXV_INPUT_REGS + 0x48u)   /* i16           */
#define ZXV_I_MOUSE_Y           (ZXV_INPUT_REGS + 0x4Au)   /* i16           */
#define ZXV_I_MOUSE_BTN         (ZXV_INPUT_REGS + 0x4Cu)   /* u8            */
#define ZXV_I_MOUSE_WHEEL       (ZXV_INPUT_REGS + 0x4Du)   /* i8            */
#define ZXV_I_KEY_FIFO          (ZXV_INPUT_REGS + 0x80u)   /* 64 x u16      */
#define ZXV_KEY_FIFO_LEN        64

#define ZXV_KEY_SPECIAL         0xF000u     /* 0xF000 | código HID */

#define ZXV_JOY_RIGHT           0x0001u
#define ZXV_JOY_LEFT            0x0002u
#define ZXV_JOY_DOWN            0x0004u
#define ZXV_JOY_UP              0x0008u
#define ZXV_JOY_FIRE1           0x0010u
#define ZXV_JOY_FIRE2           0x0020u
#define ZXV_JOY_FIRE3           0x0040u
#define ZXV_JOY_START           0x0080u
#define ZXV_JOY_SELECT          0x0100u
#define ZXV_JOY_CONNECTED       0x8000u

/* --- Registros de sonido (spec §5.2) ------------------------------------ */
#define ZXV_S_BEEPER            (ZXV_SOUND_REGS + 0x00u)   /* u8  */
#define ZXV_S_AY_STEREO         (ZXV_SOUND_REGS + 0x01u)   /* u8  */
#define ZXV_S_VOLUME            (ZXV_SOUND_REGS + 0x02u)   /* u8  */
#define ZXV_S_AFLAGS            (ZXV_SOUND_REGS + 0x03u)   /* u8  */
#define ZXV_S_AY_HEAD           (ZXV_SOUND_REGS + 0x04u)   /* u16, programa */
#define ZXV_S_AY_TAIL           (ZXV_SOUND_REGS + 0x06u)   /* u16, sistema  */
#define ZXV_S_BEEP_HEAD         (ZXV_SOUND_REGS + 0x08u)   /* u16, programa */
#define ZXV_S_BEEP_TAIL         (ZXV_SOUND_REGS + 0x0Au)   /* u16, sistema  */
#define ZXV_S_AY_REGS           (ZXV_SOUND_REGS + 0x10u)   /* 16 x u8       */
#define ZXV_S_AY2_REGS          (ZXV_SOUND_REGS + 0x20u)   /* 16 x u8, el segundo AY */
#define ZXV_AY_FIFO_LEN         256
#define ZXV_AY2                 16          /* sumado al registro: el segundo AY (TurboSound) */
#define ZXV_BEEP_QUEUE_LEN      256
#define ZXV_BEEP_LATENCY_US     2000    /* retraso fijo de la cola del beeper */
/* El dato de la cola del beeper (spec §5.4): el nivel del beeper o, con el
 * bit 31, una escritura en el AY que suena a su hora (reg 0-31, como en la
 * cola del AY). */
#define ZXV_EV_AY               0x80000000u
#define ZXV_EV_AY_WRITE(reg, val) (ZXV_EV_AY | ((uint32_t)(reg) & 0xFFu) << 8 | ((uint32_t)(val) & 0xFFu))
#define ZXV_AY_STEREO_MONO      0
#define ZXV_AY_STEREO_ABC       1
#define ZXV_AY_STEREO_ACB       2

/* --- Variables del sistema (spec §8.1) ---------------------------------- */
#define ZXV_SV_FRAMES           (ZXV_SYSVARS + 0x00u)
#define ZXV_SV_VERSION          (ZXV_SYSVARS + 0x04u)
#define ZXV_SV_FRAME_HOOK       (ZXV_SYSVARS + 0x08u)
#define ZXV_SV_RAMTOP           (ZXV_SYSVARS + 0x0Cu)
#define ZXV_SV_URAM_START       (ZXV_SYSVARS + 0x10u)
#define ZXV_SV_URAM_END         (ZXV_SYSVARS + 0x14u)
#define ZXV_SV_CPU_HZ           (ZXV_SYSVARS + 0x18u)
#define ZXV_SV_SYSFLAGS         (ZXV_SYSVARS + 0x1Cu)
#define ZXV_SV_LANG             (ZXV_SYSVARS + 0x20u)   /* u32: ZXV_LANG('e','s') */

/* El idioma de la máquina: su código ISO 639-1, dos letras minúsculas. */
#define ZXV_LANG(a, b)          ((uint32_t)(a) | (uint32_t)(b) << 8)
#define ZXV_LANG_EN             ZXV_LANG('e', 'n')
#define ZXV_LANG_ES             ZXV_LANG('e', 's')

#define ZXV_SYSF_LINK_BUSY      0x01u
#define ZXV_SYSF_BREAK          0x02u

/* --- Interfaz interna ROM <-> plataforma (spec §7.1) -------------------- */
#define ZXV_PIF_VERSION         (ZXV_PLATFORM_IF + 0x00u)  /* u32 */
#define ZXV_PIF_IRQ_ACK         (ZXV_PLATFORM_IF + 0x04u)  /* void (*)(void) */
#define ZXV_PIF_TIME_US         (ZXV_PLATFORM_IF + 0x08u)  /* u32  (*)(void) */
/* Versión 2: sistema de ficheros de la tarjeta SD (mismas firmas que la API). */
#define ZXV_PIF_FS_OPEN         (ZXV_PLATFORM_IF + 0x0Cu)
#define ZXV_PIF_FS_READ         (ZXV_PLATFORM_IF + 0x10u)
#define ZXV_PIF_FS_WRITE        (ZXV_PLATFORM_IF + 0x14u)
#define ZXV_PIF_FS_CLOSE        (ZXV_PLATFORM_IF + 0x18u)
#define ZXV_PIF_FS_SEEK         (ZXV_PLATFORM_IF + 0x1Cu)
#define ZXV_PIF_FS_STAT         (ZXV_PLATFORM_IF + 0x20u)
#define ZXV_PIF_FS_REMOVE       (ZXV_PLATFORM_IF + 0x24u)
#define ZXV_PIF_FS_RENAME       (ZXV_PLATFORM_IF + 0x28u)
#define ZXV_PIF_FS_MKDIR        (ZXV_PLATFORM_IF + 0x2Cu)
#define ZXV_PIF_FS_CHDIR        (ZXV_PLATFORM_IF + 0x30u)
#define ZXV_PIF_FS_GETCWD       (ZXV_PLATFORM_IF + 0x34u)
#define ZXV_PIF_FS_OPENDIR      (ZXV_PLATFORM_IF + 0x38u)
#define ZXV_PIF_FS_READDIR      (ZXV_PLATFORM_IF + 0x3Cu)
#define ZXV_PIF_FS_CLOSEDIR     (ZXV_PLATFORM_IF + 0x40u)
#define ZXV_PIF_VERSION_FS      2u
/* Versión 3: el reloj (mismas firmas que la API). */
#define ZXV_PIF_CLOCK_GET       (ZXV_PLATFORM_IF + 0x44u)
#define ZXV_PIF_CLOCK_SET       (ZXV_PLATFORM_IF + 0x48u)
#define ZXV_PIF_VERSION_CLOCK   3u
/* Versión 4: el idioma, que la plataforma guarda (u32 lang_get(void), que da
 * 0 si no sabe ninguno; int lang_set(u32), que da 0 o ZXV_E_INVAL). */
#define ZXV_PIF_LANG_GET        (ZXV_PLATFORM_IF + 0x4Cu)
#define ZXV_PIF_LANG_SET        (ZXV_PLATFORM_IF + 0x50u)
#define ZXV_PIF_VERSION_LANG    4u

/* --- Números de la API (spec §7.3) -------------------------------------- */
#define ZXV_API_VERSION         0
#define ZXV_API_WAIT_VBLANK     1
#define ZXV_API_SET_FRAME_HOOK  2
#define ZXV_API_TIME_US         3
#define ZXV_API_SET_MODE        4
#define ZXV_API_SET_PAGE        5
#define ZXV_API_CLS             6
#define ZXV_API_PLOT            7
#define ZXV_API_PRINT_CHAR      8
#define ZXV_API_PRINT_STR       9
#define ZXV_API_KEY_GET         10
#define ZXV_API_JOY_READ        11
#define ZXV_API_AY_WRITE        12
#define ZXV_API_BEEPER_EVENT    13
#define ZXV_API_FILE_OPEN       14
#define ZXV_API_FILE_READ       15
#define ZXV_API_FILE_WRITE      16
#define ZXV_API_FILE_CLOSE      17
#define ZXV_API_FILE_LOAD       18
#define ZXV_API_EXIT_TO_BASIC   19
#define ZXV_API_REPORT_ERROR    20
#define ZXV_API_FILE_SEEK       21
#define ZXV_API_FILE_STAT       22
#define ZXV_API_FILE_REMOVE     23
#define ZXV_API_FILE_RENAME     24
#define ZXV_API_DIR_MAKE        25
#define ZXV_API_DIR_CHANGE      26
#define ZXV_API_DIR_CURRENT     27
#define ZXV_API_DIR_OPEN        28
#define ZXV_API_DIR_READ        29
#define ZXV_API_DIR_CLOSE       30
#define ZXV_API_ZXV_EXEC        31

/* Gráficos para juegos (spec §7.8); 32-63 reservados. */
#define ZXV_API_GFX_TARGET      32
#define ZXV_API_GFX_DRAW_PAGE   33
#define ZXV_API_GFX_CLIP        34
#define ZXV_API_GFX_BLIT        35
#define ZXV_API_SPR_PREPARE     36
#define ZXV_API_SPR_DRAW        37
#define ZXV_API_SPR_COLLIDE     38
#define ZXV_API_MAP_DRAW        39
#define ZXV_API_MAP_CELL        40
#define ZXV_API_M7_DRAW         41
#define ZXV_API_M7_CAMERA       42
#define ZXV_API_GFX_LINE        43
#define ZXV_API_GFX_RECT        44
#define ZXV_API_GFX_TRI         45
#define ZXV_API_GFX_POLY        46
#define ZXV_API_GFX_CIRCLE      47
#define ZXV_API_CLOCK_GET       48
#define ZXV_API_CLOCK_SET       49
#define ZXV_API_LANG_SET        50
#define ZXV_API_AY_EVENT        51
#define ZXV_API_SPR_DRAW_SCALED 52
#define ZXV_API_TEXT_COLORS     53
#define ZXV_API_TEXT_AT         54

/* --- Reloj (spec §7.9) --------------------------------------------------- */
#define ZXV_CLOCK_SIZE          8
#define ZXV_CK_YEAR             0       /* u16, 2000-2099 */
#define ZXV_CK_MONTH            2       /* u8, 1-12 */
#define ZXV_CK_DAY              3       /* u8, 1-31 */
#define ZXV_CK_HOUR             4       /* u8, 0-23 */
#define ZXV_CK_MIN              5       /* u8, 0-59 */
#define ZXV_CK_SEC              6       /* u8, 0-59 */
#define ZXV_CK_WDAY             7       /* u8, 1 lunes ... 7 domingo (al leer) */

/* --- Ficheros (spec §7.7) ------------------------------------------------ */
#define ZXV_O_READ              0x01u
#define ZXV_O_WRITE             0x02u
#define ZXV_O_CREATE            0x04u   /* crear si no existe */
#define ZXV_O_TRUNC             0x08u   /* vaciar al abrir */
#define ZXV_O_APPEND            0x10u   /* posición al final al abrir */

#define ZXV_SEEK_SET            0
#define ZXV_SEEK_CUR            1
#define ZXV_SEEK_END            2

#define ZXV_NAME_MAX            63      /* caracteres por nombre */
#define ZXV_PATH_MAX            255     /* caracteres por ruta */
#define ZXV_MAX_FILES           8       /* ficheros abiertos a la vez */
#define ZXV_MAX_DIRS            4       /* carpetas abiertas a la vez */

/* Entrada de carpeta / resultado de stat (72 bytes). */
#define ZXV_DIRENT_SIZE         72
#define ZXV_DE_NAME             0       /* char[64], terminado en cero */
#define ZXV_DE_SIZE             64      /* u32 */
#define ZXV_DE_ATTR             68      /* u32 */
#define ZXV_ATTR_DIR            0x01u

/* Códigos de error (negativos). */
#define ZXV_OK                  0
#define ZXV_E_NOENT             (-1)    /* no existe */
#define ZXV_E_EXIST             (-2)    /* ya existe */
#define ZXV_E_NAME              (-3)    /* nombre o ruta no válidos */
#define ZXV_E_NOTDIR            (-4)    /* no es una carpeta */
#define ZXV_E_ISDIR             (-5)    /* es una carpeta */
#define ZXV_E_NOTEMPTY          (-6)    /* carpeta no vacía */
#define ZXV_E_BADF              (-7)    /* descriptor no válido */
#define ZXV_E_MFILE             (-8)    /* demasiados abiertos */
#define ZXV_E_ACCESS            (-9)    /* operación no permitida por el modo */
#define ZXV_E_FAULT             (-10)   /* buffer fuera de la memoria permitida */
#define ZXV_E_IO                (-11)   /* error de lectura o escritura */
#define ZXV_E_NOCARD            (-12)   /* no hay tarjeta */
#define ZXV_E_NOSPC             (-13)   /* tarjeta llena */
#define ZXV_E_FORMAT            (-14)   /* fichero .ZXV no válido */
#define ZXV_E_RANGE             (-15)   /* no cabe en la memoria de usuario */
#define ZXV_E_NOCLOCK           (-16)   /* el reloj no está en hora */
#define ZXV_E_INVAL             (-17)   /* valor no válido (una fecha que no existe) */

/* --- Ejecutables .ZXV (spec §9.2) ---------------------------------------- */
#define ZXV_HDR_SIZE            64
#define ZXV_HDR_MAGIC           0x3156585Au     /* "ZXV1" */
#define ZXV_HDR_PROFILE         1u
#define ZXV_H_MAGIC             0x00
#define ZXV_H_HDR_SIZE          0x04    /* u16 */
#define ZXV_H_PROFILE           0x06    /* u16 */
#define ZXV_H_LOAD              0x08
#define ZXV_H_ENTRY             0x0C
#define ZXV_H_IMAGE_SIZE        0x10
#define ZXV_H_BSS_SIZE          0x14
#define ZXV_H_STACK             0x18
#define ZXV_H_MODE              0x1C    /* u8 */
#define ZXV_H_FLAGS             0x1D    /* u8, reservado: 0 al escribir, se ignora al leer */
#define ZXV_H_CRC               0x20
#define ZXV_H_NAME              0x24    /* char[28] */

/* --- Temporización ------------------------------------------------------ */
#define ZXV_FRAME_HZ            50
#define ZXV_CPU_HZ_REF          144000000u

#endif /* ZXV_H */
