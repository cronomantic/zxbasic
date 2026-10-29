/*
 * pt2.c - El reproductor de PT2: módulos de Pro Tracker 2.
 *
 * La cabecera: el tempo, el número de posiciones y la de vuelta; en 3 las
 * 32 muestras, en 67 los 16 ornamentos, en 99 los patrones (tres punteros
 * por patrón), en 101 el nombre (30 caracteres) y en 131 la lista de
 * posiciones (el número de patrón, hasta 0xFF).
 *
 * Una muestra: largo, vuelta y 3 bytes por línea:
 *   b0: bits 7-3 el ruido, bit 2 el signo del desplazamiento del tono, bit 1
 *       sin tono, bit 0 sin ruido;
 *   b1: bits 7-4 el volumen, bits 3-0 la parte alta del desplazamiento;
 *   b2: la parte baja del desplazamiento.
 * Un ornamento: largo, vuelta y un desplazamiento de nota por línea.
 *
 * Como en zxtune: cada patrón empieza con los saltos de fila a 0, y los
 * registros que un paso no toca se quedan como estaban.
 */
#include "music_int.h"

extern const uint16_t pt3_notes[7][96];
#define NOTES   pt3_notes[2]            /* la tabla de Sound Tracker */

#define B(i)    mus_b(s->m, s->n, i)
#define W(i)    mus_w(s->m, s->n, i)
#define NONE    0xFF

static int freq(int note)
{
    return NOTES[note < 0 ? 0 : note > 95 ? 95 : note];
}

static void set_sample(pt2_t *s, pt2_chan_t *c, int n)
{
    uint16_t p = W(3 + (n & 31) * 2);
    c->smp_len = B(p);
    c->smp_loop = B(p + 1);
    c->smp = p + 2;
    c->pos_smp = 0;
}

static void set_ornament(pt2_t *s, pt2_chan_t *c, int n)
{
    uint16_t p = W(67 + (n & 15) * 2);
    c->orn_len = B(p);
    c->orn_loop = B(p + 1);
    c->orn = p + 2;
    c->pos_orn = 0;
}

static void set_position(pt2_t *s)
{
    uint16_t pat = W(99) + B(131 + s->pos) * 6;
    for (int i = 0; i < 3; i++) {
        s->ch[i].addr = W(pat + i * 2);
        s->ch[i].skip = 0;
        s->ch[i].skip_cnt = 0;
    }
}

int pt2_init(pt2_t *s, const uint8_t *m, uint32_t n)
{
    /* Sin firma: lo que tiene que cumplir la cabecera. */
    if (n < 133 || m[0] < 2 || m[1] == 0 || m[2] >= m[1] || m[131] == 0xFF)
        return -1;
    uint32_t pat = m[99] | m[100] << 8;
    if (pat < 132 || pat >= n)
        return -1;
    for (int i = 0; i < m[1]; i++)
        if (131u + i >= n || m[131 + i] > 31)
            return -1;
    if (131u + m[1] >= n || m[131 + m[1]] != 0xFF)
        return -1;

    uint8_t *p = (uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; i++)
        p[i] = 0;
    s->m = m;
    s->n = n;
    s->delay = m[0];
    s->npos = m[1];
    s->loop = m[2];
    s->delay_cnt = 1;
    s->r[7] = 0;
    set_position(s);
    for (int i = 0; i < 3; i++) {
        pt2_chan_t *c = &s->ch[i];
        set_sample(s, c, 1);
        set_ornament(s, c, 0);
        c->volume = 15;
        c->target = NONE;
    }
    return 0;
}

/* Las órdenes de una fila de un canal. */
static void pattern(pt2_t *s, pt2_chan_t *c)
{
    int note = -1, rest = 0, smp = -1, orn = -1, vol = -1;
    int env = -1, env_off = 0, noise = -1000, gliss_off = 0;
    int gliss = 0, gl_step = 0, gl_note = 0;
    uint16_t env_tone = 0, gl_limit = 0;
    uint16_t a = c->addr;
    while (a < s->n) {
        uint8_t v = B(a++);
        if (v == 0) {
            continue;
        } else if (v >= 0xE1) {
            smp = v - 0xE0;
        } else if (v == 0xE0) {
            rest = 1;
            break;
        } else if (v >= 0x80) {
            note = v - 0x80;
            break;
        } else if (v == 0x7F) {
            env_off = 1;
        } else if (v >= 0x71) {
            env = v - 0x70;
            env_tone = W(a);
            a += 2;
        } else if (v == 0x70) {
            break;
        } else if (v >= 0x60) {
            orn = v - 0x60;
        } else if (v >= 0x20) {
            c->skip = v - 0x20;
        } else if (v >= 0x10) {
            vol = v - 0x10;
        } else if (v == 0x0F) {
            s->delay = B(a++);
        } else if (v == 0x0E) {
            gliss = 1;
            gl_step = (int8_t)B(a++);
        } else if (v == 0x0D) {
            gl_note = 1;
            gl_step = (int8_t)B(a);
            gl_limit = W(a + 1);
            a += 3;
        } else if (v == 0x0C) {
            gliss_off = 1;
        } else {
            noise = (int8_t)B(a++);
        }
    }
    c->addr = a;

    /* Como el reproductor de zxtune: la nota (o el silencio), la muestra,
     * el ornamento, el volumen y luego las órdenes. Con el portamento, la
     * nota es su destino y la que suena no cambia. */
    int target = gl_limit;
    if (note >= 0 && gl_note) {
        target = note;
        note = -1;
        rest = -1;                      /* suena, con la nota de antes */
    }
    if (note >= 0 || rest) {
        c->enabled = rest <= 0;
        if (!c->enabled) {
            c->sliding = c->glissade = 0;
            c->target = NONE;
        }
        c->pos_smp = c->pos_orn = 0;
    }
    if (note >= 0) {
        c->note = note;
        c->sliding = c->glissade = 0;
        c->target = NONE;
    }
    if (smp >= 0)
        set_sample(s, c, smp);
    if (orn >= 0)
        set_ornament(s, c, orn);
    if (vol >= 0)
        c->volume = vol;
    if (env >= 0) {
        s->r[13] = env;
        s->r13_write = 1;
        s->r[11] = env_tone & 0xFF;
        s->r[12] = env_tone >> 8;
        c->env_on = 1;
    }
    if (env_off)
        c->env_on = 0;
    if (noise != -1000)
        c->noise_add = noise;
    if (gl_note) {
        c->sliding = 0;
        c->glissade = gl_step;
        c->target = target > 95 ? 95 : target;
    }
    if (gliss) {
        c->glissade = gl_step;
        c->target = NONE;
    }
    if (gliss_off)
        c->glissade = 0;
}

static void channel(pt2_t *s, pt2_chan_t *c, int ch)
{
    if (!c->enabled) {
        s->r[8 + ch] = 0;
        return;
    }
    uint16_t l = c->smp + c->pos_smp * 3;
    int in = c->pos_smp < c->smp_len;
    uint8_t b0 = in ? B(l) : 0x03, b1 = in ? B(l + 1) : 0, b2 = in ? B(l + 2) : 0;
    int vib = (b1 & 15) << 8 | b2;
    if (!(b0 & 4))
        vib = -vib;
    int note = c->note + (c->pos_orn < c->orn_len ? (int8_t)B(c->orn + c->pos_orn) : 0);
    uint16_t tone = (freq(note) + c->sliding + vib) & 0xFFF;
    s->r[ch * 2] = tone & 0xFF;
    s->r[ch * 2 + 1] = tone >> 8;
    if (b0 & 2)
        s->r[7] |= 1 << ch;
    int v = c->volume;
    s->r[8 + ch] = (uint8_t)((v * 17 + (v > 7)) * (b1 >> 4) / 256) | (c->env_on ? 16 : 0);
    if (!(b0 & 1))
        s->r[6] = ((b0 >> 3) + c->noise_add) & 31;
    else
        s->r[7] |= 8 << ch;

    if (c->target != NONE) {
        int range = freq(c->target) - freq(c->note) - (c->sliding + c->glissade);
        if ((c->glissade > 0 && range <= 0) || (c->glissade < 0 && range >= 0)) {
            c->note = c->target;
            c->target = NONE;
            c->sliding = c->glissade = 0;
        }
    }
    c->sliding += c->glissade;
    if (++c->pos_smp >= c->smp_len)
        c->pos_smp = c->smp_loop;
    if (++c->pos_orn >= c->orn_len)
        c->pos_orn = c->orn_loop;
}

void pt2_step(pt2_t *s, mus_ay_t *ay)
{
    s->r13_write = 0;
    if (--s->delay_cnt == 0) {
        pt2_chan_t *c = s->ch;
        if (c[0].skip_cnt == 0 && B(c[0].addr) == 0) {     /* fin del patrón */
            if (++s->pos >= s->npos) {
                s->pos = s->loop;
                s->looped++;
            }
            set_position(s);
        }
        for (int i = 0; i < 3; i++) {
            if (c[i].skip_cnt) {
                c[i].skip_cnt--;
            } else {
                pattern(s, &c[i]);
                c[i].skip_cnt = c[i].skip;
            }
        }
        s->delay_cnt = s->delay;
    }
    s->r[7] = 0;
    for (int i = 0; i < 3; i++)
        channel(s, &s->ch[i], i);
    for (int i = 0; i < 14; i++)
        ay->r[i] = s->r[i];
    ay->r13 = s->r13_write;
}
