/*
 * mapmove.c - Mover cosas por un mapa de tiles sin atravesar las paredes
 * (include/zxv_mapmove.h).
 *
 * Opcional: añade $(SDK)/lib/mapmove.c a tu programa.
 *
 * El rectángulo ocupa [x, x + w) × [y, y + h) en coma fija 16.16. Al moverse
 * por un eje, solo se miran las columnas (o filas) en las que entra, de la
 * más cercana a la más lejana, con las filas (o columnas) que ocupa en el
 * otro eje: la primera que bloquea lo para tocándola. Así no se salta
 * ninguna pared por rápido que vaya, y si ya estaba metido en una puede
 * salir. Una plataforma solo bloquea al bajar, y una fila en la que se entra
 * bajando está entera por debajo de los pies de antes: es justo «venir de
 * encima».
 */
#include "zxv_mapmove.h"

static int32_t wrap_mod(int64_t a, int32_t n)
{
    int64_t r = a % n;
    return (int32_t)(r < 0 ? r + n : r);
}

static int cell_kind(const zxv_mapsolid *ms, int64_t cx, int64_t cy)
{
    const zxv_tilemap *m = ms->map;
    int32_t x, y;
    if (m->flags & ZXV_MAP_WRAP) {
        x = wrap_mod(cx, m->w);
        y = wrap_mod(cy, m->h);
    } else {
        if (cx < 0 || cy < 0 || cx >= m->w || cy >= m->h)
            return ms->outside;
        x = (int32_t)cx;
        y = (int32_t)cy;
    }
    unsigned t = m->cells[(uint32_t)y * m->w + (uint32_t)x] & ZXV_CELL_TILE;
    if (!ms->kinds)
        return t ? ZXV_TILE_SOLID : ZXV_TILE_EMPTY;
    return t < ms->nkinds ? ms->kinds[t] : ZXV_TILE_EMPTY;
}

static int shift_of(const zxv_mapsolid *ms) { return 16 + (ms->map->size == 16 ? 4 : 3); }

static int valid(const zxv_mapsolid *ms)
{
    return ms && ms->map && ms->map->cells && ms->map->w && ms->map->h &&
           (ms->map->size == 8 || ms->map->size == 16);
}

/* ¿Alguna celda de la columna c (filas r0..r1) bloquea? (o de la fila c,
 * columnas r0..r1, con by_row). */
static int line_blocks(const zxv_mapsolid *ms, int64_t c, int64_t r0, int64_t r1, int by_row, int platforms)
{
    for (int64_t r = r0; r <= r1; r++) {
        int k = by_row ? cell_kind(ms, r, c) : cell_kind(ms, c, r);
        if (k == ZXV_TILE_SOLID || (platforms && k == ZXV_TILE_PLATFORM))
            return 1;
    }
    return 0;
}

/* Mueve *p (la x, o la y con by_row) d unidades; o0, o1: lo que ocupa en el
 * otro eje (de o0 a o1 sin incluir); size: su tamaño en este eje. */
static unsigned move_axis(const zxv_mapsolid *ms, int64_t *p, int64_t size, int64_t o0, int64_t o1, int64_t d,
                          int by_row, unsigned flags)
{
    if (d == 0)
        return 0;
    int sh = shift_of(ms);
    int64_t r0 = o0 >> sh, r1 = (o1 - 1) >> sh;
    if (d > 0) {
        int64_t e0 = *p + size, e1 = e0 + d;
        int plat = by_row && !(flags & ZXV_MOVE_THROUGH);
        for (int64_t c = ((e0 - 1) >> sh) + 1; c <= (e1 - 1) >> sh; c++)
            if (line_blocks(ms, c, r0, r1, by_row, plat)) {
                *p = (c << sh) - size;
                return by_row ? ZXV_HIT_DOWN : ZXV_HIT_RIGHT;
            }
    } else {
        int64_t e0 = *p, e1 = e0 + d;
        for (int64_t c = (e0 >> sh) - 1; c >= e1 >> sh; c--)
            if (line_blocks(ms, c, r0, r1, by_row, 0)) {
                *p = (c + 1) << sh;
                return by_row ? ZXV_HIT_UP : ZXV_HIT_LEFT;
            }
    }
    *p += d;
    return 0;
}

unsigned zxv_map_move(const zxv_mapsolid *ms, zxv_fix *x, zxv_fix *y, int w, int h, zxv_fix dx, zxv_fix dy,
                      unsigned flags)
{
    if (!valid(ms) || !x || !y)
        return 0;
    int64_t W = (int64_t)(w < 1 ? 1 : w) << 16, H = (int64_t)(h < 1 ? 1 : h) << 16;
    int64_t px = *x, py = *y;
    unsigned hit = move_axis(ms, &px, W, py, py + H, dx, 0, flags);
    hit |= move_axis(ms, &py, H, px, px + W, dy, 1, flags);
    *x = (zxv_fix)px;
    *y = (zxv_fix)py;
    return hit;
}

int zxv_map_on_ground(const zxv_mapsolid *ms, zxv_fix x, zxv_fix y, int w, int h, unsigned flags)
{
    if (!valid(ms))
        return 0;
    int64_t W = (int64_t)(w < 1 ? 1 : w) << 16, H = (int64_t)(h < 1 ? 1 : h) << 16;
    int64_t py = y;
    return move_axis(ms, &py, H, x, x + W, 1, 1, flags) != 0;
}

int zxv_map_find(const zxv_mapsolid *ms, zxv_fix x, zxv_fix y, int w, int h, int kind, int *cx, int *cy)
{
    if (!valid(ms))
        return 0;
    int sh = shift_of(ms);
    int64_t W = (int64_t)(w < 1 ? 1 : w) << 16, H = (int64_t)(h < 1 ? 1 : h) << 16;
    int64_t c0 = (int64_t)x >> sh, c1 = ((int64_t)x + W - 1) >> sh;
    int64_t r0 = (int64_t)y >> sh, r1 = ((int64_t)y + H - 1) >> sh;
    for (int64_t r = r0; r <= r1; r++)
        for (int64_t c = c0; c <= c1; c++)
            if (cell_kind(ms, c, r) == kind) {
                int wrap = ms->map->flags & ZXV_MAP_WRAP;
                if (cx)
                    *cx = wrap ? wrap_mod(c, ms->map->w) : (int)c;
                if (cy)
                    *cy = wrap ? wrap_mod(r, ms->map->h) : (int)r;
                return 1;
            }
    return 0;
}

int zxv_map_kind(const zxv_mapsolid *ms, int px, int py)
{
    if (!valid(ms))
        return ZXV_TILE_EMPTY;
    int lg = ms->map->size == 16 ? 4 : 3;
    return cell_kind(ms, (int64_t)px >> lg, (int64_t)py >> lg);
}
