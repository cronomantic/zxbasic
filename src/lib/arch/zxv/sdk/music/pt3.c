/*
 * pt3.c - El reproductor de PT3: módulos de Pro Tracker 3.x y de Vortex
 * Tracker II, y los TurboSound (dos módulos seguidos, uno por AY).
 *
 * La cabecera: el texto "ProTracker 3.v compilation of " (v, la versión), el
 * título y el autor; en 0x63 la tabla de notas, el tempo, el número de
 * posiciones y la de vuelta; en 0x67 los patrones (tres punteros por patrón,
 * uno por canal), en 0x69 las 32 muestras, en 0xA9 los 16 ornamentos y en
 * 0xC9 la lista de posiciones (patrón × 3, hasta 0xFF).
 *
 * Una muestra: vuelta, largo y 4 bytes por línea:
 *   b0: bit 7 desliza el volumen (bit 6, hacia arriba), bits 5-1 el ruido o la
 *       envolvente (con signo), bit 0 sin envolvente;
 *   b1: bit 7 sin ruido (y entonces los bits 5-1 de b0 son de la envolvente),
 *       bit 6 acumula el tono, bit 5 acumula el ruido o la envolvente, bit 4
 *       sin tono, bits 3-0 el volumen;
 *   b2-b3: el desplazamiento del tono.
 * Un ornamento: vuelta, largo y un desplazamiento de nota (con signo) por línea.
 */
#include "music_int.h"

extern const uint16_t pt3_notes[7][96];
extern const uint8_t pt3_vols[2][256];

#define B(i)    mus_b(s->m, s->n, i)
#define W(i)    mus_w(s->m, s->n, i)

static void set_sample(pt3_t *s, pt3_chan_t *c, int n)
{
    uint16_t p = W(0x69 + (n & 31) * 2);
    c->smp_loop = B(p);
    c->smp_len = B(p + 1);
    c->smp = p + 2;
}

static void set_ornament(pt3_t *s, pt3_chan_t *c, int n)
{
    uint16_t p = W(0xA9 + (n & 15) * 2);
    c->orn_loop = B(p);
    c->orn_len = B(p + 1);
    c->orn = p + 2;
    c->pos_orn = 0;
}

static void set_position(pt3_t *s)
{
    uint16_t pat = W(0x67) + B(0xC9 + s->pos) * 2;     /* patrón × 3 × 2 bytes */
    for (int i = 0; i < 3; i++)
        s->ch[i].addr = W(pat + i * 2);
}

static int note_freq(pt3_t *s, int note)
{
    if (note < 0)
        note = 0;
    if (note > 95)
        note = 95;
    return s->notes[note];
}

int pt3_init(pt3_t *s, const uint8_t *m, uint32_t n)
{
    static const char sig[] = "ProTracker 3.";
    static const char vt[] = "Vortex Tracker II";
    int ok = n > 0xCA;
    for (int i = 0; ok && i < 13; i++)
        if (m[i] != sig[i])
            ok = 0;
    if (!ok && n > 0xCA) {
        ok = 1;
        for (int i = 0; ok && i < 17; i++)
            if (m[i] != vt[i])
                ok = 0;
    }
    if (!ok)
        return -1;

    uint8_t *p = (uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; i++)
        p[i] = 0;
    s->m = m;
    s->n = n;
    s->version = 6;
    if (m[13] >= '0' && m[13] <= '9')
        s->version = m[13] - '0';
    int t = B(0x63) & 3;
    int old = s->version <= 3;
    static const uint8_t tab[2][4] = { { 1, 2, 4, 6 }, { 0, 2, 3, 5 } };
    s->notes = pt3_notes[tab[old][t]];
    s->vols = pt3_vols[s->version >= 5];
    s->delay = B(0x64);
    s->npos = B(0x65);
    s->loop = B(0x66);
    s->delay_cnt = 1;
    set_position(s);
    for (int i = 0; i < 3; i++) {
        pt3_chan_t *c = &s->ch[i];
        set_sample(s, c, 1);
        set_ornament(s, c, 0);
        c->volume = 15;
        c->skip = 1;
        c->skip_cnt = 1;
    }
    return 0;
}

/* Las órdenes de una fila de un canal, hasta su nota (o su silencio). */
static void pattern(pt3_t *s, pt3_chan_t *c)
{
    uint8_t f9 = 0, f8 = 0, f5 = 0, f4 = 0, f3 = 0, f2 = 0, f1 = 0, cnt = 0;
    int prev_note = c->note;
    int16_t prev_sliding = c->sliding;
    uint16_t a = c->addr;
    for (int quit = 0; !quit; a++) {
        uint8_t v = B(a);
        if (v >= 0xF0) {
            set_ornament(s, c, v - 0xF0);
            set_sample(s, c, B(++a) / 2);
            c->env_on = 0;
        } else if (v >= 0xD1) {
            set_sample(s, c, v - 0xD0);
        } else if (v == 0xD0) {
            quit = 1;
        } else if (v >= 0xC1) {
            c->volume = v - 0xC0;
        } else if (v == 0xC0) {
            c->pos_smp = 0;
            c->amp_slide = 0;
            c->noise_slide = 0;
            c->env_slide = 0;
            c->pos_orn = 0;
            c->slide_cnt = 0;
            c->sliding = 0;
            c->acc = 0;
            c->onoff = 0;
            c->enabled = 0;
            quit = 1;
        } else if (v >= 0xB2) {
            c->env_on = 1;
            s->r13 = v - 0xB1;
            s->r13_write = 1;
            s->env_base = B(a + 1) << 8 | B(a + 2);
            a += 2;
            c->pos_orn = 0;
            s->env_slide = 0;
            s->env_delay_cnt = 0;
        } else if (v == 0xB1) {
            c->skip = B(++a);
        } else if (v == 0xB0) {
            c->env_on = 0;
            c->pos_orn = 0;
        } else if (v >= 0x50) {
            c->note = v - 0x50;
            c->pos_smp = 0;
            c->amp_slide = 0;
            c->noise_slide = 0;
            c->env_slide = 0;
            c->pos_orn = 0;
            c->slide_cnt = 0;
            c->sliding = 0;
            c->acc = 0;
            c->onoff = 0;
            c->enabled = 1;
            quit = 1;
        } else if (v >= 0x40) {
            set_ornament(s, c, v - 0x40);
        } else if (v >= 0x20) {
            s->noise_base = v - 0x20;
        } else if (v >= 0x10) {
            if (v == 0x10) {
                c->env_on = 0;
            } else {
                s->r13 = v - 0x10;
                s->r13_write = 1;
                s->env_base = B(a + 1) << 8 | B(a + 2);
                a += 2;
                c->env_on = 1;
                s->env_slide = 0;
                s->env_delay_cnt = 0;
            }
            set_sample(s, c, B(++a) / 2);
            c->pos_orn = 0;
        } else if (v == 9) {
            f9 = ++cnt;
        } else if (v == 8) {
            f8 = ++cnt;
        } else if (v == 5) {
            f5 = ++cnt;
        } else if (v == 4) {
            f4 = ++cnt;
        } else if (v == 3) {
            f3 = ++cnt;
        } else if (v == 2) {
            f2 = ++cnt;
        } else if (v == 1) {
            f1 = ++cnt;
        }
    }

    /* Los parámetros de los efectos van tras la nota, del último al primero. */
    for (; cnt > 0; cnt--) {
        if (cnt == f1) {                        /* glissando */
            c->slide_delay = B(a);
            c->slide_cnt = c->slide_delay;
            c->slide_step = (int16_t)W(a + 1);
            a += 3;
            c->gliss = 1;
            c->onoff = 0;
            if (c->slide_cnt == 0 && s->version >= 7)
                c->slide_cnt++;
        } else if (cnt == f2) {                 /* portamento */
            c->gliss = 0;
            c->onoff = 0;
            c->slide_delay = B(a);
            c->slide_cnt = c->slide_delay;
            int16_t step = (int16_t)W(a + 3);
            a += 5;
            if (step < 0)
                step = -step;
            c->delta = note_freq(s, c->note) - note_freq(s, prev_note);
            c->slide_to = c->note;
            c->note = prev_note;
            if (s->version >= 6)
                c->sliding = prev_sliding;
            if (c->delta - c->sliding < 0)
                step = -step;
            c->slide_step = step;
        } else if (cnt == f3) {                 /* posición en la muestra */
            c->pos_smp = B(a++);
        } else if (cnt == f4) {                 /* posición en el ornamento */
            c->pos_orn = B(a++);
        } else if (cnt == f5) {                 /* vibrato: encendido y apagado */
            c->on_delay = B(a);
            c->off_delay = B(a + 1);
            a += 2;
            c->onoff = c->on_delay;
            c->slide_cnt = 0;
            c->sliding = 0;
        } else if (cnt == f8) {                 /* desliza la envolvente */
            s->env_delay = B(a);
            s->env_delay_cnt = s->env_delay;
            s->env_slide_add = (int16_t)W(a + 1);
            a += 3;
        } else if (cnt == f9) {                 /* tempo */
            s->delay = B(a++);
        }
    }
    c->addr = a;
    c->skip_cnt = c->skip;
}

/* Un canal en este paso: su tono, su volumen y lo que pone en el mezclador,
 * el ruido y la envolvente. */
static void channel(pt3_t *s, pt3_chan_t *c, uint8_t *mixer)
{
    if (c->enabled) {
        /* Una posición fuera de la muestra (al cambiarla por una más corta
         * con la nota sonando, o con el efecto 3) es una línea en silencio,
         * sin tono, ruido ni envolvente, como en zxtune. */
        uint16_t l = c->smp + c->pos_smp * 4;
        int in = c->pos_smp < c->smp_len;
        uint8_t b0 = in ? B(l) : 0x01, b1 = in ? B(l + 1) : 0x90;
        c->ton = (in ? W(l + 2) : 0) + c->acc;
        if (b1 & 0x40)
            c->acc = c->ton;
        /* Una posición fuera del ornamento (el efecto 4 puede dejarla ahí)
         * no desplaza la nota, como en zxtune. */
        int note = c->note + (c->pos_orn < c->orn_len ? (int8_t)B(c->orn + c->pos_orn) : 0);
        c->ton = (c->ton + c->sliding + note_freq(s, note)) & 0xFFF;
        if (c->slide_cnt > 0 && --c->slide_cnt == 0) {
            c->sliding += c->slide_step;
            c->slide_cnt = c->slide_delay;
            if (!c->gliss && ((c->slide_step < 0 && c->sliding <= c->delta) ||
                              (c->slide_step >= 0 && c->sliding >= c->delta))) {
                c->note = c->slide_to;
                c->slide_cnt = 0;
                c->sliding = 0;
            }
        }
        if (b0 & 0x80) {
            if (b0 & 0x40) {
                if (c->amp_slide < 15)
                    c->amp_slide++;
            } else if (c->amp_slide > -15) {
                c->amp_slide--;
            }
        }
        int amp = (b1 & 15) + c->amp_slide;
        if (amp < 0)
            amp = 0;
        if (amp > 15)
            amp = 15;
        c->amp = s->vols[c->volume * 16 + amp];
        if (!(b0 & 1) && c->env_on)
            c->amp |= 16;
        if (b1 & 0x80) {
            int16_t j = (int8_t)(b0 & 0x20 ? (b0 >> 1) | 0xF0 : (b0 >> 1) & 0x0F) + c->env_slide;
            if (b1 & 0x20)
                c->env_slide = j;
            s->add_env += j;
        } else {
            s->add_noise = (b0 >> 1) + c->noise_slide;
            if (b1 & 0x20)
                c->noise_slide = s->add_noise;
        }
        *mixer |= (b1 >> 1) & 0x48;
        if (++c->pos_smp >= c->smp_len)
            c->pos_smp = c->smp_loop;
        if (++c->pos_orn >= c->orn_len)
            c->pos_orn = c->orn_loop;
    } else {
        c->amp = 0;
    }
    *mixer >>= 1;
    if (c->onoff > 0 && --c->onoff == 0) {
        c->enabled = !c->enabled;
        c->onoff = c->enabled ? c->on_delay : c->off_delay;
    }
}

void pt3_step(pt3_t *s, mus_ay_t *ay)
{
    s->r13_write = 0;
    if (--s->delay_cnt == 0) {
        pt3_chan_t *c = s->ch;
        if (--c[0].skip_cnt == 0) {
            if (B(c[0].addr) == 0) {            /* fin del patrón */
                if (++s->pos >= s->npos) {
                    s->pos = s->loop;
                    s->looped++;
                }
                set_position(s);
                s->noise_base = 0;
            }
            pattern(s, &c[0]);
        }
        if (--c[1].skip_cnt == 0)
            pattern(s, &c[1]);
        if (--c[2].skip_cnt == 0)
            pattern(s, &c[2]);
        s->delay_cnt = s->delay;
    }

    uint8_t mixer = 0;
    s->add_env = 0;
    for (int i = 0; i < 3; i++)
        channel(s, &s->ch[i], &mixer);

    for (int i = 0; i < 3; i++) {
        ay->r[i * 2] = s->ch[i].ton & 0xFF;
        ay->r[i * 2 + 1] = s->ch[i].ton >> 8;
        ay->r[8 + i] = s->ch[i].amp;
    }
    ay->r[6] = (s->noise_base + s->add_noise) & 31;
    ay->r[7] = mixer;
    uint16_t env = s->env_base + s->add_env + s->env_slide;
    ay->r[11] = env & 0xFF;
    ay->r[12] = env >> 8;
    ay->r[13] = s->r13;
    ay->r13 = s->r13_write;
    if (s->env_delay_cnt > 0 && --s->env_delay_cnt == 0) {
        s->env_delay_cnt = s->env_delay;
        s->env_slide += s->env_slide_add;
    }
}

int pt3_ts_split(const uint8_t *m, uint32_t n, uint32_t *n1, uint32_t *n2)
{
    if (n < 16 + 2 * 0xCA)
        return 0;
    const uint8_t *f = m + n - 16;
    static const char a[] = "PT3!", b[] = "02TS";
    for (int i = 0; i < 4; i++)
        if (f[i] != a[i] || f[6 + i] != a[i] || f[12 + i] != b[i])
            return 0;
    *n1 = f[4] | f[5] << 8;
    *n2 = f[10] | f[11] << 8;
    return *n1 + *n2 + 16 <= n;
}
