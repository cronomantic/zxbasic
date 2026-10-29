/*
 * afx.c - Los efectos AYFX (del AY Sound FX Editor de Shiru), como su
 * reproductor de Z80: un frame por paso de 1/50 s, cada uno un byte de
 * opciones y los valores que cambian:
 *
 *   bits 0-3 el volumen, bit 4 sin tono, bit 5 sigue el tono (2 bytes),
 *   bit 6 sigue el ruido (1 byte), bit 7 sin ruido.
 *
 * Un ruido de 0x20 (la secuencia D0 20) termina el efecto. Un banco (.afb):
 * el número de efectos (0 son 256) y, por efecto, su desplazamiento desde
 * el segundo byte del propio desplazamiento; un .afx es un efecto solo.
 */
#include <stddef.h>

#include "music_int.h"

const uint8_t *afx_find(const uint8_t *m, uint32_t n, int fx, int *count)
{
    int total = n ? (m[0] ? m[0] : 256) : 0;
    if (count)
        *count = total;
    if (fx < 0 || fx >= total || 3u + 2u * fx > n)
        return NULL;
    uint32_t at = 2u + 2u * fx + (m[1 + 2 * fx] | m[2 + 2 * fx] << 8);
    return at < n ? m + at : NULL;
}

void afx_start(afx_t *a, const uint8_t *p, const uint8_t *end)
{
    a->p = p;
    a->end = end;
    a->vol = 0;
    a->mix = 3;
    a->has_tone = 0;
    a->has_noise = 0;
    a->age = 0;
}

int afx_step(afx_t *a)
{
    const uint8_t *p = a->p;
    if (!p)
        return 0;
    if (p >= a->end) {
        a->p = NULL;
        a->vol = 0;
        return 2;
    }
    uint8_t e = *p++;
    a->vol = e & 15;
    if (e & 0x20) {
        if (p + 2 > a->end) {
            a->p = NULL;
            return 2;
        }
        a->tone = (p[0] | p[1] << 8) & 0xFFF;
        a->has_tone = 1;
        p += 2;
    }
    if (e & 0x40) {
        if (p >= a->end || *p >= 0x20) {       /* el final: solo cuenta el volumen */
            a->p = NULL;
            return 2;
        }
        a->noise = *p++;
        a->has_noise = 1;
    }
    a->mix = (e >> 4 & 1) | (e >> 6 & 2);
    a->p = p;
    a->age++;
    return 1;
}
