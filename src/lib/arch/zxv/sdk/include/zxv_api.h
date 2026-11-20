/*
 * zxv_api.h - Llamadas a la API de la ROM desde C (spec §7.3).
 */
#ifndef ZXV_API_H
#define ZXV_API_H

#include <stddef.h>
#include <stdint.h>

#include "zxv.h"

#define ZXV_API_FN(n, type) ((type)(*(volatile uint32_t *)(uintptr_t)(ZXV_API_TABLE + 4u * (n))))

typedef struct {
    char name[64];
    uint32_t size;
    uint32_t attr;
} zxv_dirent_t;

typedef struct {
    uint16_t year;                      /* 2000-2099 */
    uint8_t month, day, hour, min, sec;
    uint8_t wday;                       /* 1 lunes ... 7 domingo (al leer) */
} zxv_clock_t;

static inline uint32_t zxv_api_version(void) { return ZXV_API_FN(ZXV_API_VERSION, uint32_t (*)(void))(); }
static inline void zxv_wait_vblank(void) { ZXV_API_FN(ZXV_API_WAIT_VBLANK, void (*)(void))(); }
static inline void zxv_set_frame_hook(void (*fn)(void)) { ZXV_API_FN(ZXV_API_SET_FRAME_HOOK, void (*)(void (*)(void)))(fn); }
static inline uint32_t zxv_time_us(void) { return ZXV_API_FN(ZXV_API_TIME_US, uint32_t (*)(void))(); }
static inline void zxv_set_mode(int mode) { ZXV_API_FN(ZXV_API_SET_MODE, void (*)(int))(mode); }
static inline void zxv_set_page(int page) { ZXV_API_FN(ZXV_API_SET_PAGE, void (*)(int))(page); }
static inline void zxv_cls(int color) { ZXV_API_FN(ZXV_API_CLS, void (*)(int))(color); }
static inline void zxv_plot(int x, int y, int color) { ZXV_API_FN(ZXV_API_PLOT, void (*)(int, int, int))(x, y, color); }
static inline void zxv_print_char(int c) { ZXV_API_FN(ZXV_API_PRINT_CHAR, void (*)(int))(c); }
static inline void zxv_print_str(const char *s) { ZXV_API_FN(ZXV_API_PRINT_STR, void (*)(const char *))(s); }
/* Colores del texto (0-15) y posición del cursor (columna, fila) de print_char y print_str. */
static inline void zxv_text_colors(int ink, int paper) { ZXV_API_FN(ZXV_API_TEXT_COLORS, void (*)(int, int))(ink, paper); }
static inline void zxv_text_at(int col, int row) { ZXV_API_FN(ZXV_API_TEXT_AT, void (*)(int, int))(col, row); }
static inline int zxv_key_get(void) { return ZXV_API_FN(ZXV_API_KEY_GET, int (*)(void))(); }
static inline uint32_t zxv_joy_read(int n) { return ZXV_API_FN(ZXV_API_JOY_READ, uint32_t (*)(int))(n); }
/* reg: 0-15 el AY; ZXV_AY2 + 0-15, el segundo. */
static inline void zxv_ay_write(int reg, int val) { ZXV_API_FN(ZXV_API_AY_WRITE, void (*)(int, int))(reg, val); }
static inline void zxv_beeper_event(int level) { ZXV_API_FN(ZXV_API_BEEPER_EVENT, void (*)(int))(level); }
/* Una escritura en el AY que suena a la hora t_us (la de zxv_time_us) más
 * ZXV_BEEP_LATENCY_US; con la cola llena, espera. Para la música digitalizada. */
static inline void zxv_ay_event(uint32_t t_us, int reg, int val) { ZXV_API_FN(ZXV_API_AY_EVENT, void (*)(uint32_t, int, int))(t_us, reg, val); }
static inline void zxv_beeper_event_at(uint32_t t_us, int level) { ZXV_API_FN(ZXV_API_BEEPER_EVENT_AT, void (*)(uint32_t, int))(t_us, level); }

/* --- Los canales de muestras (spec §5.6) ---------------------------------- */

/* Un registro de un canal (ZXV_SMP_SLOT...): por la cola del AY, en orden con
 * las escrituras de los AY. Con zxv_ay_event(t_us, ZXV_SMP_REG(ch, reg), val),
 * a su hora. */
static inline void zxv_smp_write(int ch, int reg, int val) { zxv_ay_write((int)ZXV_SMP_REG(ch, reg), val); }

/* El paso para una muestra grabada a sample_hz (que suene a su altura). */
static inline uint32_t zxv_smp_step(uint32_t sample_hz) { return (uint32_t)(((uint64_t)sample_hz << 16) / 48000u); }

/* Sube len bytes (múltiplo de 4) de src a la memoria de sonido, desde dst, y
 * espera a que acabe (unas pocas imágenes): ZXV_OK, o ZXV_E_RANGE si no cabe
 * o src no es de la RAM del programa. */
static inline int zxv_smp_upload(uint32_t dst, const void *src, uint32_t len)
{
    volatile uint8_t *ctl = (volatile uint8_t *)(uintptr_t)ZXV_S_SMP_UPLOAD;
    while (*ctl == 1)
        zxv_wait_vblank();
    *(volatile uint32_t *)(uintptr_t)ZXV_S_SMP_SRC = (uint32_t)(uintptr_t)src;
    *(volatile uint32_t *)(uintptr_t)ZXV_S_SMP_DST = dst;
    *(volatile uint32_t *)(uintptr_t)ZXV_S_SMP_LEN = len;
    __asm__ volatile("fence w, w" ::: "memory");
    *ctl = 1;
    while (*ctl == 1)
        zxv_wait_vblank();
    return *ctl ? ZXV_E_RANGE : ZXV_OK;
}

/* La entrada slot de la tabla de muestras: dónde empieza (addr, en la memoria
 * de sonido), su formato (ZXV_SMP_PCM8 o ZXV_SMP_ADPCM), cuántas muestras tiene
 * y desde cuál vuelve a empezar el bucle (len o más: sin bucle). */
static inline int zxv_smp_define(int slot, uint32_t addr, int fmt, uint32_t len, uint32_t loop)
{
    uint32_t e[3] = { addr | (uint32_t)fmt << 24, len, loop };
    return zxv_smp_upload(ZXV_SMP_TABLE + (uint32_t)slot * ZXV_SMP_SLOT_SIZE, e, sizeof e);
}

/* Deja la muestra slot lista en el canal ch, sin arrancarla: con su paso
 * (zxv_smp_step), volumen (0-64) y panorama (0-255; 128 el centro), desde el
 * principio. La arranca zxv_smp_start. */
static inline void zxv_smp_prepare(int ch, int slot, uint32_t step, int vol, int pan)
{
    zxv_smp_write(ch, ZXV_SMP_SLOT, slot);
    zxv_smp_write(ch, ZXV_SMP_VOL, vol);
    zxv_smp_write(ch, ZXV_SMP_PAN, pan);
    zxv_smp_write(ch, ZXV_SMP_STEP, (int)(step & 0xFF));
    zxv_smp_write(ch, ZXV_SMP_STEP + 1, (int)(step >> 8 & 0xFF));
    zxv_smp_write(ch, ZXV_SMP_STEP + 2, (int)(step >> 16 & 0xFF));
    zxv_smp_write(ch, ZXV_SMP_OFFSET, 0);
}

/* Arranca a la vez, en la misma muestra, los canales de mask (el bit n, el
 * canal n): como ZXV_SMP_KEY en cada uno, pero sin que se separen, por
 * muchas escrituras que hiciera falta antes (los acordes, las filas de un
 * MOD). */
static inline void zxv_smp_start(uint32_t mask)
{
    if (mask >> 8 & 0x0F)
        zxv_ay_write((int)ZXV_SMP_KEYON_HI, (int)(mask >> 8 & 0x0F));
    zxv_ay_write((int)ZXV_SMP_KEYON, (int)(mask & 0xFF));
}

/* Toca la muestra slot en el canal ch, ya (zxv_smp_prepare y su KEY). */
static inline void zxv_smp_play(int ch, int slot, uint32_t step, int vol, int pan)
{
    zxv_smp_prepare(ch, slot, step, vol, pan);
    zxv_smp_write(ch, ZXV_SMP_CTRL, ZXV_SMP_KEY);
}

static inline void zxv_smp_stop(int ch) { zxv_smp_write(ch, ZXV_SMP_CTRL, ZXV_SMP_STOP); }

static inline int zxv_file_open(const char *name, uint32_t flags) { return ZXV_API_FN(ZXV_API_FILE_OPEN, int (*)(const char *, uint32_t))(name, flags); }
static inline int zxv_file_read(int f, void *buf, uint32_t n) { return ZXV_API_FN(ZXV_API_FILE_READ, int (*)(int, void *, uint32_t))(f, buf, n); }
static inline int zxv_file_write(int f, const void *buf, uint32_t n) { return ZXV_API_FN(ZXV_API_FILE_WRITE, int (*)(int, const void *, uint32_t))(f, buf, n); }
static inline int zxv_file_close(int f) { return ZXV_API_FN(ZXV_API_FILE_CLOSE, int (*)(int))(f); }
/* Carga el fichero entero en addr si cabe en max bytes (si no, ZXV_E_RANGE y no carga nada); devuelve su tamaño. */
static inline int zxv_file_load(const char *name, void *addr, uint32_t max) { return ZXV_API_FN(ZXV_API_FILE_LOAD, int (*)(const char *, void *, uint32_t))(name, addr, max); }
static inline int zxv_file_seek(int f, int32_t off, int whence) { return ZXV_API_FN(ZXV_API_FILE_SEEK, int (*)(int, int32_t, int))(f, off, whence); }
static inline int zxv_file_stat(const char *name, zxv_dirent_t *e) { return ZXV_API_FN(ZXV_API_FILE_STAT, int (*)(const char *, zxv_dirent_t *))(name, e); }
static inline int zxv_file_remove(const char *name) { return ZXV_API_FN(ZXV_API_FILE_REMOVE, int (*)(const char *))(name); }
static inline int zxv_file_rename(const char *from, const char *to) { return ZXV_API_FN(ZXV_API_FILE_RENAME, int (*)(const char *, const char *))(from, to); }
static inline int zxv_dir_make(const char *name) { return ZXV_API_FN(ZXV_API_DIR_MAKE, int (*)(const char *))(name); }
static inline int zxv_dir_change(const char *name) { return ZXV_API_FN(ZXV_API_DIR_CHANGE, int (*)(const char *))(name); }
static inline int zxv_dir_current(char *buf, uint32_t len) { return ZXV_API_FN(ZXV_API_DIR_CURRENT, int (*)(char *, uint32_t))(buf, len); }
static inline int zxv_dir_open(const char *name) { return ZXV_API_FN(ZXV_API_DIR_OPEN, int (*)(const char *))(name); }
static inline int zxv_dir_read(int d, zxv_dirent_t *e) { return ZXV_API_FN(ZXV_API_DIR_READ, int (*)(int, zxv_dirent_t *))(d, e); }
static inline int zxv_dir_close(int d) { return ZXV_API_FN(ZXV_API_DIR_CLOSE, int (*)(int))(d); }
static inline int zxv_exec(const char *name, int *result) { return ZXV_API_FN(ZXV_API_ZXV_EXEC, int (*)(const char *, int *))(name, result); }

static inline int zxv_clock_get(zxv_clock_t *c) { return ZXV_API_FN(ZXV_API_CLOCK_GET, int (*)(zxv_clock_t *))(c); }
static inline int zxv_clock_set(const zxv_clock_t *c) { return ZXV_API_FN(ZXV_API_CLOCK_SET, int (*)(const zxv_clock_t *))(c); }

/* El idioma de la máquina (spec §7.10): ZXV_LANG('e', 's'), ZXV_LANG_EN... */
static inline uint32_t zxv_lang_get(void) { return *(volatile uint32_t *)(uintptr_t)ZXV_SV_LANG; }
static inline int zxv_lang_set(uint32_t lang) { return ZXV_API_FN(ZXV_API_LANG_SET, int (*)(uint32_t))(lang); }

/* Vuelve al BASIC: con 0, el informe 0 OK; con otro código, W con ese número. */
static inline void zxv_exit_to_basic(int code) { ZXV_API_FN(ZXV_API_EXIT_TO_BASIC, void (*)(int))(code); }
/* Vuelve al BASIC con el informe W y este texto (UTF-8, hasta 47 bytes), y el código si no es 0. */
static inline void zxv_report_error(int code, const char *msg) { ZXV_API_FN(ZXV_API_REPORT_ERROR, void (*)(int, const char *))(code, msg); }

/* --- Ratón (spec §6.1) ---------------------------------------------------
 * Un puntero en píxeles que sigue a los contadores del ratón (X a la
 * derecha, Y hacia abajo), sin aceleración, dentro de [0, w) x [0, h). Es lo
 * que hace MOUSE en el BASIC. */
typedef struct {
    int x, y;                   /* el puntero */
    int wheel;                  /* la rueda acumulada */
    uint8_t buttons;            /* bits 0-2: izquierdo, derecho y central */
    uint16_t rx, ry;            /* los contadores en la última lectura */
    uint8_t rw;
} zxv_mouse_t;

static inline void zxv_mouse_init(zxv_mouse_t *p, int x, int y)
{
    p->x = x;
    p->y = y;
    p->wheel = 0;
    p->rx = *(volatile uint16_t *)(uintptr_t)ZXV_I_MOUSE_X;
    p->ry = *(volatile uint16_t *)(uintptr_t)ZXV_I_MOUSE_Y;
    p->rw = *(volatile uint8_t *)(uintptr_t)ZXV_I_MOUSE_WHEEL;
    p->buttons = *(volatile uint8_t *)(uintptr_t)ZXV_I_MOUSE_BTN & 7u;
}

static inline void zxv_mouse_update(zxv_mouse_t *p, int w, int h)
{
    uint16_t x = *(volatile uint16_t *)(uintptr_t)ZXV_I_MOUSE_X;
    uint16_t y = *(volatile uint16_t *)(uintptr_t)ZXV_I_MOUSE_Y;
    uint8_t wh = *(volatile uint8_t *)(uintptr_t)ZXV_I_MOUSE_WHEEL;
    p->x += (int16_t)(uint16_t)(x - p->rx);
    p->y += (int16_t)(uint16_t)(y - p->ry);
    p->wheel += (int8_t)(uint8_t)(wh - p->rw);
    p->rx = x;
    p->ry = y;
    p->rw = wh;
    p->buttons = *(volatile uint8_t *)(uintptr_t)ZXV_I_MOUSE_BTN & 7u;
    p->x = p->x < 0 ? 0 : p->x >= w ? w - 1 : p->x;
    p->y = p->y < 0 ? 0 : p->y >= h ? h - 1 : p->y;
}

#endif /* ZXV_API_H */
