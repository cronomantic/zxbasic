/*
 * vgm.c - Los VGM (VGM 1.71: vgmrips, VGMPlay/vgmspec171.txt) con el chip
 * que tiene ZX-V: los AY (AY-3-8910 y familia, y el YM2149). Un .vgz
 * (comprimido) se va descomprimiendo mientras suena (inflate.c).
 *
 * - Los comandos van a su hora: cada paso hace los de hasta la espera
 *   siguiente (en 1/44.100 s; las de menos de 1 ms se juntan con la de
 *   detrás) y music.c los manda con su hora (vgm->step_us).
 * - Los AY: sus registros tal cual, con los periodos del tono, del ruido y
 *   de la envolvente pasados de su reloj (el del fichero: 1,79 MHz en un
 *   MSX, 1 MHz en un CPC, 2 MHz en un Atari ST) al nuestro (1,7734475
 *   MHz), para que suenen a su altura. Dos AY, si el fichero los tiene.
 * - Lo demás (los otros chips, el SCC de Konami incluido, y sus datos) se
 *   salta: un VGM sin AY no se reconoce.
 *
 * Licencia MIT, como el resto del SDK.
 */
#include "music_int.h"
#include "zxv_music.h"

#define VGM_HZ          44100u
#define AY_X2           3546895u        /* nuestro reloj del AY, x 2 */
#define WAIT_MIN        44u             /* 1 ms: menos, con la espera siguiente */

static const uint8_t ay_bits[16] = { 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF,
                                     0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF };

/* --- Leer el fichero (o lo que sale de un .vgz) ----------------------------------- */

/* El byte siguiente, o -1 al final. */
static int next(vgm_t *v)
{
    if (!v->gz)
        return v->pos < v->n ? v->m[v->pos++] : -1;
    if (v->pos == v->z.out && !mus_inflate_run(&v->z, 1024))
        return -1;
    return v->z.win[v->pos++ & MUS_WIN_MASK];
}

static uint32_t next16(vgm_t *v)
{
    uint32_t lo = (uint32_t)next(v) & 0xFF;
    return lo | ((uint32_t)next(v) & 0xFF) << 8;
}

static void skip(vgm_t *v, uint32_t n)
{
    while (n--)
        if (next(v) < 0)
            return;
}

/* A at: en un .vgz, hacia atrás es volver a empezar a descomprimir. */
static void seek(vgm_t *v, uint32_t at)
{
    if (!v->gz) {
        v->pos = at;
        return;
    }
    if (at < v->pos) {
        uint32_t s, size;
        mus_gzip_data(v->m, v->n, &s, &size);
        mus_inflate_init(&v->z, v->m + s, v->n - s, v->z.win);
        v->pos = 0;
    }
    while (v->pos < at) {
        if (v->pos == v->z.out && !mus_inflate_run(&v->z, 4096))
            return;
        uint32_t ahead = v->z.out - v->pos;
        v->pos += ahead < at - v->pos ? ahead : at - v->pos;
    }
}

/* --- Lo que se escribe ------------------------------------------------------------ */

/* Un periodo del AY de su reloj al nuestro. */
static uint32_t scale(const vgm_t *v, uint32_t p, uint32_t max)
{
    if (!p)
        return 0;
    uint64_t q = ((uint64_t)p * v->ay_k + 32768u) >> 16;
    return q < 1 ? 1 : q > max ? max : (uint32_t)q;
}

static void ay_out(const vgm_t *v, int chip, mus_ay_t *ay)
{
    const uint8_t *r = v->ay_raw[chip];
    for (int i = 0; i < 14; i++)
        ay->r[i] = r[i] & ay_bits[i];
    for (int c = 0; c < 3; c++) {
        uint32_t p = scale(v, r[c * 2] | (uint32_t)(r[c * 2 + 1] & 15) << 8, 4095);
        ay->r[c * 2] = (uint8_t)p;
        ay->r[c * 2 + 1] = (uint8_t)(p >> 8);
    }
    ay->r[6] = (uint8_t)scale(v, r[6] & 31u, 31);
    uint32_t e = scale(v, r[11] | (uint32_t)r[12] << 8, 65535);
    ay->r[11] = (uint8_t)e;
    ay->r[12] = (uint8_t)(e >> 8);
    ay->r13 = (uint8_t)(v->ay_r13 >> chip & 1);
}

/* --- Los pasos -------------------------------------------------------------------- */

static void restart(vgm_t *v)
{
    v->looped++;
    seek(v, v->loop ? v->loop : v->data);
}

void vgm_step(vgm_t *v, mus_ay_t ay[2])
{
    uint32_t wait = 0;
    int ends = 0;
    v->ay_r13 = 0;
    while (wait < WAIT_MIN && !v->stopped) {
        int c = next(v);
        if (c < 0 || c == 0x66 || (v->end && v->pos > v->end)) {
            if (++ends > 1) {           /* nada que tocar entre el final y la vuelta */
                v->stopped = 1;
                break;
            }
            restart(v);
            continue;
        }
        if (c == 0x61) {
            wait += next16(v);
        } else if (c == 0x62) {
            wait += 735;
        } else if (c == 0x63) {
            wait += 882;
        } else if (c >= 0x70 && c <= 0x7F) {
            wait += (uint32_t)(c & 15) + 1;
        } else if (c >= 0x80 && c <= 0x8F) {
            wait += (uint32_t)(c & 15);         /* el YM2612 con su DAC: solo la espera */
        } else if (c == 0xA0) {
            int a = next(v), d = next(v);
            int chip = a >> 7 & 1, r = a & 0x7F;
            if (r < 16 && chip < v->ays) {
                v->ay_raw[chip][r] = (uint8_t)d;
                if (r == 13)
                    v->ay_r13 |= (uint8_t)(1u << chip);
            }
        } else if (c == 0x67) {                 /* un bloque de datos: 0x66 tt tamaño */
            next(v);
            next(v);
            uint32_t size = next16(v);
            size |= next16(v) << 16;
            skip(v, size);
        } else if (c == 0x68) {
            skip(v, 11);
        } else if (c >= 0x90 && c <= 0x95) {
            static const uint8_t dac[6] = { 4, 4, 5, 10, 1, 4 };
            skip(v, dac[c - 0x90]);
        } else if (c >= 0x30 && c <= 0x3F) {
            skip(v, 1);
        } else if (c >= 0x40 && c <= 0x4E) {
            skip(v, v->version >= 0x160 ? 2 : 1);
        } else if (c == 0x4F || c == 0x50) {
            skip(v, 1);
        } else if ((c >= 0x51 && c <= 0x5F) || (c >= 0xA1 && c <= 0xBF)) {
            skip(v, 2);
        } else if (c >= 0xC0 && c <= 0xDF) {
            skip(v, 3);
        } else if (c >= 0xE0) {
            skip(v, 4);
        } else {                                /* uno que no existe: se acaba ahí */
            if (++ends > 1) {
                v->stopped = 1;
                break;
            }
            restart(v);
        }
    }
    for (int i = 0; i < v->ays; i++)
        ay_out(v, i, &ay[i]);
    uint64_t f = v->frac + (uint64_t)(v->stopped ? 882u : wait) * 1000000u;
    v->step_us = (uint32_t)(f / VGM_HZ);
    v->frac = (uint32_t)(f % VGM_HZ);
    if (!v->step_us)
        v->step_us = 1;
}

/* --- Reconocerlo ------------------------------------------------------------------ */

static uint32_t le32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int vgm_probe(const uint8_t *m, uint32_t n)
{
    if (n >= 64 && m[0] == 'V' && m[1] == 'g' && m[2] == 'm' && m[3] == ' ') {
        /* Su AY, en la cabecera (si llega hasta él: antes de los comandos). */
        uint32_t version = le32(m + 0x08), data = version >= 0x150 && le32(m + 0x34) ? le32(m + 0x34) + 0x34 : 0x40;
        uint32_t ay = version >= 0x151 && data >= 0x78 && n >= 0x78 ? le32(m + 0x74) : 0;
        return !(ay & 0x3FFFFFFF) ? 0 : ay & 0x40000000 ? 2 : 1;
    }
    uint32_t at, size;
    return !mus_gzip_data(m, n, &at, &size);
}

int vgm_init(vgm_t *v, const uint8_t *m, uint32_t n, uint8_t *work)
{
    uint8_t *p = (uint8_t *)v;
    for (unsigned i = 0; i < sizeof *v; i++)
        p[i] = 0;
    v->m = m;
    v->n = n;
    uint32_t at, size;
    if (!mus_gzip_data(m, n, &at, &size)) {
        if (!work)
            return -1;                          /* sin la ventana, un .vgz no suena */
        v->gz = 1;
        mus_inflate_init(&v->z, m + at, n - at, work);
    }
    /* La cabecera: lo que esté detrás del principio de los comandos (o falte)
     * cuenta como 0. */
    uint8_t h[0x100];
    for (int i = 0; i < 0x100; i++) {
        int c = next(v);
        h[i] = (uint8_t)(c < 0 ? 0 : c);
    }
    if (h[0] != 'V' || h[1] != 'g' || h[2] != 'm' || h[3] != ' ')
        return -1;
    v->version = (uint16_t)le32(h + 0x08);
    v->end = le32(h + 0x04) ? le32(h + 0x04) + 4 : 0;
    v->data = v->version >= 0x150 && le32(h + 0x34) ? le32(h + 0x34) + 0x34 : 0x40;
    v->loop = le32(h + 0x1C) ? le32(h + 0x1C) + 0x1C : 0;
    for (uint32_t i = v->data; i < 0x100; i++)
        h[i] = 0;
    uint32_t ay = v->version >= 0x151 ? le32(h + 0x74) : 0;
    if (ay & 0x3FFFFFFF) {
        v->ays = ay & 0x40000000 ? 2 : 1;
        uint32_t clk = ay & 0x3FFFFFFF;
        if (h[0x78] >= 0x10 && (h[0x79] & 0x10))
            clk /= 2;                           /* el YM2149 con su divisor (la patilla 26) */
        v->ay_k = (uint32_t)(((uint64_t)AY_X2 << 15) / clk);
        for (int i = 0; i < 2; i++)
            v->ay_raw[i][7] = 0x3F;             /* todo callado */
    }
    if (!v->ays)
        return -1;                              /* sin AY: nada que tocar */
    seek(v, v->data);
    return 0;
}
