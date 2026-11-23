/*
 * fade.c - Fundidos de paleta (include/zxv_fade.h).
 *
 * Opcional: añade $(SDK)/lib/fade.c a tu programa.
 *
 * Al empezar se guardan los colores de partida de cada tabla; en cada paso
 * k (de 1 a n) cada componente es partida + (destino - partida)·k / n, al
 * más cercano, así que el último paso deja exactamente el destino.
 */
#include "zxv_fade.h"

#include "zxv_api.h"

#define ZXV_FADE_REGS ((volatile uint16_t *)(uintptr_t)ZXV_PALETTES)

static uint16_t from[4][16], target[16];
static unsigned mask;
static int step_k, step_n;

/* a + (b - a)·k / n al más cercano, con los medios hacia arriba (a y b son
 * de 0 a 63): un fundido y el de vuelta pasan por los mismos colores. */
static int mix1(int a, int b, int k, int n)
{
    return (2 * (a * n + (b - a) * k) + n) / (2 * n);
}

uint16_t zxv_rgb_mix(uint16_t a, uint16_t b, int k, int n)
{
    if (n <= 0 || k >= n)
        return b;
    if (k <= 0)
        return a;
    int ra = a >> 11, ga = (a >> 5) & 63, ba = a & 31;
    int rb = b >> 11, gb = (b >> 5) & 63, bb = b & 31;
    return (uint16_t)((mix1(ra, rb, k, n) << 11) | (mix1(ga, gb, k, n) << 5) | mix1(ba, bb, k, n));
}

static void apply(void)
{
    volatile uint16_t *pal = ZXV_FADE_REGS;
    for (int t = 0; t < 4; t++)
        if (mask & (1u << t))
            for (int i = 0; i < 16; i++)
                pal[t * 16 + i] = zxv_rgb_mix(from[t][i], target[i], step_k, step_n);
}

void zxv_fade_to(const uint16_t to[16], int frames, unsigned tables)
{
    volatile uint16_t *pal = ZXV_FADE_REGS;
    mask = tables & ZXV_FADE_ALL;
    for (int t = 0; t < 4; t++)
        for (int i = 0; i < 16; i++)
            from[t][i] = pal[t * 16 + i];                 /* desde donde está (o iba) */
    for (int i = 0; i < 16; i++)
        target[i] = to ? to[i] : ZXV_STD_RGB565(i);
    step_n = frames > 0 ? frames : 0;
    step_k = 0;
    if (!step_n)
        apply();
}

void zxv_fade_to_colour(uint16_t rgb, int frames, unsigned tables)
{
    uint16_t to[16];
    for (int i = 0; i < 16; i++)
        to[i] = rgb;
    zxv_fade_to(to, frames, tables);
}

void zxv_fade_step(void)
{
    if (step_k >= step_n)
        return;
    step_k++;
    apply();
}

int zxv_fade_busy(void) { return step_k < step_n; }

void zxv_fade_wait(void)
{
    while (zxv_fade_busy()) {
        zxv_wait_vblank();
        zxv_fade_step();
    }
}
