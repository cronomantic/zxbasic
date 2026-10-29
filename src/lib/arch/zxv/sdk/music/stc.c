/*
 * stc.c - El reproductor de STC: módulos compilados de Sound Tracker 1.x.
 *
 * La cabecera (27 bytes): el tempo, dónde están las posiciones, los
 * ornamentos y los patrones, 18 caracteres de nombre y el tamaño. Luego las
 * muestras, de 99 bytes: su número, 32 líneas de 3 bytes, la vuelta y el
 * largo del tramo que se repite. Una línea:
 *   b0: bits 7-4 la parte alta del desplazamiento del tono, 3-0 el volumen;
 *   b1: bit 7 sin ruido, bit 6 sin tono, bit 5 el signo del desplazamiento,
 *       bits 4-0 el ruido;
 *   b2: la parte baja del desplazamiento.
 * Las posiciones: cuántas hay menos una y, por posición, el patrón (desde 1)
 * y una transposición. Los ornamentos (33 bytes): su número y 32
 * desplazamientos de nota. Los patrones: su número (desde 1) y los tres
 * canales; el canal A acaba en 0xFF. Las muestras, los ornamentos y los
 * patrones se buscan por su número.
 *
 * Como en zxtune: cada patrón empieza con los saltos de fila a 0 y dura al
 * menos 5 filas y como mucho 64, y los registros que un paso no toca se
 * quedan como estaban.
 */
#include "music_int.h"

extern const uint16_t stc_notes[96];
#define NOTES   stc_notes

#define B(i)    mus_b(s->m, s->n, i)
#define W(i)    mus_w(s->m, s->n, i)
#define STUB    0xFFFFu                 /* sin muestra u ornamento: todo a 0 */

/* El primer objeto con ese número de una tabla de 16 (o STUB). */
static uint16_t find(const stc_t *s, uint32_t base, uint32_t size, int num)
{
    for (int i = 0; i < 16; i++) {
        uint32_t at = base + (uint32_t)i * size;
        if (at + size > s->n)
            break;
        if (s->m[at] == num)
            return (uint16_t)(at + 1);
    }
    return STUB;
}

static void set_position(stc_t *s)
{
    uint16_t p = W(1);
    int pat = B(p + 1 + s->pos * 2);
    s->transpose = (int8_t)B(p + 2 + s->pos * 2);
    uint16_t at = STUB;
    for (int i = 0; i < 32; i++) {
        uint32_t e = W(5) + i * 7u;
        uint8_t num = B(e);
        if (num < 1 || num > 32)
            break;
        if (num == pat) {
            at = (uint16_t)e;
            break;
        }
    }
    for (int i = 0; i < 3; i++) {
        s->ch[i].addr = at == STUB ? (uint16_t)s->n : W(at + 1 + i * 2);
        s->ch[i].skip = 0;
        s->ch[i].skip_cnt = 0;
    }
    s->row = 0;
    s->ended = 0;
    s->end_row = 0;
}

/* Las zonas de la cabecera: su tamaño es lo que hay hasta la siguiente. */
static uint32_t area(const uint8_t *m, uint32_t n, uint32_t at)
{
    uint32_t a[4] = { m[1] | m[2] << 8, m[3] | m[4] << 8, m[5] | m[6] << 8, n };
    uint32_t next = 0xFFFFFFFFu;
    for (int i = 0; i < 4; i++)
        if (a[i] > at && a[i] < next)
            next = a[i];
    return next - at;
}

int stc_init(stc_t *s, const uint8_t *m, uint32_t n)
{
    if (n < 128 || m[0] == 0 || m[0] > 32)
        return -1;
    uint32_t pos = m[1] | m[2] << 8, orn = m[3] | m[4] << 8, pat = m[5] | m[6] << 8;
    if (pos < 27 || orn < 27 || pat < 27 || pos >= n || orn + 33 > n || pat + 7 > n)
        return -1;
    /* Las muestras, a partir de 27, y los ornamentos, de tamaño exacto (el
     * primer ornamento puede pisar el final de lo de antes). */
    uint32_t smp = area(m, n, 27), ornsz = area(m, n, orn);
    int overlap = 27 < orn && 27 + 99 > orn && 27 + 99 < orn + 33;
    if ((smp == 0 || smp % 99) && !overlap)
        return -1;
    if (ornsz == 0 || ornsz % 33)
        return -1;
    uint32_t need = 3 + 2u * m[pos], have = area(m, n, pos);
    if (have != need && !(pos < orn && pos + need > orn && pos + need < orn + 33))
        return -1;
    for (int i = 0; i <= m[pos]; i++) {
        uint8_t p = mus_b(m, n, pos + 1 + i * 2);
        if (p < 1 || p > 32)
            return -1;
    }

    uint8_t *p = (uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; i++)
        p[i] = 0;
    s->m = m;
    s->n = n;
    s->delay = m[0];
    s->npos = m[pos] + 1;
    s->delay_cnt = 1;
    set_position(s);
    for (int i = 0; i < 3; i++) {
        s->ch[i].smp = STUB;
        s->ch[i].orn = STUB;
        s->ch[i].count = -1;
    }
    return 0;
}

/* Las órdenes de una fila de un canal. */
static void pattern(stc_t *s, stc_chan_t *c)
{
    uint16_t a = c->addr;
    while (a < s->n) {
        uint8_t v = B(a++);
        if (v <= 0x5F) {
            c->count = 32;
            c->note = v;
            c->pos = 0;
            break;
        } else if (v <= 0x6F) {
            c->smp = find(s, 27, 99, v - 0x60);
        } else if (v <= 0x7F) {
            c->env = 0;
            c->orn = find(s, W(3), 33, v - 0x70);
        } else if (v == 0x80) {
            c->count = -1;
            break;
        } else if (v == 0x81) {
            break;
        } else if (v == 0x82) {
            c->orn = find(s, W(3), 33, 0);
            c->env = 0;
        } else if (v <= 0x8E) {
            c->orn = find(s, W(3), 33, 0);
            s->env_type = v - 0x80;
            s->env_tone = B(a++);
            c->env = 1;
        } else {
            c->skip = (uint8_t)(v - 0xA1);
        }
    }
    c->addr = a;
}

/* El cursor de la muestra un paso adelante: 32 líneas y, si tiene vuelta,
 * su tramo una y otra vez. */
static void next(const stc_t *s, const stc_chan_t *c, int16_t *count, uint8_t *pos)
{
    if (*count < 0)
        return;
    --*count;
    *pos = (*pos + 1) & 31;
    if (*count == 0) {
        uint8_t loop = c->smp == STUB ? 0 : mus_b(s->m, s->n, c->smp + 96);
        if (loop) {
            uint8_t len = mus_b(s->m, s->n, c->smp + 97);
            int limit = loop > 32 ? 32 : loop;
            int end = loop + len > 32 ? 32 : loop + len;
            *pos = loop & 31;
            *count = (int16_t)(end - limit + 1);
        } else {
            *count = -1;
        }
    }
}

static void channel(stc_t *s, stc_chan_t *c, int ch)
{
    int16_t count = c->count;
    uint8_t pos = c->pos;
    next(s, c, &count, &pos);
    if (count < 0) {
        s->r[8 + ch] = 0;
        return;
    }
    int line = (pos - 1) & 31;
    uint8_t b0 = 0, b1 = 0xC0, b2 = 0;
    if (c->smp != STUB) {
        b0 = B(c->smp + line * 3);
        b1 = B(c->smp + line * 3 + 1);
        b2 = B(c->smp + line * 3 + 2);
    }
    int eff = (b0 & 0xF0) * 16 + b2;
    if (!(b1 & 0x20))
        eff = -eff;
    int note = c->note + s->transpose + (c->orn != STUB ? (int8_t)B(c->orn + line) : 0);
    uint16_t tone = (NOTES[note < 0 ? 0 : note > 95 ? 95 : note] + eff) & 0xFFF;
    s->r[8 + ch] = (b0 & 15) | (c->env ? 16 : 0);
    s->r[ch * 2] = tone & 0xFF;
    s->r[ch * 2 + 1] = tone >> 8;
    if (b1 & 0x40)
        s->r[7] |= 1 << ch;
    if (!(b1 & 0x80))
        s->r[6] = b1 & 31;
    else
        s->r[7] |= 8 << ch;
}

void stc_step(stc_t *s, mus_ay_t *ay)
{
    if (--s->delay_cnt == 0) {
        stc_chan_t *c = s->ch;
        /* El patrón acaba en el 0xFF del canal A (o a las 64 filas), y dura
         * al menos 5 filas. */
        for (;;) {
            if (!s->ended && (s->row >= 64 || (c[0].skip_cnt == 0 && B(c[0].addr) == 0xFF))) {
                s->ended = 1;
                s->end_row = s->row;
            }
            int len = !s->ended ? 64 : s->end_row < 5 ? 5 : s->end_row;
            if (s->row < len)
                break;
            if (++s->pos >= s->npos) {
                s->pos = 0;
                s->looped++;
            }
            set_position(s);
        }
        if (!s->ended) {
            for (int i = 0; i < 3; i++) {
                if (c[i].skip_cnt) {
                    c[i].skip_cnt--;
                } else {
                    pattern(s, &c[i]);
                    c[i].skip_cnt = c[i].skip;
                }
            }
        }
        s->row++;
        s->delay_cnt = s->delay;
    }

    s->r[7] = 0;
    for (int i = 0; i < 3; i++)
        channel(s, &s->ch[i], i);
    ay->r13 = 0;
    if (s->env_type) {
        s->r[13] = s->env_type;
        s->r[11] = s->env_tone;
        s->r[12] = 0;
        ay->r13 = 1;
    }
    for (int i = 0; i < 14; i++)
        ay->r[i] = s->r[i];
    for (int i = 0; i < 3; i++) {
        stc_chan_t *c = &s->ch[i];
        next(s, c, &c->count, &c->pos);
        if (c->count >= 0 && c->env) {
            c->env = 2;
            s->env_type = 0;
        }
    }
}
