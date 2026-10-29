/*
 * asc.c - El reproductor de ASC: módulos de ASC Sound Master 0.x y 1.x.
 *
 * La cabecera: el tempo, la posición de vuelta (no en la 0.x), dónde están
 * los patrones, las muestras y los ornamentos, el número de posiciones y la
 * lista de posiciones (el número de patrón). Puede seguir "ASM COMPILATION
 * OF ", el título (20), " BY " y el autor (20). Los patrones: tres
 * desplazamientos (desde el principio de los patrones) por patrón; el canal
 * A acaba en 0xFF. Las muestras y los ornamentos: 32 desplazamientos (desde
 * su lista) y sus líneas, hasta la que lleva el bit de final:
 *
 *   muestra (3 bytes):  B E F aaaaa (vuelta, fin del tramo, final y lo que
 *                       se suma al ruido o a la envolvente, con signo), el
 *                       desvío del tono (con signo), y LLLL n CC t (el
 *                       volumen, sin ruido, la orden: 1 envolvente, 2 baja y
 *                       3 sube el volumen, y sin tono)
 *   ornamento (2):      B E F ooooo (el desplazamiento del ruido) y el de la
 *                       nota, los dos con signo
 *
 * Los desvíos se acumulan: el tono, la nota del ornamento, el ruido y el
 * volumen avanzan línea a línea desde la nota. Como en zxtune: cada patrón
 * empieza con los saltos de fila a 0 y sin envolvente en el canal, y los
 * registros que un paso no toca se quedan como estaban.
 */
#include "music_int.h"

extern const uint16_t asc_notes[86];

#define B(i)    mus_b(s->m, s->n, i)
#define W(i)    mus_w(s->m, s->n, i)
#define NONE    0xFF

static int freq(int note)
{
    return asc_notes[note < 0 ? 0 : note > 85 ? 85 : note];
}

/* Una muestra (líneas de 3 bytes, hasta 150) o un ornamento (de 2, hasta
 * 30): su principio, su largo, la vuelta y el final del tramo. */
static void object(const asc_t *s, uint32_t list, int num, int line, int max, asc_obj_t *o)
{
    uint32_t at = list + mus_w(s->m, s->n, list + num * 2);
    o->addr = (uint16_t)at;
    o->len = o->loop = o->limit = 0;
    for (int i = 0; i < max && at + (uint32_t)(i + 1) * line <= s->n; i++) {
        uint8_t f = s->m[at + i * line];
        o->len = (uint8_t)(i + 1);
        if (f & 0x80)
            o->loop = (uint8_t)i;
        if (f & 0x40)
            o->limit = (uint8_t)i;
        if (f & 0x20)
            break;
    }
}

static void set_position(asc_t *s)
{
    uint32_t pat = s->pats + B(s->hdr + s->pos) * 6u;
    for (int i = 0; i < 3; i++) {
        asc_chan_t *c = &s->ch[i];
        c->addr = (uint16_t)(s->pats + W(pat + i * 2));
        c->skip = c->skip_cnt = 0;
        c->env_parse = 0;
        c->base_noise = 0;
    }
    s->row = 0;
    s->ended = 0;
}

int asc_init(asc_t *s, const uint8_t *m, uint32_t n)
{
    if (n < 256)
        return -1;
    /* La 1.x tiene la vuelta en el byte 1; se reconoce porque los patrones
     * empiezan justo tras la cabecera (o tras el nombre). */
    int v1 = -1;
    for (int v = 1; v >= 0 && v1 < 0; v--) {
        uint32_t hdr = 8u + v, len = m[hdr - 1], pats = m[1 + v] | m[2 + v] << 8;
        if (len >= 1 && len <= 100 && (pats == hdr + len || pats == hdr + len + 63))
            v1 = v;
    }
    if (v1 < 0 || m[0] < 3 || m[0] > 50 || (v1 && m[1] > 99))
        return -1;

    uint8_t *p = (uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; i++)
        p[i] = 0;
    s->m = m;
    s->n = n;
    s->delay = m[0];
    s->loop = v1 ? m[1] : 0;
    s->pats = W(1 + v1);
    s->smps = W(3 + v1);
    s->orns = W(5 + v1);
    s->npos = m[7 + v1];
    s->hdr = (uint16_t)(8 + v1);
    if (s->loop >= s->npos || s->smps >= n || s->orns >= n || s->pats >= n)
        return -1;
    for (int i = 0; i < s->npos; i++)
        if (m[s->hdr + i] >= 32)
            return -1;
    s->delay_cnt = 1;
    set_position(s);
    for (int i = 0; i < 3; i++) {
        asc_chan_t *c = &s->ch[i];
        c->volume = 15;
        c->target = NONE;
        object(s, s->smps, 0, 3, 150, &c->smp);
        object(s, s->orns, 0, 2, 30, &c->orn);
    }
    return 0;
}

/* Las órdenes de una fila de un canal, como las junta zxtune: primero las
 * órdenes en su orden, luego el ornamento, la muestra y la nota. */
static void pattern(asc_t *s, asc_chan_t *c)
{
    int note = -1, rest = 0, smp = -1, orn = -1, vol = -1;
    int cont_smp = 0, cont_orn = 0, reload = 0;
    int slide = 0, slide_steps = 0, slide_tone = 0, gliss = 0, gl = 0, vol_step = -1;
    int content = 0;                    /* la fila trae algo para el canal */
    uint16_t a = c->addr;
    while (a < s->n) {
        uint8_t v = B(a++);
        if (v <= 0x55) {
            note = v;
            if (c->env_parse) {
                s->env_tone = B(a++);
                s->r[11] = (uint8_t)s->env_tone;
                s->r[12] = 0;
            }
            content = 1;
            break;
        } else if (v <= 0x5D) {
            break;
        } else if (v == 0x5E) {
            c->brk = 1;
            content = 1;
            break;
        } else if (v == 0x5F) {
            rest = 1;
            content = 1;
            break;
        } else if (v <= 0x9F) {
            c->skip = v - 0x60;
            continue;
        } else if (v <= 0xBF) {
            smp = v - 0xA0;
        } else if (v <= 0xDF) {
            orn = v - 0xC0;
        } else if (v == 0xE0) {
            vol = 15;
            c->env = 1;
            c->env_parse = 1;
        } else if (v <= 0xEF) {
            vol = v - 0xE0;
            c->env = 0;
            c->env_parse = 0;
        } else if (v == 0xF0) {
            c->base_noise = B(a++);
        } else if (v <= 0xF3) {
            cont_smp |= v & 1;
            cont_orn |= v & 2;
        } else if (v == 0xF4) {
            s->delay = B(a++);
            continue;
        } else if (v <= 0xF6) {
            gliss = 1;
            gl = (v == 0xF5 ? -16 : 16) * B(a++);
        } else if (v == 0xF7 || v == 0xF9) {
            if (v == 0xF7)
                cont_smp = 1;
            slide = 1;
            slide_steps = (int8_t)B(a++);
            slide_tone = v == 0xF7;
        } else if ((v & 0xF9) == 0xF8) {
            s->r[13] = v & 15;
            s->r13_write = 1;
        } else if (v == 0xFB) {
            vol_step = B(a++);
        }
        content = 1;
    }
    c->addr = a;

    /* Una fila con algo para el canal corta su glissando y su rampa de
     * volumen; una vacía (solo un salto, el tempo y «parar») no. */
    if (content) {
        c->vol_cnt = 0;
        c->steps = 0;
    }
    if (gliss) {
        c->glissade = gl;
        c->steps = -1;                  /* sin fin */
    }
    if (vol_step >= 0) {
        c->vol_cnt = c->vol_delay = vol_step & 31;
        c->vol_slide = vol_step & 32 ? -1 : 1;
    }

    if (rest)
        c->enabled = 0;
    else if (note >= 0)
        c->enabled = 1;
    if (slide) {
        int div = slide_steps ? slide_steps : 1;
        c->steps = (int16_t)slide_steps;
        if (note >= 0) {                /* deslizarse hasta la nota */
            int range = freq(note) - freq(c->note) - (slide_tone ? c->sliding / 16 : 0);
            c->glissade = 16 * range / div;
            c->target = (uint8_t)note;
            note = -1;
            reload = 1;
        } else {
            int sl = (c->sliding | 15) ^ 15;
            c->glissade = -sl / div;
            c->sliding = c->glissade * slide_steps;
        }
    }
    if (orn >= 0)
        c->orn_num = orn;
    if (smp >= 0)
        c->smp_num = smp;
    if (note >= 0) {
        c->note = note;
        reload = 1;
    }
    if (reload) {
        c->noise = c->base_noise;
        if (c->steps <= 0)
            c->sliding = 0;
        if (!cont_smp) {
            object(s, s->smps, c->smp_num, 3, 150, &c->smp);
            c->pos_smp = 0;
            c->vol_add = 0;
            c->tone_dev = 0;
            c->brk = 0;
        }
        if (!cont_orn) {
            object(s, s->orns, c->orn_num, 2, 30, &c->orn);
            c->pos_orn = 0;
            c->note_add = 0;
        }
    }
    if (vol >= 0)
        c->volume = vol;
}

static void channel(asc_t *s, asc_chan_t *c, int ch)
{
    if (!c->enabled) {
        s->r[8 + ch] = 0;
        return;
    }
    /* Fuera de la muestra: sin tono, ruido ni volumen. */
    uint8_t f = 0, lv = 0x09;
    int8_t dev = 0;
    if (c->pos_smp < c->smp.len) {
        uint32_t l = c->smp.addr + c->pos_smp * 3u;
        f = B(l);
        dev = (int8_t)B(l + 1);
        lv = B(l + 2);
    }
    int8_t note_off = 0, noise_off = 0;
    if (c->pos_orn < c->orn.len) {
        uint32_t l = c->orn.addr + c->pos_orn * 2u;
        noise_off = (int8_t)(B(l) << 3) >> 3;
        note_off = (int8_t)B(l + 1);
    }
    int adding = (int8_t)(f << 3) >> 3;
    int cmd = lv >> 1 & 3;

    if (c->vol_cnt >= 2) {
        c->vol_cnt--;
    } else if (c->vol_cnt) {
        c->vol_add += c->vol_slide;
        c->vol_cnt = c->vol_delay;
    }
    c->vol_add += cmd == 2 ? -1 : cmd == 3 ? 1 : 0;
    if (c->vol_add < -15)
        c->vol_add = -15;
    if (c->vol_add > 15)
        c->vol_add = 15;

    c->tone_dev += dev;
    c->note_add = (int8_t)(c->note_add + note_off);
    int ht = (int8_t)(c->note + c->note_add);
    ht = ht < 0 ? 0 : ht > 0x55 ? 0x55 : ht;
    uint16_t tone = (freq(ht) + c->tone_dev + c->sliding / 16) & 0xFFF;
    s->r[ch * 2] = tone & 0xFF;
    s->r[ch * 2 + 1] = tone >> 8;
    int amp = c->vol_add + (lv >> 4);
    amp = amp < 0 ? 0 : amp > 15 ? 15 : amp;
    s->r[8 + ch] = (uint8_t)((c->volume + 1) * amp / 16);
    if (c->env && cmd == 1)
        s->r[8 + ch] |= 16;
    c->noise += noise_off;
    if (lv & 1)
        s->r[7] |= 1 << ch;
    if ((lv & 8) && cmd == 1) {
        s->env_tone += adding;
        s->r[11] = s->env_tone & 0xFF;
        s->r[12] = s->env_tone >> 8;
    } else {
        c->noise += adding;
    }
    if (!(lv & 8))
        s->r[6] = (c->noise + c->sliding / 256) & 31;
    else
        s->r[7] |= 8 << ch;

    if (c->steps != 0) {
        if (c->steps > 0 && !--c->steps && c->target != NONE) {
            c->note = c->target;
            c->target = NONE;
            c->sliding = c->glissade = 0;
        }
        c->sliding += c->glissade;
    }
    if (c->pos_smp++ >= c->smp.limit) {
        if (!c->brk)
            c->pos_smp = c->smp.loop;
        else if (c->pos_smp >= c->smp.len)
            c->enabled = 0;
    }
    if (c->pos_orn++ >= c->orn.limit)
        c->pos_orn = c->orn.loop;
}

void asc_step(asc_t *s, mus_ay_t *ay)
{
    s->r13_write = 0;
    if (--s->delay_cnt == 0) {
        asc_chan_t *c = s->ch;
        for (;;) {
            if (!s->ended && (s->row >= 64 || (c[0].skip_cnt == 0 && B(c[0].addr) == 0xFF))) {
                s->ended = 1;
                s->end_row = s->row;
            }
            int len = !s->ended ? 64 : s->end_row < 1 ? 1 : s->end_row;
            if (s->row < len)
                break;
            if (++s->pos >= s->npos) {
                s->pos = s->loop;
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
    for (int i = 0; i < 14; i++)
        ay->r[i] = s->r[i];
    ay->r13 = s->r13_write;
}
