/*
 * zxvbankcode.c - Los bancos de código (D109): lo que hacen los puentes que
 * genera tools/zxvbanks.py, y zxv_bank_preload. Lo enlaza esa herramienta;
 * las tablas (los bancos, sus zonas) y la pila de los puentes son suyas.
 *
 * Cada zona tiene un banco cargado (o ninguno) y la cuenta de las llamadas
 * que están dentro de él. Entrar en un banco que no está lo carga si su
 * zona está libre; si en ella hay otro en uso (se entró y aún no se ha
 * vuelto), el programa para con un error en vez de borrar el código al que
 * tiene que volver. Antes de cargar, la zona se llena de c.ebreak: un salto
 * perdido a lo que había (un puntero guardado a una función del banco de
 * antes) para enseguida, con su dirección, en vez de ejecutar otra cosa.
 *
 * Para depurar, la tabla de GDB (_ovly_table, con overlay auto) dice qué
 * banco está cargado; tras cada carga se llama a _ovly_debug_event, donde
 * GDB para un momento para mirarla. El emulador la lee también (el
 * perfilador y los nombres del depurador). MIT, como el SDK.
 */
#include <stdint.h>

#include "zxv.h"
#include "zxv_api.h"
#include "zxv_bank.h"

#define REG32(a)        (*(volatile uint32_t *)(uintptr_t)(a))
#define REGIONS_MAX     8
#define C_EBREAK        0x9002u

typedef struct {
    const char *name;
    uint32_t region;
} bank_t;

typedef struct {
    uint32_t start, end;
} region_t;

/* De zxvbanks.py (bridges.S). */
extern const uint32_t __zxvbank_nbanks, __zxvbank_nregions;
extern const bank_t __zxvbank_banks[];
extern const region_t __zxvbank_regions[];

/* La tabla de GDB (bridges.S): dónde va, lo que mide, dónde está en el ELF
 * y si está cargado. */
extern uint32_t _ovly_table[][4];

/* GDB pone aquí un punto de ruptura suyo para volver a leer la tabla. */
void __attribute__((noinline)) _ovly_debug_event(void)
{
    __asm__ volatile("");
}

/* De zxvbank.c. */
int __zxvbank_find(const char *name, uint8_t e[ZXV_BANK_ENTRY]);
int __zxvbank_read(int f, const uint8_t e[ZXV_BANK_ENTRY], void *dst);

static uint32_t loaded[REGIONS_MAX];    /* el banco + 1, o 0 */
static uint32_t depth[REGIONS_MAX];     /* llamadas dentro de él */

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* --- Los errores: el programa para con el mensaje -------------------------------- */

static char msg[80];                    /* lo que enseña el BASIC: 79 bytes (UTF-8) */
static int msg_n;

static void add(const char *s)
{
    while (*s && msg_n < (int)sizeof msg - 1)
        msg[msg_n++] = *s++;
    msg[msg_n] = 0;
}

static int spanish(void)
{
    return REG32(ZXV_SV_LANG) == ZXV_LANG_ES;
}

static const char *err_text(int r)
{
    int es = spanish();
    switch (r) {
    case ZXV_E_CHANGED: return es ? "el programa ha cambiado: vuelve a cargarlo" : "the program changed: load it again";
    case ZXV_E_FORMAT: return es ? "está dañado" : "it is damaged";
    case ZXV_E_NOENT: return es ? "no está en el fichero" : "it is not in the file";
    case ZXV_E_ACCESS: return es ? "la ROM no tiene bancos" : "the ROM has no banks";
    case ZXV_E_NOCARD: return es ? "no hay tarjeta" : "no card";
    case ZXV_E_OFFLINE: return es ? "EXT: sin servidor" : "no server for EXT:";
    default: return es ? "no se puede leer" : "cannot be read";
    }
}

static void __attribute__((noreturn)) stop(int code)
{
    zxv_report_error(code, msg);
    for (;;)
        ;
}

static void __attribute__((noreturn)) fail(int code, const char *bank, const char *what)
{
    msg_n = 0;
    add(spanish() ? "Banco " : "Bank ");
    add(bank);
    add(": ");
    add(what);
    stop(code);
}

void __attribute__((noreturn)) __zxvbank_overflow(void)
{
    msg_n = 0;
    add(spanish() ? "Demasiadas llamadas a bancos, una dentro de otra" : "Too many nested bank calls");
    stop(ZXV_E_RANGE);
}

/* --- Cargar ------------------------------------------------------------------------ */

static const char *why;                 /* por qué no se cargó, si no es un error de ficheros */

/* Carga el banco id en su zona (libre): 0, o un error (y why). */
static int load(uint32_t id)
{
    const bank_t *b = &__zxvbank_banks[id];
    const region_t *z = &__zxvbank_regions[b->region];
    uint8_t e[ZXV_BANK_ENTRY];
    why = 0;
    int f = __zxvbank_find(b->name, e);
    if (f < 0)
        return f;
    if (e[ZXV_BANK_E_KIND] != ZXV_BANK_CODE || e[ZXV_BANK_E_REGION] != b->region ||
        get32(e + ZXV_BANK_E_ADDR) != z->start || get32(e + ZXV_BANK_E_SIZE) > z->end - z->start)
        return ZXV_E_FORMAT;            /* no es lo que se enlazó */
    uint32_t hook = REG32(ZXV_SV_FRAME_HOOK);
    if (hook >= z->start && hook < z->end) {
        why = spanish() ? "el gancho del frame apunta a su zona" : "the frame hook points into its region";
        return ZXV_E_ACCESS;
    }
    if (loaded[b->region])
        _ovly_table[loaded[b->region] - 1][3] = 0;
    loaded[b->region] = 0;
    for (uint16_t *p = (uint16_t *)(uintptr_t)z->start; (uintptr_t)p < z->end; p++)
        *p = (uint16_t)C_EBREAK;
    int r = __zxvbank_read(f, e, (void *)(uintptr_t)z->start);
    if (r < 0) {
        for (uint16_t *p = (uint16_t *)(uintptr_t)z->start; (uintptr_t)p < z->end; p++)
            *p = (uint16_t)C_EBREAK;
        _ovly_debug_event();
        return r;
    }
    __asm__ volatile("fence.i" ::: "memory");
    loaded[b->region] = id + 1;
    _ovly_table[id][3] = 1;
    _ovly_debug_event();
    return 0;
}

/* --- Los puentes --------------------------------------------------------------------- */

void __zxvbank_enter(uint32_t id)
{
    const bank_t *b = &__zxvbank_banks[id];
    uint32_t r = b->region;
    if (loaded[r] == id + 1) {
        depth[r]++;
        return;
    }
    if (depth[r]) {
        msg_n = 0;
        add(spanish() ? "Banco " : "Bank ");
        add(__zxvbank_banks[loaded[r] - 1].name);
        add(spanish() ? " en uso: no se puede cargar " : " in use: cannot load ");
        add(b->name);
        add(spanish() ? " en su zona" : " in its region");
        stop(ZXV_E_ACCESS);
    }
    uint32_t mstatus;
    __asm__ volatile("csrr %0, mstatus" : "=r"(mstatus));
    if (!(mstatus & 8))
        fail(ZXV_E_ACCESS, b->name, spanish() ? "no se carga con las interrupciones quitadas"
                                              : "not loaded with interrupts off");
    int e = load(id);
    if (e < 0)
        fail(e, b->name, why ? why : err_text(e));
    depth[r] = 1;
}

void __zxvbank_leave(uint32_t id)
{
    uint32_t r = __zxvbank_banks[id].region;
    if (depth[r])
        depth[r]--;
}

/* --- zxv_bank_preload ------------------------------------------------------------- */

int zxv_bank_preload(const char *name)
{
    for (uint32_t id = 0; id < __zxvbank_nbanks; id++) {
        const char *a = __zxvbank_banks[id].name, *n = name;
        while (*a && *a == *n) {
            a++;
            n++;
        }
        if (*a || *n)
            continue;
        uint32_t r = __zxvbank_banks[id].region;
        if (loaded[r] == id + 1)
            return 0;
        if (depth[r])
            return ZXV_E_ACCESS;        /* hay otro en uso en su zona */
        return load(id);
    }
    return ZXV_E_NOENT;
}
