/*
 * zxb_zxv.c - What is only in ZX-V, for zxv.bas: the video modes and pages,
 * scroll and colour changes, sprites, filled shapes, the keys, joysticks
 * and the AY chips. The same as the commands of the ZX-V BASIC (spec §7.8):
 * the engine of the ROM draws on the drawing page, with the y downwards, and
 * colour 8 is transparent.
 */
#include "zxb.h"
#include "zxv_gfx.h"

#define REG8(a)     (*(volatile uint8_t *)(uintptr_t)(a))
#define REG16(a)    (*(volatile uint16_t *)(uintptr_t)(a))

#define ERR_INT_RANGE   10              /* B Integer out of range */
#define ERR_COLOUR      19              /* K Invalid colour */

void zxb_ERROR(int code);

static int width = 256, height = 192;  /* Of the pages */
static int draw_page;
static int clip_on, clip_x0, clip_y0, clip_x1, clip_y1;
static uint32_t wait_frame;

static uint8_t *page_pix(int page) { return (uint8_t *)(uintptr_t)(page ? ZXV_VRAM1_BASE : ZXV_VRAM0_BASE); }

static void range(int v, int lo, int hi)
{
    if (v < lo || v > hi)
        zxb_ERROR(ERR_INT_RANGE);
}

static void colour(int c)
{
    if (c < 0 || c > 15)
        zxb_ERROR(ERR_COLOUR);
}

/* The engine draws on the drawing page, with the CLIP rectangle */
static void begin(void)
{
    zxv_bitmap b = { page_pix(draw_page), (uint16_t)(width / 2), (uint16_t)width, (uint16_t)height, 0 };
    zxv_gfx_target(&b);
    if (clip_on)
        zxv_gfx_clip(clip_x0, clip_y0, clip_x1, clip_y1);
}

static void end(void) { zxv_gfx_target(0); }

/* --- Modes and pages ------------------------------------------------------------- */

static void select_page(int n)
{
    draw_page = n;
    zxb_vram = (volatile uint32_t *)page_pix(n);
}

static void clear_page(int n)
{
    uint32_t c = (uint32_t)zxb_paper() * 0x11111111u;
    uint32_t *p = (uint32_t *)page_pix(n);
    for (uint32_t i = 0; i < ZXV_VRAM_PAGE_SIZE / 4; i++)
        p[i] = c;
}

/* MODE n[, w, h]: mode 1 (512 x 384) or mode 0 (256 x 192, or a bigger
 * page: 512 x 384, 1024 x 192 or 256 x 768). Both pages are cleared, page 0
 * is shown and drawn on, and the scroll, colour changes and CLIP go. */
void zxb_zxv_mode(int mode, int w, int h)
{
    static const uint16_t size0[4][2] = { { 256, 192 }, { 512, 384 }, { 1024, 192 }, { 256, 768 } };
    int vsize = -1;
    if (mode == 1 && (w == 0 || (w == 512 && h == 384)))
        vsize = 0;
    else if (mode == 0)
        for (int i = 0; i < 4; i++)
            if ((w == 0 && h == 0 && i == 0) || (w == size0[i][0] && h == size0[i][1]))
                vsize = i;
    if (vsize < 0)
        zxb_ERROR(ERR_INVALID_ARG);
    zxv_set_mode(mode);
    REG8(ZXV_V_VSIZE) = (uint8_t)(mode ? 0 : vsize);
    width = mode ? 512 : size0[vsize][0];
    height = mode ? 384 : size0[vsize][1];
    zxb_wpl = width / 8;
    REG16(ZXV_V_SCROLL_X) = 0;
    REG16(ZXV_V_SCROLL_Y) = 0;
    for (int t = 0; t < 4; t++)
        for (int i = 0; i < 16; i++)
            REG8(ZXV_REMAP_TABLES + t * 16 + i) = (uint8_t)i;
    REG8(ZXV_V_REMAP) = 0;
    clip_on = 0;
    for (int p = 1; p >= 0; p--) {
        select_page(p);
        clear_page(p);
        zxb_CLS();
    }
    zxv_set_page(0);
}

/* PAGE n: the page shown */
void zxb_zxv_page(int n)
{
    range(n, 0, 1);
    zxv_set_page(n);
}

/* SCREEN n: the page PRINT, PLOT, CLS, the sprites and the rest draw on */
void zxb_zxv_screen(int n)
{
    range(n, 0, 1);
    select_page(n);
}

/* SCROLL x, y: the pixel (x, y) of the page is the top left corner */
void zxb_zxv_scroll(int x, int y)
{
    REG16(ZXV_V_SCROLL_X) = (uint16_t)(((x % width) + width) % width);
    REG16(ZXV_V_SCROLL_Y) = (uint16_t)(((y % height) + height) % height);
}

/* REMAP c, n: colour c is shown as n (table 0); with c = -1, as they were */
void zxb_zxv_remap(int c, int n)
{
    if (c < 0) {
        for (int i = 0; i < 16; i++)
            REG8(ZXV_REMAP_TABLES + i) = (uint8_t)i;
        return;
    }
    colour(c);
    colour(n);
    REG8(ZXV_REMAP_TABLES + c) = (uint8_t)n;
}

/* WAIT n: at least until the next VBLANK, and until n frames have gone since
 * the last WAIT (a steady pace, whatever each turn takes) */
void zxb_zxv_wait(int n)
{
    range(n, 0, 65535);
    do
        zxv_wait_vblank();
    while (*(volatile uint32_t *)(uintptr_t)ZXV_SV_FRAMES - wait_frame < (uint32_t)n);
    wait_frame = *(volatile uint32_t *)(uintptr_t)ZXV_SV_FRAMES;
}

/* --- Sprites ----------------------------------------------------------------------- */

#define SPRITES 256

typedef struct {
    uint16_t w, h;
} spr_t;                                /* followed by the sprite of the engine */

static spr_t *sprites[SPRITES];

static const void *sprite(int n)
{
    range(n, 0, SPRITES - 1);
    if (!sprites[n])
        zxb_ERROR(ERR_INVALID_ARG);
    return sprites[n] + 1;
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

static void set_nibble(uint8_t *row, int x, int c)
{
    uint8_t *b = row + (x >> 1);
    *b = x & 1 ? (uint8_t)((*b & 0x0F) | (c << 4)) : (uint8_t)((*b & 0xF0) | c);
}

/* Sprite n becomes the bitmap b (that is freed) */
static void store(int n, zxv_bitmap *b)
{
    uint32_t size = zxv_spr_prepare(b, 0, 0, b->w, b->h, 0);
    spr_t *s = zxb_alloc(sizeof(spr_t) + size);
    s->w = b->w;
    s->h = b->h;
    zxv_spr_prepare(b, 0, 0, b->w, b->h, s + 1);
    zxb_free(b->pix);
    zxb_free(sprites[n]);
    sprites[n] = s;
}

static zxv_bitmap new_bitmap(int w, int h)
{
    uint16_t stride = (uint16_t)((w + 7) / 8 * 4);
    zxv_bitmap b = { zxb_alloc((uint32_t)stride * (uint32_t)h), stride, (uint16_t)w, (uint16_t)h, 0 };
    return b;
}

/* GRAB n, x, y, w, h[, t]: sprite n is that rectangle of the drawing page;
 * colour t (-1: none) and what is out of the page become transparent, and
 * the 8 there was, a 0 (both are black) */
void zxb_zxv_grab(int n, int x, int y, int w, int h, int t)
{
    range(n, 0, SPRITES - 1);
    range(w, 1, 1024);
    range(h, 1, 1024);
    zxv_bitmap b = new_bitmap(w, h);
    const uint8_t *pg = page_pix(draw_page);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int px = x + i, py = y + j, c = -1;
            if (px >= 0 && py >= 0 && px < width && py < height) {
                uint8_t v = pg[py * (width / 2) + (px >> 1)];
                c = px & 1 ? v >> 4 : v & 15;
            }
            set_nibble(b.pix + j * b.stride, i, c < 0 || c == t ? ZXV_TRANSPARENT : c == ZXV_TRANSPARENT ? 0 : c);
        }
    store(n, &b);
}

/* GRAB n, w, a$: the pixels of a string, w a row: the hex digits are the
 * colours and anything else is transparent */
void zxb_zxv_grab_str(int n, int w, zstr s)
{
    uint32_t len = zstr_len(s);
    range(n, 0, SPRITES - 1);
    if (w < 1 || w > 1024 || len == 0 || (len + (uint32_t)w - 1) / (uint32_t)w > 1024)
        zxb_ERROR(ERR_INVALID_ARG);
    int h = (int)((len + (uint32_t)w - 1) / (uint32_t)w);
    zxv_bitmap b = new_bitmap(w, h);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            uint32_t k = (uint32_t)(j * w + i);
            int c = k < len ? hex_colour(s[2 + k]) : -1;
            set_nibble(b.pix + j * b.stride, i, c < 0 ? ZXV_TRANSPARENT : c);
        }
    store(n, &b);
}

/* SPRITE n, x, y[, f[, c$[, w, h]]]: f mirrors it (1 left to right, 2 upside
 * down), c$ changes its colours (colour i is drawn as the hex digit i of c$)
 * and w x h scales it (0: its own size) */
void zxb_zxv_sprite(int n, int x, int y, int f, zstr c, int w, int h)
{
    const void *spr = sprite(n);
    const spr_t *hd = (const spr_t *)spr - 1;
    range(f, 0, 3);
    uint8_t lut[16];
    uint32_t len = zstr_len(c);
    for (int i = 0; i < 16; i++) {
        int v = (uint32_t)i < len ? hex_colour(c[2 + i]) : -1;
        lut[i] = (uint8_t)(v < 0 ? i : v);
    }
    if (w == 0 && h == 0) {
        w = hd->w;
        h = hd->h;
    }
    range(w, 1, 4096);
    range(h, 1, 4096);
    begin();
    zxv_spr_draw_scaled(spr, x, y, w, h, f, len ? lut : 0);
    end();
}

/* HIT(a, ax, ay, b, bx, by): 1 if a solid pixel of sprite a at (ax, ay) falls
 * on one of sprite b at (bx, by) */
int zxb_zxv_hit(int a, int ax, int ay, int b, int bx, int by)
{
    return zxv_spr_collide(sprite(a), ax, ay, sprite(b), bx, by) ? 1 : 0;
}

/* --- Shapes ------------------------------------------------------------------------ */

void zxb_zxv_box(int c, int x, int y, int w, int h)
{
    colour(c);
    begin();
    zxv_gfx_rect(x, y, w, h, c);
    end();
}

void zxb_zxv_disc(int c, int x, int y, int r)
{
    colour(c);
    begin();
    zxv_gfx_circle(x, y, r, c, 1);
    end();
}

/* POLY c, x1, y1, ...: a point, a line or a filled polygon, up to 8 points;
 * xy ends at the first x that is POLY_END */
#define POLY_END    (-32768)

void zxb_zxv_poly(int c, const int32_t *xy)
{
    colour(c);
    int16_t p[16];
    int n = 0;
    while (n < 8 && xy[2 * n] != POLY_END) {
        p[2 * n] = (int16_t)xy[2 * n];
        p[2 * n + 1] = (int16_t)xy[2 * n + 1];
        n++;
    }
    if (n == 0)
        zxb_ERROR(ERR_INVALID_ARG);
    begin();
    if (n == 1)
        zxv_gfx_rect(p[0], p[1], 1, 1, c);
    else if (n == 2)
        zxv_gfx_line(p[0], p[1], p[2], p[3], c);
    else
        zxv_gfx_poly(p, n, c);
    end();
}

/* CLIP x, y, w, h: the engine only draws there; with w < 0, everywhere */
void zxb_zxv_clip(int x, int y, int w, int h)
{
    clip_on = w >= 0;
    clip_x0 = x;
    clip_y0 = y;
    clip_x1 = x + w;
    clip_y1 = y + h;
}

/* PIXEL(x, y): the colour of a pixel of the drawing page; -1 out of it */
int zxb_zxv_pixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= width || y >= height)
        return -1;
    uint8_t v = page_pix(draw_page)[y * (width / 2) + (x >> 1)];
    return x & 1 ? v >> 4 : v & 15;
}

/* --- Keys, joysticks and sound -------------------------------------------------------- */

/* KEY(n): 1 if the key is down. n: a letter or digit (its code), 8-11 the
 * cursor keys (left, right, down, up), 12 Delete, 13 Enter, 32 Space, or
 * 256 + a USB key code */
int zxb_zxv_key(int n)
{
    int hid = -1;
    if (n >= 256 && n < 512)
        hid = n - 256;
    else if (n >= 'a' && n <= 'z')
        hid = 4 + n - 'a';
    else if (n >= 'A' && n <= 'Z')
        hid = 4 + n - 'A';
    else if (n >= '1' && n <= '9')
        hid = 30 + n - '1';
    else
        switch (n) {
        case '0': hid = 39; break;
        case 13: hid = 40; break;
        case 32: hid = 44; break;
        case 8: hid = 0x50; break;
        case 9: hid = 0x4F; break;
        case 10: hid = 0x51; break;
        case 11: hid = 0x52; break;
        case 12: hid = 42; break;
        }
    if (hid < 0)
        zxb_ERROR(ERR_INVALID_ARG);
    return (REG8(ZXV_I_KEYMAP + (hid >> 3)) >> (hid & 7)) & 1;
}

/* JOY(n): the buttons of joystick n (0 or 1), 0 if it is not there */
int zxb_zxv_joy(int n)
{
    range(n, 0, 1);
    uint32_t j = zxv_joy_read(n);
    return j & ZXV_JOY_CONNECTED ? (int)(j & 0x1FF) : 0;
}

/* SOUND r, v: AY register r (0-15 the first chip, 16-31 the second) */
void zxb_zxv_sound(int r, int v)
{
    range(r, 0, 31);
    range(v, 0, 255);
    zxv_ay_write(r < 16 ? r : ZXV_AY2 + r - 16, v);
}

/* TIME: the frames since the machine started */
uint32_t zxb_zxv_time(void) { return *(volatile uint32_t *)(uintptr_t)ZXV_SV_FRAMES; }
