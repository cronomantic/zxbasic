/*
 * zxv_gfx.h - Gráficos para juegos (spec §7.8): formatos y llamadas.
 *
 * Todos los gráficos son de 4 bpp como la VRAM (nibble bajo = píxel par) y
 * el índice 8 es el transparente. Las funciones dibujan en el destino actual
 * (la página de dibujo, o un bitmap en memoria) y recortan contra su
 * rectángulo de recorte.
 */
#ifndef ZXV_GFX_H
#define ZXV_GFX_H

#include <stdint.h>

#include "zxv.h"

#define ZXV_TRANSPARENT     8

/* Bitmap: una pantalla, una hoja de sprites o un buffer. */
typedef struct {
    uint8_t *pix;           /* primer píxel, alineado a 4 */
    uint16_t stride;        /* bytes por línea, múltiplo de 4 */
    uint16_t w, h;          /* en píxeles */
    uint16_t reserved;
} zxv_bitmap;

/* Opciones de gfx_blit, spr_draw y spr_draw_scaled. */
#define ZXV_FLIP_H          0x01u
#define ZXV_FLIP_V          0x02u
#define ZXV_BLIT_TRANSPARENT 0x04u      /* gfx_blit: el 8 no se copia */

/* Mapa de tiles. */
typedef struct {
    const uint16_t *cells;  /* celdas, fila a fila */
    uint16_t w, h;          /* en celdas */
    const uint8_t *tiles;   /* gráficos de los tiles, seguidos (32 o 128 B) */
    uint8_t size;           /* lado del tile: 8 o 16 */
    uint8_t flags;          /* ZXV_MAP_WRAP */
} zxv_tilemap;

#define ZXV_MAP_WRAP        0x01u       /* el mapa se repite */

/* Celda: número de tile y atributos. */
#define ZXV_CELL_TILE       0x0FFFu
#define ZXV_CELL_FLIP_H     0x1000u
#define ZXV_CELL_FLIP_V     0x2000u
#define ZXV_CELL_TRANSP     0x4000u     /* el 8 deja ver lo de debajo */
#define ZXV_CELL_PRIO       0x8000u

/* Filtro de map_draw. */
#define ZXV_MAP_ALL         0
#define ZXV_MAP_BACK        1           /* solo las celdas sin prioridad */
#define ZXV_MAP_FRONT       2           /* solo las celdas con prioridad */

/* Modo 7. */
typedef struct {
    const uint8_t *texels;  /* 1 byte por texel (0-15), fila a fila */
    uint8_t log2size;       /* lado = 2^log2size (de 1 a 10) */
    uint8_t wrap;           /* 1: se repite; 0: fuera se ve 'out' */
    uint8_t out;
    uint8_t reserved;
} zxv_m7tex;

typedef struct { int32_t u, v, du, dv; } zxv_m7line;   /* coma fija 16.16 */

typedef struct {            /* coma fija 16.16 */
    int32_t x, y;           /* posición sobre la textura */
    int32_t height;         /* altura sobre el suelo */
    int32_t angle;          /* 0x10000 = una vuelta */
    int32_t focal;          /* distancia focal, en píxeles */
    int16_t horizon;        /* línea del destino donde está el horizonte (entera) */
    int16_t reserved;
} zxv_m7cam;

#ifndef ZXV_GFX_NO_API
#include "zxv_api.h"

static inline void zxv_gfx_target(const zxv_bitmap *b)
{ ZXV_API_FN(ZXV_API_GFX_TARGET, void (*)(const zxv_bitmap *))(b); }
static inline void zxv_gfx_draw_page(int page)
{ ZXV_API_FN(ZXV_API_GFX_DRAW_PAGE, void (*)(int))(page); }
static inline void zxv_gfx_clip(int x0, int y0, int x1, int y1)
{ ZXV_API_FN(ZXV_API_GFX_CLIP, void (*)(int, int, int, int))(x0, y0, x1, y1); }
static inline void zxv_gfx_blit(const zxv_bitmap *src, int sx, int sy, int w, int h, int x, int y, int flags)
{ ZXV_API_FN(ZXV_API_GFX_BLIT, void (*)(const zxv_bitmap *, int, int, int, int, int, int, int))(src, sx, sy, w, h, x, y, flags); }
static inline uint32_t zxv_spr_prepare(const zxv_bitmap *src, int sx, int sy, int w, int h, void *buf)
{ return ZXV_API_FN(ZXV_API_SPR_PREPARE, uint32_t (*)(const zxv_bitmap *, int, int, int, int, void *))(src, sx, sy, w, h, buf); }
static inline void zxv_spr_draw(const void *spr, int x, int y, int flags, const uint8_t *colours)
{ ZXV_API_FN(ZXV_API_SPR_DRAW, void (*)(const void *, int, int, int, const uint8_t *))(spr, x, y, flags, colours); }
/* El sprite estirado o encogido a w x h (de 1 a 4096; vecino más cercano). */
static inline void zxv_spr_draw_scaled(const void *spr, int x, int y, int w, int h, int flags, const uint8_t *colours)
{ ZXV_API_FN(ZXV_API_SPR_DRAW_SCALED, void (*)(const void *, int, int, int, int, int, const uint8_t *))(spr, x, y, w, h, flags, colours); }
static inline int zxv_spr_collide(const void *a, int ax, int ay, const void *b, int bx, int by)
{ return ZXV_API_FN(ZXV_API_SPR_COLLIDE, int (*)(const void *, int, int, const void *, int, int))(a, ax, ay, b, bx, by); }
static inline void zxv_map_draw(const zxv_tilemap *m, int mx, int my, int x, int y, int w, int h, int filter)
{ ZXV_API_FN(ZXV_API_MAP_DRAW, void (*)(const zxv_tilemap *, int, int, int, int, int, int, int))(m, mx, my, x, y, w, h, filter); }
static inline uint16_t zxv_map_cell(const zxv_tilemap *m, int px, int py)
{ return ZXV_API_FN(ZXV_API_MAP_CELL, uint16_t (*)(const zxv_tilemap *, int, int))(m, px, py); }
static inline void zxv_m7_draw(const zxv_m7tex *t, const zxv_m7line *l, int y, int n)
{ ZXV_API_FN(ZXV_API_M7_DRAW, void (*)(const zxv_m7tex *, const zxv_m7line *, int, int))(t, l, y, n); }
static inline void zxv_m7_camera(zxv_m7line *l, int n, const zxv_m7cam *c)
{ ZXV_API_FN(ZXV_API_M7_CAMERA, void (*)(zxv_m7line *, int, const zxv_m7cam *))(l, n, c); }
static inline void zxv_gfx_line(int x0, int y0, int x1, int y1, int c)
{ ZXV_API_FN(ZXV_API_GFX_LINE, void (*)(int, int, int, int, int))(x0, y0, x1, y1, c); }
static inline void zxv_gfx_rect(int x, int y, int w, int h, int c)
{ ZXV_API_FN(ZXV_API_GFX_RECT, void (*)(int, int, int, int, int))(x, y, w, h, c); }
static inline void zxv_gfx_tri(const int16_t *p, int c)
{ ZXV_API_FN(ZXV_API_GFX_TRI, void (*)(const int16_t *, int))(p, c); }
static inline void zxv_gfx_poly(const int16_t *p, int n, int c)
{ ZXV_API_FN(ZXV_API_GFX_POLY, void (*)(const int16_t *, int, int))(p, n, c); }
static inline void zxv_gfx_circle(int x, int y, int r, int c, int fill)
{ ZXV_API_FN(ZXV_API_GFX_CIRCLE, void (*)(int, int, int, int, int))(x, y, r, c, fill); }
#endif

#endif /* ZXV_GFX_H */
