/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_maps.c - Tiles, tile maps, mode 7 textures and planes for zxv.bas: TILE,
 * TILESTR, MAPDIM, MAPPOKE, MAPPOKESTR, MAP, CELL, TEXTURE, FLOOR, FLOORMAP,
 * ROTATE and ROTATEMAP. The same as the ZX-V BASIC (its rom/src/basic/bgfx.c:
 * TILE, MAP DIM, MAP POKE, MAP, CELL, TEXTURE, FLOOR [MAP], ROTATE [MAP]):
 * two sets of tiles (8 x 8 and 16 x 16, 0-4095, that grow when a higher one
 * is defined, the new ones black), 16 maps and 4 textures, in the heap. The
 * engine of the ROM draws them on the drawing page, with CLIP and PATTERN.
 * ROTATE, FLOORMAP and ROTATEMAP need a ROM with them (ZX-V D110).
 */
#include "zxb.h"
#include "zxv_gfx.h"

#define ERR_INT_RANGE   10              /* B Integer out of range */
#define ERR_COLOUR      19              /* K Invalid colour */

#define TILES_MAX   4096
#define MAPS        16
#define TEXTURES    4

static void range(int v, int lo, int hi)
{
    if (v < lo || v > hi)
        zxb_ERROR(ERR_INT_RANGE);
}

static void colour(int c)
{
    if (c < -1 || c > 15)
        zxb_ERROR(ERR_COLOUR);
}

/* A pixel of the drawing page as it goes into a tile or a texture: colour t
 * (-1: none) and what is out of the page become transparent, and the 8
 * there was, a 0 (both are black), as GRAB */
static int grab_pixel(const zxv_bitmap *pg, int x, int y, int t)
{
    if (x < 0 || y < 0 || x >= pg->w || y >= pg->h)
        return ZXV_TRANSPARENT;
    uint8_t v = pg->pix[(uint32_t)y * pg->stride + (uint32_t)(x >> 1)];
    int c = x & 1 ? v >> 4 : v & 15;
    return c == t ? ZXV_TRANSPARENT : c == ZXV_TRANSPARENT ? 0 : c;
}

static void set_nibble(uint8_t *p, int x, int c)
{
    uint8_t *b = p + (x >> 1);
    *b = x & 1 ? (uint8_t)((*b & 0x0F) | (c << 4)) : (uint8_t)((*b & 0xF0) | c);
}

static int hex_colour(int ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

/* --- Tiles ------------------------------------------------------------------------ */

static uint8_t *tile_pix[2];             /* 8 x 8 and 16 x 16: 32 and 128 bytes each */
static uint16_t tile_count[2];

static int set_of(int s)
{
    if (s != 8 && s != 16)
        zxb_ERROR(ERR_INVALID_ARG);
    return s == 16;
}

/* The set of side s has at least n + 1 tiles (the new ones, black); tile n */
static uint8_t *tile(int s, int n)
{
    int k = set_of(s);
    uint32_t tb = (uint32_t)(s * s / 2);
    range(n, 0, TILES_MAX - 1);
    if (n >= tile_count[k]) {
        uint8_t *p = zxb_alloc((uint32_t)(n + 1) * tb);
        uint32_t old = tile_count[k] * tb;
        for (uint32_t i = 0; i < old; i++)
            p[i] = tile_pix[k][i];
        for (uint32_t i = old; i < (uint32_t)(n + 1) * tb; i++)
            p[i] = 0;
        if (tile_pix[k])
            zxb_free(tile_pix[k]);
        tile_pix[k] = p;
        tile_count[k] = (uint16_t)(n + 1);
    }
    return tile_pix[k] + (uint32_t)n * tb;
}

/* TILE s, n, x, y[, w, h[, t]]: tile n of side s (8 or 16) from the drawing
 * page at (x, y); with w and h (not 0), that rectangle cut in tiles numbered
 * from n row by row. Colour t becomes transparent */
void zxb_tile(int s, int n, int x, int y, int w, int h, int t)
{
    set_of(s);
    colour(t);
    int cols = 1, rows = 1;
    if (w || h) {
        range(w, s, 4096);
        range(h, s, 4096);
        cols = w / s;
        rows = h / s;
    }
    range(n + cols * rows - 1, 0, TILES_MAX - 1);
    tile(s, n + cols * rows - 1);                       /* once, before the pointers */
    zxv_bitmap pg = zxb_page_bitmap();
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            uint8_t *p = tile(s, n + r * cols + c);
            for (int j = 0; j < s; j++)
                for (int i = 0; i < s; i++)
                    set_nibble(p + j * (s / 2), i, grab_pixel(&pg, x + c * s + i, y + r * s + j, t));
        }
}

/* TILESTR s, n, a$: tiles from a string, s x s characters each (the hex
 * digits are colours, anything else transparent); a longer one defines the
 * next tiles */
void zxb_tile_str(int s, int n, zstr a)
{
    set_of(s);
    uint32_t len = zstr_len(a), per = (uint32_t)(s * s);
    if (!len)
        zxb_ERROR(ERR_INVALID_ARG);
    int count = (int)((len + per - 1) / per);
    range(n + count - 1, 0, TILES_MAX - 1);
    tile(s, n + count - 1);
    for (int k = 0; k < count; k++) {
        uint8_t *p = tile(s, n + k);
        for (uint32_t i = 0; i < per; i++) {
            uint32_t at = (uint32_t)k * per + i;
            int c = at < len ? hex_colour(a[2 + at]) : -1;
            set_nibble(p + (i / (uint32_t)s) * (uint32_t)(s / 2), (int)(i % (uint32_t)s), c < 0 ? ZXV_TRANSPARENT : c);
        }
    }
}

/* --- Maps ----------------------------------------------------------------------------- */

typedef struct {
    uint16_t *cells;
    uint16_t w, h;
    uint8_t size, flags;
    uint16_t maxt;                      /* the highest tile it uses */
} map_t;

static map_t maps[MAPS];

static map_t *map_get(int m)
{
    range(m, 0, MAPS - 1);
    if (!maps[m].cells)
        zxb_ERROR(ERR_INVALID_ARG);
    return &maps[m];
}

/* The map for the engine; with need, all its tiles must exist */
static zxv_tilemap tilemap(int m, int need)
{
    map_t *p = map_get(m);
    int k = p->size == 16;
    if (need && tile_count[k] <= p->maxt)
        zxb_ERROR(ERR_INVALID_ARG);
    zxv_tilemap t = { p->cells, p->w, p->h, tile_pix[k], p->size, p->flags };
    return t;
}

/* MAPDIM m, w, h[, s[, r]]: map m (0-15) of w x h cells, all tile 0, of
 * tiles of side s (8 or 16); with r = 1 it repeats */
void zxb_map_dim(int m, int w, int h, int s, int r)
{
    range(m, 0, MAPS - 1);
    range(w, 1, 32767);
    range(h, 1, 32767);
    set_of(s);
    range(r, 0, 1);
    uint32_t n = (uint32_t)w * (uint32_t)h;
    uint16_t *c = zxb_alloc(n * 2);
    for (uint32_t i = 0; i < n; i++)
        c[i] = 0;
    if (maps[m].cells)
        zxb_free(maps[m].cells);
    maps[m] = (map_t){ c, (uint16_t)w, (uint16_t)h, (uint8_t)s, (uint8_t)(r ? ZXV_MAP_WRAP : 0), 0 };
}

/* MAPPOKE m, x, y, v: the cell (x, y) is v: the tile, + 4096 mirrored left to
 * right, + 8192 upside down, + 16384 its 8 transparent, + 32768 priority */
void zxb_map_poke(int m, int x, int y, int v)
{
    map_t *p = map_get(m);
    range(x, 0, p->w - 1);
    range(y, 0, p->h - 1);
    range(v, 0, 65535);
    p->cells[(uint32_t)y * p->w + (uint32_t)x] = (uint16_t)v;
    if ((v & ZXV_CELL_TILE) > p->maxt)
        p->maxt = (uint16_t)(v & ZXV_CELL_TILE);
}

/* MAPPOKESTR m, x, y, a$: the cells from (x, y) to the right, the code of
 * each character as the tile */
void zxb_map_poke_str(int m, int x, int y, zstr a)
{
    map_t *p = map_get(m);
    range(x, 0, p->w - 1);
    range(y, 0, p->h - 1);
    uint32_t len = zstr_len(a);
    uint16_t *row = p->cells + (uint32_t)y * p->w;
    for (uint32_t i = 0; i < len && x + (int)i < p->w; i++) {
        row[x + (int)i] = a[2 + i];
        if (a[2 + i] > p->maxt)
            p->maxt = a[2 + i];
    }
}

/* MAP m, mx, my, x, y, w, h, f: draws map m from its pixel (mx, my) in the
 * rectangle (x, y, w, h) of the page (w or h < 0: to the end of the page);
 * f 0 every cell, 1 without priority, 2 only with priority */
void zxb_map_draw(int m, int mx, int my, int x, int y, int w, int h, int f)
{
    range(f, 0, 2);
    zxv_tilemap t = tilemap(m, 1);
    zxv_bitmap pg = zxb_page_bitmap();
    if (w < 0)
        w = pg.w - x;
    if (h < 0)
        h = pg.h - y;
    zxb_gfx_begin();
    zxv_map_draw(&t, mx, my, x, y, w, h, f);
    zxb_gfx_end();
}

/* Map m for zxb_sdklib.c (zxvmapmove.bas): it must exist, not its tiles */
zxv_tilemap zxb_maps_tilemap(int m) { return tilemap(m, 0); }

/* CELL(m, x, y): the cell at column x and row y, with its bits (0 out of a
 * map that does not repeat) */
int zxb_cell(int m, int x, int y)
{
    zxv_tilemap t = tilemap(m, 0);
    return zxv_map_cell(&t, x * t.size, y * t.size);
}

/* --- Mode 7 -------------------------------------------------------------------------- */

typedef struct {
    uint8_t *texels;
    uint8_t log2, wrap, out, flags;
} tex_t;

static tex_t texs[TEXTURES];

/* TEXTURE n, x, y, s[, c[, t]]: texture n (0-3) is the square of s x s (2,
 * 4... 256) of the page at (x, y); it repeats, or outside it is colour c
 * (-1: it repeats); colour t is transparent, and with c = t the outside too */
void zxb_texture(int n, int x, int y, int s, int c, int t)
{
    range(n, 0, TEXTURES - 1);
    colour(c);
    colour(t);
    if (s < 2 || s > 256 || (s & (s - 1)))
        zxb_ERROR(ERR_INVALID_ARG);
    int l2 = 0;
    while ((1 << l2) < s)
        l2++;
    uint8_t *p = zxb_alloc((uint32_t)(s * s));
    zxv_bitmap pg = zxb_page_bitmap();
    for (int j = 0; j < s; j++)
        for (int i = 0; i < s; i++) {
            int px = x + i, py = y + j, v = 0;
            if (px >= 0 && py >= 0 && px < pg.w && py < pg.h) {
                uint8_t b = pg.pix[(uint32_t)py * pg.stride + (uint32_t)(px >> 1)];
                v = px & 1 ? b >> 4 : b & 15;
                if (t >= 0)
                    v = v == t ? ZXV_TRANSPARENT : v == ZXV_TRANSPARENT ? 0 : v;
            }
            p[j * s + i] = (uint8_t)v;
        }
    if (texs[n].texels)
        zxb_free(texs[n].texels);
    int out = c < 0 ? 0 : t >= 0 ? (c == t ? ZXV_TRANSPARENT : c == ZXV_TRANSPARENT ? 0 : c) : c;
    texs[n] = (tex_t){ p, (uint8_t)l2, (uint8_t)(c < 0), (uint8_t)out, (uint8_t)(t >= 0 ? ZXV_M7_TRANSPARENT : 0) };
}

static tex_t *texture(int n)
{
    range(n, 0, TEXTURES - 1);
    if (!texs[n].texels)
        zxb_ERROR(ERR_INVALID_ARG);
    return &texs[n];
}

/* An angle in radians (16.16) in turns of 65536, to the nearest: PI / 2 as a
 * Fixed is a little short (102943), and it must give 16384 */
static int32_t turns(int32_t rad)
{
    int64_t num = (int64_t)rad * 2000000;               /* rad / 2·pi, in 1/65536 of a turn */
    int64_t t = (num + (num >= 0 ? 6283185 : -6283185)) / 12566370;
    return (int32_t)(t & 0xFFFF);
}

#define LINES_MAX 768                   /* the tallest page: 256 x 768 */
static zxv_m7line lines[LINES_MAX];

/* The camera of the last FLOOR or FLOORMAP and the width of its page, for
 * zxvm7.bas: NULL if there was none */
static zxv_m7cam floor_cam;
static int floor_w;

const zxv_m7cam *zxb_maps_floor_cam(int *width)
{
    *width = floor_w;
    return floor_w ? &floor_cam : 0;
}

/* The rows [y0, page height) of plane n (a texture, or a map with is_map) */
static void plane(int is_map, int n, int y0)
{
    zxv_bitmap pg = zxb_page_bitmap();
    if (y0 < 0)
        y0 = 0;
    if (y0 >= pg.h)
        return;
    zxb_gfx_begin();
    if (is_map) {
        zxv_tilemap t = tilemap(n, 1);
        zxv_m7_draw_map(&t, lines + y0, y0, pg.h - y0, ZXV_CLEAR);
    } else {
        tex_t *x = texture(n);
        zxv_m7tex t = { x->texels, x->log2, x->wrap, x->out, x->flags };
        zxv_m7_draw(&t, lines + y0, y0, pg.h - y0);
    }
    zxb_gfx_end();
}

/* FLOOR n, x, y, height, angle, horizon[, focal] (FLOORMAP: the same with
 * map n): the floor in perspective seen from point (x, y) at that height,
 * looking at angle (radians; 0 towards the growing x), with the horizon at
 * row horizon and that focal length (0: half the page) */
void zxb_floor(int is_map, int n, int32_t x, int32_t y, int32_t h, int32_t a, int horizon, int32_t focal)
{
    if (is_map)
        tilemap(n, 1);
    else
        texture(n);
    zxv_bitmap pg = zxb_page_bitmap();
    if (focal == 0)
        focal = (pg.w / 2) << 16;
    if (h <= 0 || focal <= 0)
        zxb_ERROR(ERR_INT_RANGE);
    range(horizon, -32768, 32767);
    zxv_m7cam cam = { x, y, h, turns(a), focal, (int16_t)horizon, 0 };
    floor_cam = cam;
    floor_w = pg.w;
    zxv_m7_camera(lines, pg.h, &cam);
    plane(is_map, n, horizon + 1);
}

/* ROTATE n, x, y[, angle[, zoom[, cx, cy]]] (ROTATEMAP: with map n): the
 * plane turned angle radians clockwise and zoom times bigger around its
 * point (x, y), seen at (cx, cy) of the page (-32768: its centre) */
void zxb_rotate(int is_map, int n, int32_t x, int32_t y, int32_t a, int32_t zoom, int cx, int cy)
{
    if (is_map)
        tilemap(n, 1);
    else
        texture(n);
    if (zoom <= 0)
        zxb_ERROR(ERR_INT_RANGE);
    zxv_bitmap pg = zxb_page_bitmap();
    zxv_m7aff af = { x, y, turns(a), zoom, zoom, (int16_t)(cx == -32768 ? pg.w / 2 : cx),
                     (int16_t)(cy == -32768 ? pg.h / 2 : cy) };
    zxv_m7_affine(lines, pg.h, &af);
    plane(is_map, n, 0);
}
