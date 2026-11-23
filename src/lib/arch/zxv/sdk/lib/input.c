/*
 * input.c - Teclado y mandos a la vez, para juegos (include/zxv_input.h).
 *
 * Opcional: añade $(SDK)/lib/input.c a tu programa.
 *
 * Lee el mapa de teclas mantenidas (ZXV_I_KEYMAP, un bit por código HID) y
 * el mando (ZXV_I_JOY0/1) directamente, sin llamadas a la ROM.
 */
#include "zxv_input.h"

#ifdef ZXV_LIB_HOST
/* Las pruebas en el PC (emulator/tests/sdklib/input_host.c) ponen lo que se
 * lee de la máquina. */
#include "zxv.h"
#else
#include "zxv_api.h"
#define ZXV_INPUT_KEYMAP ((const volatile uint8_t *)(uintptr_t)ZXV_I_KEYMAP)
#define ZXV_INPUT_JOY(n) (*(const volatile uint16_t *)(uintptr_t)((n) ? ZXV_I_JOY1 : ZXV_I_JOY0))
#define ZXV_INPUT_FLUSH() do { while (zxv_key_get() >= 0) {} } while (0)
#define ZXV_INPUT_LANG() zxv_lang_get()
#endif

/* Códigos HID (posiciones de un teclado de EE. UU.). */
enum {
    K_A = 0x04, K_B = 0x05, K_M = 0x10, K_N = 0x11, K_O = 0x12, K_P = 0x13, K_Q = 0x14, K_X = 0x1B, K_Z = 0x1D,
    K_ENTER = 0x28, K_ESC = 0x29, K_TAB = 0x2B, K_SPACE = 0x2C,
    K_RIGHT = 0x4F, K_LEFT = 0x50, K_DOWN = 0x51, K_UP = 0x52,
};

const zxv_input_map zxv_input_default = {
    .keys = {
        { K_RIGHT, K_P },           /* ZXV_IN_RIGHT */
        { K_LEFT, K_O },            /* LEFT */
        { K_DOWN, K_A },            /* DOWN */
        { K_UP, K_Q },              /* UP */
        { K_SPACE, K_M },           /* FIRE */
        { K_N, K_Z },               /* FIRE2 */
        { K_B, K_X },               /* FIRE3 */
        { K_ENTER },                /* START */
        { K_TAB },                  /* SELECT */
        { K_ESC },                  /* ESC */
    },
    .joy = { ZXV_JOY_RIGHT, ZXV_JOY_LEFT, ZXV_JOY_DOWN, ZXV_JOY_UP, ZXV_JOY_FIRE1, ZXV_JOY_FIRE2, ZXV_JOY_FIRE3,
             ZXV_JOY_START, ZXV_JOY_SELECT },
    .pad = 0,
    .flags = 0,
};

int zxv_input_key_down(int hid)
{
    if (hid < 0 || hid > 255)
        return 0;
    return (ZXV_INPUT_KEYMAP[hid >> 3] >> (hid & 7)) & 1;
}

void zxv_input_init(zxv_input *in, const zxv_input_map *map)
{
    in->map = map ? map : &zxv_input_default;
    in->held = in->pressed = in->released = 0;
    for (int i = 0; i < ZXV_IN_ACTIONS; i++)
        in->frames[i] = 0;
}

void zxv_input_update(zxv_input *in)
{
    const zxv_input_map *m = in->map;
    uint16_t joy = 0;
    if (m->pad == 0 || m->pad == 1) {
        joy = ZXV_INPUT_JOY(m->pad);
        if (!(joy & ZXV_JOY_CONNECTED))
            joy = 0;
    }
    uint16_t now = 0;
    for (int a = 0; a < ZXV_IN_ACTIONS; a++) {
        int on = (joy & m->joy[a]) != 0;
        for (int k = 0; k < ZXV_IN_KEYS && !on; k++)
            on = m->keys[a][k] && zxv_input_key_down(m->keys[a][k]);
        if (on) {
            now |= (uint16_t)(1u << a);
            if (in->frames[a] < 0xFFFF)
                in->frames[a]++;
        } else {
            in->frames[a] = 0;
        }
    }
    in->pressed = now & (uint16_t)~in->held;
    in->released = in->held & (uint16_t)~now;
    in->held = now;
    if (!(m->flags & ZXV_IN_KEEP_QUEUE))
        ZXV_INPUT_FLUSH();
}

int zxv_input_repeat(const zxv_input *in, uint16_t actions, int delay, int rate)
{
    if (rate < 1)
        rate = 1;
    for (int a = 0; a < ZXV_IN_ACTIONS; a++) {
        if (!(actions & (1u << a)))
            continue;
        int f = in->frames[a];
        if (f == 1 || (f > delay && (f - delay) % rate == 0))
            return 1;
    }
    return 0;
}

int zxv_input_new_key(void)
{
    static uint8_t prev[32];
    static int armed;
    int found = -1;
    for (int i = 0; i < 32; i++) {
        uint8_t now = ZXV_INPUT_KEYMAP[i];
        uint8_t fresh = (uint8_t)(now & ~prev[i]);
        if (armed && fresh && found < 0)
            for (int b = 0; b < 8; b++)
                if (fresh & (1u << b)) {
                    found = i * 8 + b;
                    break;
                }
        prev[i] = now;
    }
    armed = 1;
    return found;
}

/* --- Los nombres --------------------------------------------------------------- */

static const struct {
    uint8_t hid;
    const char *en, *es;
} names[] = {
    { 0x28, "Enter", "Intro" },
    { 0x29, "Esc", "Esc" },
    { 0x2A, "Backspace", "Retroceso" },
    { 0x2B, "Tab", "Tab" },
    { 0x2C, "Space", "Espacio" },
    { 0x39, "Caps Lock", "Bloq Mayús" },
    { 0x46, "Print Screen", "Impr Pant" },
    { 0x47, "Scroll Lock", "Bloq Despl" },
    { 0x48, "Pause", "Pausa" },
    { 0x49, "Insert", "Insertar" },
    { 0x4A, "Home", "Inicio" },
    { 0x4B, "Page Up", "Re Pág" },
    { 0x4C, "Delete", "Supr" },
    { 0x4D, "End", "Fin" },
    { 0x4E, "Page Down", "Av Pág" },
    { 0x4F, "Right", "Cursor derecha" },
    { 0x50, "Left", "Cursor izquierda" },
    { 0x51, "Down", "Cursor abajo" },
    { 0x52, "Up", "Cursor arriba" },
    { 0x58, "Keypad Enter", "Intro numérico" },
    { 0xE0, "Left Ctrl", "Ctrl izquierdo" },
    { 0xE1, "Left Shift", "Mayús izquierda" },
    { 0xE2, "Left Alt", "Alt" },
    { 0xE3, "Left GUI", "Windows izquierda" },
    { 0xE4, "Right Ctrl", "Ctrl derecho" },
    { 0xE5, "Right Shift", "Mayús derecha" },
    { 0xE6, "Right Alt", "Alt Gr" },
    { 0xE7, "Right GUI", "Windows derecha" },
};

const char *zxv_input_key_name(int hid)
{
    static char buf[24];
    int es = ZXV_INPUT_LANG() == ZXV_LANG_ES;
    if (hid >= 0x04 && hid <= 0x1D) {                   /* A-Z */
        buf[0] = (char)('A' + hid - 0x04);
        buf[1] = 0;
        return buf;
    }
    if (hid >= 0x1E && hid <= 0x27) {                   /* 1-9, 0 */
        buf[0] = hid == 0x27 ? '0' : (char)('1' + hid - 0x1E);
        buf[1] = 0;
        return buf;
    }
    if (hid >= 0x3A && hid <= 0x45) {                   /* F1-F12 */
        int f = hid - 0x3A + 1;
        buf[0] = 'F';
        buf[1] = (char)(f < 10 ? '0' + f : '1');
        buf[2] = (char)(f < 10 ? 0 : '0' + f - 10);
        buf[3] = 0;
        return buf;
    }
    if (hid >= 0x59 && hid <= 0x62) {                   /* 1-9, 0 del teclado numérico */
        const char *p = es ? "Numérico " : "Keypad ";
        int n = 0;
        while (*p)
            buf[n++] = *p++;
        buf[n++] = hid == 0x62 ? '0' : (char)('1' + hid - 0x59);
        buf[n] = 0;
        return buf;
    }
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (names[i].hid == hid)
            return es ? names[i].es : names[i].en;
    const char *p = es ? "Tecla " : "Key ";
    int n = 0;
    while (*p)
        buf[n++] = *p++;
    unsigned v = (unsigned)hid & 0xFF;
    buf[n++] = "0123456789ABCDEF"[v >> 4];
    buf[n++] = "0123456789ABCDEF"[v & 15];
    buf[n] = 0;
    return buf;
}
