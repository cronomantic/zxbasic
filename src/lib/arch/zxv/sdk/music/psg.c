/*
 * psg.c - Los volcados de registros PSG: "PSG" 0x1A y 12 bytes más de
 * cabecera, y luego las escrituras (registro, valor) separadas por marcas:
 * 0xFF pasa un paso, 0xFE n pasa 4·n y 0xFD es el final. Lo que va tras una
 * marca es del paso que empieza en ella. Como en zxtune, lo que va antes
 * de la primera marca no cuenta, los registros de más de 13 se saltan y los
 * valores se quedan en los bits que tiene cada registro.
 */
#include <stddef.h>

#include "music_int.h"

#define B(i)    mus_b(s->m, s->n, i)

static const uint8_t bits[14] = { 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF,
                                  0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F };

/* Las escrituras desde pos hasta la siguiente marca (o el final); con ay
 * NULL, solo se pasan. */
static void writes(psg_t *s, mus_ay_t *ay)
{
    uint8_t b;
    while (s->pos < s->n && (b = B(s->pos)) < 0xFD) {
        if (b < 14 && ay) {
            s->r[b] = B(s->pos + 1) & bits[b];
            if (b == 13)
                ay->r13 = 1;
        }
        s->pos += 2;
    }
}

/* La marca que acaba un paso: los pasos que quedan sin escrituras. */
static void mark(psg_t *s)
{
    uint8_t b = B(s->pos);
    if (s->pos >= s->n || b == 0xFD) {
        s->end = 1;
    } else if (b == 0xFE) {
        uint32_t n = B(s->pos + 1) * 4u;
        s->wait = n ? n - 1 : 0;
        s->pos += 2;
    } else {
        s->pos++;
    }
}

/* Desde el principio: lo que va antes de la primera marca, y la marca. */
static void start(psg_t *s)
{
    s->pos = 16;
    s->wait = 0;
    s->end = 0;
    writes(s, NULL);
    mark(s);
    s->end = 0;
}

int psg_init(psg_t *s, const uint8_t *m, uint32_t n)
{
    if (n < 16 || m[0] != 'P' || m[1] != 'S' || m[2] != 'G' || m[3] != 0x1A)
        return -1;
    s->m = m;
    s->n = n;
    s->looped = 0;
    for (int i = 0; i < 14; i++)
        s->r[i] = 0;
    start(s);
    return 0;
}

void psg_step(psg_t *s, mus_ay_t *ay)
{
    ay->r13 = 0;
    if (s->wait) {
        s->wait--;
    } else {
        if (s->end) {
            start(s);
            s->looped++;
        }
        writes(s, ay);
        mark(s);
    }
    for (int i = 0; i < 14; i++)
        ay->r[i] = s->r[i];
}
