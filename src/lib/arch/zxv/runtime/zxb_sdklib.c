/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_sdklib.c - The optional libraries of the ZX-V SDK (sdk/lib, ZX-V D111)
 * for zxvinput.bas, zxvsave.bas, zxvfade.bas, zxvinflate.bas, zxvmath.bas,
 * zxvfont.bas, zxvmapmove.bas, zxvm7.bas (on the maps and the FLOOR of
 * zxb_maps.c) and zxv3d.bas (the models live in arrays of the BASIC, from
 * obj2c.py --bas). Their state (the controls of two players, four fonts, a
 * table of scores) stays here and the BASIC names it by number. With
 * --gc-sections, only what the program uses goes into it.
 */
#include "zxb.h"
#include "zxv_3d.h"
#include "zxv_fade.h"
#include "zxv_font.h"
#include "zxv_inflate.h"
#include "zxv_input.h"
#include "zxv_m7.h"
#include "zxv_mapmove.h"
#include "zxv_math.h"
#include "zxv_save.h"

#define ERR_INT_RANGE   10              /* B Integer out of range */
#define ERR_COLOUR      19              /* K Invalid colour */

static void range(int v, int lo, int hi)
{
    if (v < lo || v > hi)
        zxb_ERROR(ERR_INT_RANGE);
}

/* A string of the BASIC as a C one, in buf (n bytes); 0 if it does not fit */
static int c_string(zstr s, char *buf, uint32_t n)
{
    uint32_t len = zstr_len(s);
    if (len + 1 > n)
        return 0;
    for (uint32_t i = 0; i < len; i++)
        buf[i] = (char)s[2 + i];
    buf[len] = 0;
    return 1;
}

static uint32_t c_len(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

/* --- zxvinput.bas -------------------------------------------------------------- */

#define PLAYERS 2

static zxv_input_map in_map[PLAYERS];
static zxv_input in_state[PLAYERS];
static uint8_t in_ready[PLAYERS];

/* Player 0: the map of always (zxv_input_default); player 1: no keys, the
 * buttons of joystick 1 */
void zxb_ctrl_reset(int p)
{
    range(p, 0, PLAYERS - 1);
    if (p == 0) {
        in_map[0] = zxv_input_default;
    } else {
        in_map[p] = (zxv_input_map){ 0 };
        for (int a = 0; a < ZXV_IN_ACTIONS; a++)
            in_map[p].joy[a] = zxv_input_default.joy[a];
        in_map[p].pad = 1;
    }
    zxv_input_init(&in_state[p], &in_map[p]);
    in_ready[p] = 1;
}

static zxv_input *player(int p)
{
    range(p, 0, PLAYERS - 1);
    if (!in_ready[p])
        zxb_ctrl_reset(p);
    return &in_state[p];
}

/* The action of a bit (CTRL_LEFT...): its number */
static int action_of(int bit)
{
    for (int a = 0; a < ZXV_IN_ACTIONS; a++)
        if (bit == 1 << a)
            return a;
    zxb_ERROR(ERR_INVALID_ARG);
    return 0;
}

void zxb_ctrl_key(int p, int bit, int slot, int hid)
{
    player(p);
    range(slot, 0, ZXV_IN_KEYS - 1);
    range(hid, 0, 255);
    in_map[p].keys[action_of(bit)][slot] = (uint8_t)hid;
}

void zxb_ctrl_button(int p, int bit, int buttons)
{
    player(p);
    in_map[p].joy[action_of(bit)] = (uint16_t)buttons;
}

void zxb_ctrl_pad(int p, int pad)
{
    player(p);
    range(pad, -1, 1);
    in_map[p].pad = (int8_t)pad;
}

void zxb_ctrl_keep_keys(int p, int on)
{
    player(p);
    in_map[p].flags = (uint8_t)(on ? in_map[p].flags | ZXV_IN_KEEP_QUEUE : in_map[p].flags & ~ZXV_IN_KEEP_QUEUE);
}

void zxb_ctrl_update(int p) { zxv_input_update(player(p)); }
int zxb_ctrl_held(int p) { return player(p)->held; }
int zxb_ctrl_pressed(int p) { return player(p)->pressed; }
int zxb_ctrl_released(int p) { return player(p)->released; }
int zxb_ctrl_frames(int p, int bit) { return player(p)->frames[action_of(bit)]; }
int zxb_ctrl_repeat(int p, int bits, int delay, int rate) { return zxv_input_repeat(player(p), (uint16_t)bits, delay, rate); }

/* The map of player p (to save it with SAVEDATA): its address and size */
uint32_t zxb_ctrl_map(int p)
{
    player(p);
    return (uint32_t)(uintptr_t)&in_map[p];
}

int zxb_ctrl_map_size(void) { return (int)sizeof(zxv_input_map); }

int zxb_new_key(void) { return zxv_input_new_key(); }

zstr zxb_key_name(int hid)
{
    const char *s = zxv_input_key_name(hid);
    return zxb_newstr(s, c_len(s));
}

/* --- zxvsave.bas ---------------------------------------------------------------- */

#define SCORES_MAX 10

static struct {
    uint32_t n;
    zxv_score t[SCORES_MAX];
} scores = { 5, { { 0, { 0 } } } };

int zxb_save_data(zstr name, int version, uint32_t addr, uint32_t len)
{
    char f[256];
    if (!c_string(name, f, sizeof f))
        return ZXV_E_NAME;
    return zxv_save(f, (uint16_t)version, (const void *)(uintptr_t)addr, len);
}

int zxb_load_data(zstr name, int version, uint32_t addr, uint32_t len)
{
    char f[256];
    if (!c_string(name, f, sizeof f))
        return ZXV_E_NAME;
    return zxv_load(f, (uint16_t)version, (void *)(uintptr_t)addr, len);
}

uint32_t zxb_crc32(uint32_t addr, uint32_t len) { return zxv_crc32((const void *)(uintptr_t)addr, len); }

void zxb_score_clear(int n)
{
    range(n, 1, SCORES_MAX);
    scores.n = (uint32_t)n;
    for (int i = 0; i < SCORES_MAX; i++)
        scores.t[i] = (zxv_score){ 0, { 0 } };
}

int zxb_score_add(uint32_t score, zstr name)
{
    char s[8];
    uint32_t len = zstr_len(name), k = 0;
    for (; k < len && k < 7; k++)
        s[k] = (char)name[2 + k];
    s[k] = 0;
    return zxv_score_add(scores.t, (int)scores.n, score, s);
}

uint32_t zxb_score_value(int i)
{
    range(i, 0, (int)scores.n - 1);
    return scores.t[i].score;
}

zstr zxb_score_name(int i)
{
    range(i, 0, (int)scores.n - 1);
    return zxb_newstr(scores.t[i].name, c_len(scores.t[i].name));
}

int zxb_score_save(zstr name, int version)
{
    return zxb_save_data(name, version, (uint32_t)(uintptr_t)&scores, sizeof scores);
}

int zxb_score_load(zstr name, int version)
{
    return zxb_load_data(name, version, (uint32_t)(uintptr_t)&scores, sizeof scores);
}

/* --- zxvfade.bas ------------------------------------------------------------------ */

void zxb_fade_to(uint32_t addr, int frames, int tables)
{
    zxv_fade_to(addr ? (const uint16_t *)(uintptr_t)addr : 0, frames, (unsigned)tables);
}

void zxb_fade_colour(int rgb, int frames, int tables) { zxv_fade_to_colour((uint16_t)rgb, frames, (unsigned)tables); }
void zxb_fade_step(void) { zxv_fade_step(); }
int zxb_fade_busy(void) { return zxv_fade_busy(); }
void zxb_fade_wait(void) { zxv_fade_wait(); }
int zxb_rgb_mix(int a, int b, int k, int n) { return zxv_rgb_mix((uint16_t)a, (uint16_t)b, k, n); }

int zxb_rgb565(int r, int g, int b)
{
    range(r, 0, 255);
    range(g, 0, 255);
    range(b, 0, 255);
    return ZXV_RGB((uint32_t)r, (uint32_t)g, (uint32_t)b);
}

/* --- zxvinflate.bas ---------------------------------------------------------------- */

int zxb_inflate(uint32_t src, uint32_t n, uint32_t dst, uint32_t max)
{
    return zxv_inflate((const void *)(uintptr_t)src, n, (void *)(uintptr_t)dst, max);
}

int zxb_gunzip(uint32_t src, uint32_t n, uint32_t dst, uint32_t max)
{
    return zxv_gunzip((const void *)(uintptr_t)src, n, (void *)(uintptr_t)dst, max);
}

int zxb_gunzip_size(uint32_t src, uint32_t n) { return zxv_gunzip_size((const void *)(uintptr_t)src, n); }

/* --- zxvmath.bas -------------------------------------------------------------------- */

int32_t zxb_tsin(int32_t a) { return zxv_sin(a); }
int32_t zxb_tcos(int32_t a) { return zxv_cos(a); }
int32_t zxb_tatan2(int32_t y, int32_t x) { return zxv_atan2(y, x); }
uint32_t zxb_isqrt(uint32_t v) { return zxv_isqrt(v); }
uint32_t zxb_hypot(int32_t x, int32_t y) { return zxv_hypot(x, y); }
void zxb_rnd_seed(uint32_t s) { zxv_srand(s); }
uint32_t zxb_rnd_next(void) { return zxv_rand(); }
int32_t zxb_rnd_range(int32_t lo, int32_t hi) { return zxv_rand_range(lo, hi); }

/* --- zxvfont.bas -------------------------------------------------------------------- */

#define FONTS 4

static zxv_font fonts[FONTS];
static uint8_t font_ok[FONTS];
static uint8_t *font_pix[FONTS];         /* the sheet of FONTGRAB, in the heap */

static zxv_font *font(int n)
{
    range(n, 0, FONTS - 1);
    if (!font_ok[n])
        zxb_ERROR(ERR_INVALID_ARG);
    return &fonts[n];
}

static void font_free(int n)
{
    if (font_pix[n])
        zxb_free(font_pix[n]);
    font_pix[n] = 0;
    font_ok[n] = 0;
}

void zxb_font_rom(int n, int fixed)
{
    range(n, 0, FONTS - 1);
    font_free(n);
    zxv_font_rom(&fonts[n], fixed ? ZXV_FONT_FIXED : 0);
    font_ok[n] = 1;
}

/* FONTGRAB n, x, y, w, h, cw, ch, first, t, fixed: the rectangle of the
 * drawing page, cut in cells of cw x ch from character first; colour t
 * becomes transparent (and an 8 there, black). The ten arguments come as
 * an array (there are 8 registers for them) */
void zxb_font_grab(const int32_t *a)
{
    int n = a[0], x = a[1], y = a[2], w = a[3], h = a[4], cw = a[5], ch = a[6], first = a[7], t = a[8], fixed = a[9];
    range(n, 0, FONTS - 1);
    range(w, 1, 1024);
    range(h, 1, 1024);
    range(cw, 1, 255);
    range(ch, 1, 255);
    range(first, 0, 0xFFFF);
    if (t < -1 || t > 15)
        zxb_ERROR(ERR_COLOUR);
    if (cw > w || ch > h)
        zxb_ERROR(ERR_INVALID_ARG);
    font_free(n);
    uint16_t stride = (uint16_t)((w + 7) / 8 * 4);
    uint8_t *pix = zxb_alloc((uint32_t)stride * (uint32_t)h);
    zxv_bitmap page = zxb_page_bitmap();
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int px = x + i, py = y + j, c = 0;
            if (px >= 0 && py >= 0 && px < page.w && py < page.h) {
                uint8_t v = page.pix[(uint32_t)py * page.stride + (uint32_t)(px >> 1)];
                c = px & 1 ? v >> 4 : v & 15;
            }
            c = c == t ? ZXV_TRANSPARENT : c == ZXV_TRANSPARENT && t >= 0 ? 0 : c;
            uint8_t *d = &pix[(uint32_t)j * stride + (uint32_t)(i >> 1)];
            *d = (uint8_t)(i & 1 ? (*d & 0x0F) | c << 4 : (*d & 0xF0) | c);
        }
    zxv_bitmap sheet = { pix, stride, (uint16_t)w, (uint16_t)h, 0 };
    if (zxv_font_sheet(&fonts[n], &sheet, cw, ch, first, fixed ? ZXV_FONT_FIXED : 0) < 0) {
        zxb_free(pix);
        zxb_ERROR(ERR_INVALID_ARG);
    }
    font_pix[n] = pix;
    font_ok[n] = 1;
}

void zxb_font_spacing(int n, int s)
{
    range(s, 0, 255);
    font(n)->spacing = (uint8_t)s;
}

/* The text as a C string: on the stack if it is short, if not in the heap */
static const char *text_of(zstr s, char *small, uint32_t n, char **heap)
{
    uint32_t len = zstr_len(s);
    *heap = 0;
    char *buf = small;
    if (len + 1 > n)
        buf = *heap = zxb_alloc(len + 1);
    for (uint32_t i = 0; i < len; i++)
        buf[i] = (char)s[2 + i];
    buf[len] = 0;
    return buf;
}

int zxb_font_draw(int n, int x, int y, zstr text, int colour, int scale)
{
    zxv_font *f = font(n);
    if (colour < -1 || colour > 15)
        zxb_ERROR(ERR_COLOUR);
    range(scale, 1, 8);
    char small[128], *heap;
    const char *t = text_of(text, small, sizeof small, &heap);
    zxb_gfx_begin();
    int end = zxv_font_draw(f, x, y, t, colour, scale);
    zxb_gfx_end();
    if (heap)
        zxb_free(heap);
    return end;
}

int zxb_font_width(int n, zstr text, int scale)
{
    zxv_font *f = font(n);
    range(scale, 1, 8);
    char small[128], *heap;
    const char *t = text_of(text, small, sizeof small, &heap);
    int w = zxv_font_width(f, t, scale);
    if (heap)
        zxb_free(heap);
    return w;
}

int zxb_font_height(int n, zstr text, int scale)
{
    zxv_font *f = font(n);
    range(scale, 1, 8);
    char small[128], *heap;
    const char *t = text_of(text, small, sizeof small, &heap);
    int h = zxv_font_height(f, t, scale);
    if (heap)
        zxb_free(heap);
    return h;
}

/* --- zxvmapmove.bas ------------------------------------------------------------------ */

static uint8_t kinds[2][4096];           /* the type of each tile, of the sets of 8 and 16 */
static uint8_t kinds_set[2];             /* given with TILEKIND (if not: 0 empty, the rest solid) */
static uint8_t outside[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static int found_x, found_y;

void zxb_tile_kind(int s, int tile, int kind)
{
    if (s != 8 && s != 16)
        zxb_ERROR(ERR_INVALID_ARG);
    range(tile, 0, 4095);
    range(kind, 0, 255);
    int k = s == 16;
    if (!kinds_set[k]) {                                /* from the default: 0 empty, the rest solid */
        for (int i = 0; i < 4096; i++)
            kinds[k][i] = i ? ZXV_TILE_SOLID : ZXV_TILE_EMPTY;
        kinds_set[k] = 1;
    }
    kinds[k][tile] = (uint8_t)kind;
}

void zxb_map_outside(int m, int kind)
{
    range(m, 0, 15);
    range(kind, 0, 255);
    outside[m] = (uint8_t)kind;
}

/* The map m, with the types, for mapmove.c (t: where to keep the tilemap) */
static zxv_mapsolid solid(int m, zxv_tilemap *t)
{
    *t = zxb_maps_tilemap(m);
    int k = t->size == 16;
    zxv_mapsolid ms = { t, kinds_set[k] ? kinds[k] : 0, 4096, outside[m] };
    return ms;
}

int zxb_map_move(int m, int32_t *x, int32_t *y, int w, int h, int32_t dx, int32_t dy, int flags)
{
    zxv_tilemap t;
    zxv_mapsolid ms = solid(m, &t);
    range(w, 1, 32767);
    range(h, 1, 32767);
    return (int)zxv_map_move(&ms, x, y, w, h, dx, dy, (unsigned)flags);
}

int zxb_map_on_ground(int m, int32_t x, int32_t y, int w, int h, int flags)
{
    zxv_tilemap t;
    zxv_mapsolid ms = solid(m, &t);
    return zxv_map_on_ground(&ms, x, y, w, h, (unsigned)flags);
}

int zxb_map_find(int m, int32_t x, int32_t y, int w, int h, int kind)
{
    zxv_tilemap t;
    zxv_mapsolid ms = solid(m, &t);
    return zxv_map_find(&ms, x, y, w, h, kind, &found_x, &found_y);
}

int zxb_map_found_x(void) { return found_x; }
int zxb_map_found_y(void) { return found_y; }

int zxb_map_kind(int m, int px, int py)
{
    zxv_tilemap t;
    zxv_mapsolid ms = solid(m, &t);
    return zxv_map_kind(&ms, px, py);
}

/* --- zxvm7.bas ------------------------------------------------------------------------- */

static zxv_m7pos proj;
static int32_t at_x, at_y;

static const zxv_m7cam *camera(int *w)
{
    const zxv_m7cam *c = zxb_maps_floor_cam(w);
    if (!c)
        zxb_ERROR(ERR_INVALID_ARG);                     /* no FLOOR before */
    return c;
}

int zxb_floor_project(int32_t x, int32_t y, int32_t h)
{
    int w;
    const zxv_m7cam *c = camera(&w);
    return zxv_m7_project(c, w, x, y, h, &proj);
}

int zxb_floor_x(void) { return proj.x; }
int zxb_floor_y(void) { return proj.y; }
int32_t zxb_floor_scale(void) { return proj.scale; }
int32_t zxb_floor_depth(void) { return proj.depth; }

int zxb_floor_at(int sx, int sy)
{
    int w;
    const zxv_m7cam *c = camera(&w);
    return zxv_m7_floor_at(c, w, sx, sy, &at_x, &at_y);
}

int32_t zxb_floor_at_x(void) { return at_x; }
int32_t zxb_floor_at_y(void) { return at_y; }

/* --- zxv3d.bas --------------------------------------------------------------------------- */

#define MODELS3D        16

static zxv_model models3d[MODELS3D];
/* The camera of CAMERA3D (cx < 0, w = 0 or focal = 0: from the drawing page) */
static zxv_camera cam3d = { { 0, 0, 0 }, 0, 0, 0, 0, -1, -1, 0, 0, 0 };
static struct {
    int x, y;
    zxv_fix scale;
} proj3d;

/* MODEL3D n, verts, nverts, faces, nfaces, scale: the arrays of obj2c.py --bas
 * (they must stay where they are: they are not copied) */
void zxb_model3d(int n, uint32_t verts, int nverts, uint32_t faces, int nfaces, int32_t scale)
{
    range(n, 0, MODELS3D - 1);
    range(nverts, 1, ZXV_3D_MAX_VERTS);
    range(nfaces, 1, 0xFFFF);
    if (scale <= 0 || !verts || !faces)
        zxb_ERROR(ERR_INT_RANGE);
    zxv_model m = { (const int16_t (*)[3])(uintptr_t)verts, (const uint16_t *)(uintptr_t)faces, (uint16_t)nverts,
                    (uint16_t)nfaces, scale };
    models3d[n] = m;
}

/* The camera, with what was left out taken from the drawing page */
static zxv_camera camera3d(void)
{
    zxv_camera c = cam3d;
    zxv_bitmap pg = zxb_page_bitmap();
    if (c.w <= 0)
        c.w = (int16_t)pg.w;
    if (c.h <= 0)
        c.h = (int16_t)pg.h;
    if (c.cx < 0)
        c.cx = (int16_t)(c.w / 2);
    if (c.cy < 0)
        c.cy = (int16_t)(c.h / 2);
    if (c.focal <= 0)
        c.focal = c.w / 2;
    return c;
}

/* CAMERA3D x, y, z, yaw, pitch, roll, focal, cx, cy, w, h */
void zxb_camera3d(const int32_t *a)
{
    range(a[6], 0, 32767);
    range(a[7], -1, 32767);
    range(a[8], -1, 32767);
    range(a[9], 0, 4096);
    range(a[10], 0, 4096);
    zxv_camera c = { { a[0], a[1], a[2] }, a[3], a[4], a[5], a[6], (int16_t)a[7], (int16_t)a[8], (int16_t)a[9],
                     (int16_t)a[10], 0 };
    cam3d = c;
}

void zxb_light3d(int32_t x, int32_t y, int32_t z, int ambient)
{
    range(ambient, 0, 4);
    zxv_3d_light(x, y, z, ambient);
}

void zxb_wireframe3d(int colour)
{
    if (colour < -1 || colour > 15)
        zxb_ERROR(ERR_COLOUR);
    zxv_3d_wireframe(colour);
}

void zxb_begin3d(void)
{
    zxv_camera c = camera3d();
    zxv_3d_begin(&c);
}

/* ADD3D(m, x, y, z, yaw, pitch, roll, scale): the faces seen, or -1 if they
 * do not fit */
int zxb_add3d(int m, int32_t x, int32_t y, int32_t z, int32_t yaw, int32_t pitch, int32_t roll, int32_t scale)
{
    range(m, 0, MODELS3D - 1);
    if (!models3d[m].verts || scale <= 0)
        zxb_ERROR(ERR_INT_RANGE);
    zxv_xform t;
    zxv_xform_set(&t, yaw, pitch, roll, x, y, z);
    if (scale != 0x10000)
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                t.m[i][j] = (zxv_fix)(((int64_t)t.m[i][j] * scale) >> 16);
    return zxv_3d_add(&models3d[m], &t);
}

void zxb_draw3d(void)
{
    zxb_gfx_begin();
    zxv_3d_draw();
    zxb_gfx_end();
}

int zxb_project3d(int32_t x, int32_t y, int32_t z)
{
    zxv_camera c = camera3d();
    zxv_vec3 p = { x, y, z };
    return zxv_3d_project(&c, p, &proj3d.x, &proj3d.y, &proj3d.scale);
}

int zxb_proj3d_x(void) { return proj3d.x; }
int zxb_proj3d_y(void) { return proj3d.y; }
int32_t zxb_proj3d_scale(void) { return proj3d.scale; }
