/*
 * zxb_screen.c - A ZX Spectrum screen on ZX-V: 256 x 192 pixels and 32 x 24
 * attributes, where the Spectrum colours are the first 16 of the palette
 * (ink + 8 * BRIGHT). It is drawn on the top left corner of the drawing
 * page (page 0, or the one SCREEN of zxv.bas chooses), in any mode.
 *
 * The bitmap is linear here; the Spectrum memory map (16384-23295, the
 * system variables and the rest up to 65535) is emulated for PEEK and POKE,
 * as the ZX-V BASIC does.
 */
#include "zxb.h"

#define COLS        32
#define ROWS        24
volatile uint32_t *zxb_vram = (volatile uint32_t *)(uintptr_t)ZXV_VRAM0_BASE;  /* The drawing page */
int zxb_wpl = 32;                   /* Its 32-bit words per line (256 pixels) */

#define CHARS_DEF   15360           /* CHARS: the font is at CHARS + 256 */
#define UDG_DEF     65368
#define LOW_BASE    23296           /* System variables and the rest */

extern const uint8_t zxb_font[96 * 8];

static uint8_t bitmap[192 * COLS];
static uint8_t attrs[ROWS * COLS];
static uint8_t lowmem[65536 - LOW_BASE];
static uint32_t expand[256];        /* Byte -> a nibble 0xF for each pixel set */

static int row, col;                /* Print position */
static int coords_x, coords_y;      /* Last PLOT */
static uint8_t attr_p = 56, mask_p, attr_t = 56, mask_t;
static uint8_t flags_p, flags_t;    /* OVER (1), INVERSE (2), INK 9 (4), PAPER 9 (8) */
static uint8_t border = 7;
static int ctrl, ctrl_arg;          /* Pending control code and its first parameter */

#define F_OVER      1
#define F_INVERSE   2
#define F_INK9      4
#define F_PAPER9    8

#define LOW(a)      lowmem[(a) - LOW_BASE]

static uint32_t low16(uint32_t a) { return LOW(a) | (uint32_t)LOW(a + 1) << 8; }

static void paint_line(int c, int y)
{
    uint8_t a = attrs[(y >> 3) * COLS + c];
    uint32_t bright = (a & 0x40) >> 3;
    uint32_t ink = ((a & 7) | bright) * 0x11111111u;
    uint32_t paper = (((a >> 3) & 7) | bright) * 0x11111111u;
    uint32_t m = expand[bitmap[y * COLS + c]];
    zxb_vram[y * zxb_wpl + c] = (ink & m) | (paper & ~m);
}

static void paint_cell(int c, int r)
{
    for (int y = r * 8; y < r * 8 + 8; y++)
        paint_line(c, y);
}

static void paint_all(void)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            paint_cell(c, r);
}

/* The attribute of a cell after printing or plotting on it */
static uint8_t new_attr(uint8_t old)
{
    uint8_t a = (uint8_t)((attr_t & ~mask_t) | (old & mask_t));
    if (flags_t & F_INK9)
        a = (uint8_t)((a & ~7) | (((a >> 3) & 7) < 4 ? 7 : 0));
    if (flags_t & F_PAPER9)
        a = (uint8_t)((a & ~0x38) | ((a & 7) < 4 ? 7 << 3 : 0));
    return a;
}

void zxb_screen_init(void)
{
    for (int b = 0; b < 256; b++) {
        uint32_t m = 0;
        for (int i = 0; i < 8; i++)
            if (b & (0x80 >> i))
                m |= 0xFu << (4 * i);
        expand[b] = m;
    }
    LOW(23606) = CHARS_DEF & 0xFF;
    LOW(23607) = CHARS_DEF >> 8;
    LOW(23675) = UDG_DEF & 0xFF;
    LOW(23676) = UDG_DEF >> 8;
    for (int i = 0; i < 21 * 8; i++)            /* The UDG are A to U, as in the Spectrum */
        LOW(UDG_DEF + i) = zxb_font[('A' - 32) * 8 + i];
    zxv_set_mode(0);
    zxv_set_page(0);
    *(volatile uint8_t *)(uintptr_t)ZXV_V_BORDER = border;
}

/* --- Attributes ------------------------------------------------------------ */

/* INK, PAPER... n: perm selects the permanent or the temporary ones */
static void set_colour(int perm, int shift, int n)
{
    uint8_t *a = perm ? &attr_p : &attr_t, *m = perm ? &mask_p : &mask_t, *f = perm ? &flags_p : &flags_t;
    uint8_t bits = shift == 0 ? 7 : 0x38;
    uint8_t nine = shift == 0 ? F_INK9 : F_PAPER9;
    *f &= (uint8_t)~nine;
    *m &= (uint8_t)~bits;
    if (n == 8)
        *m |= bits;
    else if (n == 9)
        *f |= nine;
    else
        *a = (uint8_t)((*a & ~bits) | ((n & 7) << shift));
}

static void set_bit(int perm, uint8_t bit, int n)
{
    uint8_t *a = perm ? &attr_p : &attr_t, *m = perm ? &mask_p : &mask_t;
    *m &= (uint8_t)~bit;
    if (n == 8)
        *m |= bit;
    else
        *a = (uint8_t)(n ? *a | bit : *a & ~bit);
}

static void set_flag(int perm, uint8_t flag, int n)
{
    uint8_t *f = perm ? &flags_p : &flags_t;
    *f = (uint8_t)(n ? *f | flag : *f & ~flag);
}

void zxb_INK(int n) { set_colour(1, 0, n); }
void zxb_PAPER(int n) { set_colour(1, 3, n); }
void zxb_BRIGHT(int n) { set_bit(1, 0x40, n); }
void zxb_FLASH(int n) { set_bit(1, 0x80, n); }
void zxb_OVER(int n) { set_flag(1, F_OVER, n); }
void zxb_INVERSE(int n) { set_flag(1, F_INVERSE, n); }
void zxb_INK_TMP(int n) { set_colour(0, 0, n); }
void zxb_PAPER_TMP(int n) { set_colour(0, 3, n); }
void zxb_BRIGHT_TMP(int n) { set_bit(0, 0x40, n); }
void zxb_FLASH_TMP(int n) { set_bit(0, 0x80, n); }
void zxb_OVER_TMP(int n) { set_flag(0, F_OVER, n); }
void zxb_INVERSE_TMP(int n) { set_flag(0, F_INVERSE, n); }
void zxb_BOLD(int n) { (void)n; }
void zxb_ITALIC(int n) { (void)n; }
void zxb_BOLD_TMP(int n) { (void)n; }
void zxb_ITALIC_TMP(int n) { (void)n; }

void zxb_COPY_ATTR(void)
{
    attr_t = attr_p;
    mask_t = mask_p;
    flags_t = flags_p;
}

void zxb_BORDER(int n)
{
    border = (uint8_t)(n & 7);
    *(volatile uint8_t *)(uintptr_t)ZXV_V_BORDER = border;
}

/* The colour of the paper (0-15), for zxv.bas */
int zxb_paper(void) { return ((attr_p >> 3) & 7) | ((attr_p & 0x40) >> 3); }

void zxb_CLS(void)
{
    __builtin_memset(bitmap, 0, sizeof(bitmap));
    __builtin_memset(attrs, attr_p, sizeof(attrs));
    paint_all();
    row = col = 0;
    coords_x = coords_y = 0;
}

/* --- Text -------------------------------------------------------------------- */

static void scroll(void)
{
    __builtin_memmove(bitmap, bitmap + 8 * COLS, sizeof(bitmap) - 8 * COLS);
    __builtin_memset(bitmap + sizeof(bitmap) - 8 * COLS, 0, 8 * COLS);
    __builtin_memmove(attrs, attrs + COLS, sizeof(attrs) - COLS);
    __builtin_memset(attrs + sizeof(attrs) - COLS, attr_p, COLS);
    paint_all();
}

static void newline(void)
{
    col = 0;
    if (++row >= ROWS) {
        scroll();
        row = ROWS - 1;
    }
}

static void glyph(int c, uint8_t g[8])
{
    uint32_t chars = low16(23606), udg = low16(23675);
    if (c >= 32 && c < 128) {
        for (int i = 0; i < 8; i++)
            g[i] = chars == CHARS_DEF ? zxb_font[(c - 32) * 8 + i] : zxb_vpeek(chars + (uint32_t)c * 8 + (uint32_t)i);
    } else if (c >= 128 && c < 144) {   /* Block graphics */
        uint8_t top = (uint8_t)((c & 2 ? 0xF0 : 0) | (c & 1 ? 0x0F : 0));
        uint8_t bottom = (uint8_t)((c & 8 ? 0xF0 : 0) | (c & 4 ? 0x0F : 0));
        for (int i = 0; i < 8; i++)
            g[i] = i < 4 ? top : bottom;
    } else if (c >= 144 && c < 165) {   /* UDG */
        for (int i = 0; i < 8; i++)
            g[i] = zxb_vpeek(udg + (uint32_t)(c - 144) * 8 + (uint32_t)i);
    } else {
        for (int i = 0; i < 8; i++)
            g[i] = zxb_font[('?' - 32) * 8 + i];
    }
}

static void put_glyph(int c)
{
    uint8_t g[8];
    glyph(c, g);
    if (col >= COLS)
        newline();
    for (int i = 0; i < 8; i++) {
        uint8_t b = g[i];
        if (flags_t & F_INVERSE)
            b = (uint8_t)~b;
        uint8_t *p = &bitmap[(row * 8 + i) * COLS + col];
        *p = flags_t & F_OVER ? (uint8_t)(*p ^ b) : b;
    }
    attrs[row * COLS + col] = new_attr(attrs[row * COLS + col]);
    paint_cell(col, row);
    col++;
}

void zxb_print_char(int c)
{
    c &= 0xFF;
    if (ctrl) {                         /* Parameter of a control code */
        int code = ctrl;
        if ((code == 22 || code == 23) && ctrl_arg < 0) {
            ctrl_arg = c;
            return;
        }
        ctrl = 0;
        switch (code) {
        case 16: zxb_INK_TMP(c); break;
        case 17: zxb_PAPER_TMP(c); break;
        case 18: zxb_FLASH_TMP(c); break;
        case 19: zxb_BRIGHT_TMP(c); break;
        case 20: zxb_INVERSE_TMP(c); break;
        case 21: zxb_OVER_TMP(c); break;
        case 22: row = ctrl_arg % ROWS; col = c % COLS; break;
        case 23: {
            int target = (ctrl_arg | c << 8) % COLS;
            if (target < col)
                newline();
            while (col < target)
                put_glyph(' ');
            break;
        }
        }
        return;
    }
    if (c >= 16 && c <= 23) {
        ctrl = c;
        ctrl_arg = -1;
        return;
    }
    if (c == 13) {
        zxb_echo("\n", 1);
        newline();
        return;
    }
    if (c == 8) {                       /* Back one position */
        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;
            col = COLS - 1;
        }
        return;
    }
    if (c < 32)
        return;
    char e = c < 128 ? (char)c : '?';     /* The console only gets ASCII */
    zxb_echo(&e, 1);
    put_glyph(c);
}

void zxb_print_text(const char *s, int n)
{
    for (int i = 0; i < n; i++)
        zxb_print_char((uint8_t)s[i]);
}

void zxb_PRINTSTR(zstr s, int owned)
{
    uint32_t n = zstr_len(s);
    for (uint32_t i = 0; i < n; i++)
        zxb_print_char(s[2 + i]);
    if (owned)
        zxb_free(s);
}

static void print_uint(uint32_t v, int neg)
{
    char buf[12];
    int n = 0;
    do {
        buf[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    if (neg)
        zxb_print_char('-');
    while (n)
        zxb_print_char(buf[--n]);
}

void zxb_PRINTU8(uint32_t v) { print_uint(v, 0); }
void zxb_PRINTU16(uint32_t v) { print_uint(v, 0); }
void zxb_PRINTU32(uint32_t v) { print_uint(v, 0); }
void zxb_PRINTI8(int32_t v) { print_uint(v < 0 ? (uint32_t)-v : (uint32_t)v, v < 0); }
void zxb_PRINTI16(int32_t v) { print_uint(v < 0 ? (uint32_t)-v : (uint32_t)v, v < 0); }
void zxb_PRINTI32(int32_t v) { print_uint(v < 0 ? 0u - (uint32_t)v : (uint32_t)v, v < 0); }

void zxb_PRINTF(double x)
{
    char buf[24];
    zxb_print_text(buf, fp_format(x, buf));
}

void zxb_PRINTF16(int32_t x)
{
    zxb_PRINTF((double)x / 65536.0);
}

void zxb_PRINT_EOL(void)
{
    zxb_print_char(13);
}

void zxb_PRINT_COMMA(void)
{
    int target = col < 16 ? 16 : 32;
    while (col < target)
        put_glyph(' ');
}

void zxb_PRINT_TAB(int n)
{
    zxb_print_char(23);
    zxb_print_char(n & 0xFF);
    zxb_print_char(0);
}

void zxb_PRINT_AT(int c, int r)
{
    if (r >= ROWS || c >= COLS)
        zxb_ERROR(ERR_OUT_OF_SCREEN);
    row = r;
    col = c;
}

/* The bottom line, with the colours of the border (as the lower screen of the
 * Spectrum), gets a report */
void zxb_report(const char *msg)
{
    attr_t = (uint8_t)(border << 3 | (border < 4 ? 7 : 0));
    mask_t = flags_t = 0;
    row = ROWS - 1;
    col = 0;
    __builtin_memset(bitmap + (ROWS - 1) * 8 * COLS, 0, 8 * COLS);
    for (int c = 0; c < COLS; c++) {
        attrs[(ROWS - 1) * COLS + c] = attr_t;
        paint_cell(c, ROWS - 1);
    }
    while (*msg && col < COLS)
        put_glyph((uint8_t)*msg++);
}

/* --- Graphics: (0, 0) is the bottom left corner ------------------------------- */

static void plot(int x, int y)
{
    if (x < 0 || x > 255 || y < 0 || y > 191)
        zxb_ERROR(ERR_OUT_OF_SCREEN);
    int py = 191 - y, c = x >> 3;
    uint8_t bit = (uint8_t)(0x80 >> (x & 7));
    uint8_t *p = &bitmap[py * COLS + c];
    if (flags_t & F_OVER)
        *p ^= bit;
    else if (flags_t & F_INVERSE)
        *p &= (uint8_t)~bit;
    else
        *p |= bit;
    uint8_t *a = &attrs[(py >> 3) * COLS + c];
    uint8_t na = new_attr(*a);
    if (na != *a) {
        *a = na;
        paint_cell(c, py >> 3);
    } else {
        paint_line(c, py);
    }
    coords_x = x;
    coords_y = y;
}

void zxb_PLOT(int y, int x)
{
    plot(x, y);
}

static void line_to(int x1, int y1)
{
    int x0 = coords_x, y0 = coords_y;
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (x0 != x1 || y0 != y1) {
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
        plot(x0, y0);
    }
}

void zxb_DRAW(int dy, int dx)
{
    line_to(coords_x + dx, coords_y + dy);
}

/* DRAW dx, dy, a: an arc that turns a radians */
void zxb_DRAW3(double a, int dy, int dx)
{
    double fa = b_fabs(a);
    if (fa < 1e-4 || (dx == 0 && dy == 0)) {
        zxb_DRAW(dy, dx);
        return;
    }
    double x0 = coords_x, y0 = coords_y;
    double half = b_sin(a / 2);
    /* Centre: on the perpendicular bisector of the chord */
    double mx = x0 + dx / 2.0, my = y0 + dy / 2.0;
    double k = b_cos(a / 2) / (2 * half);
    double cx = mx - dy * k, cy = my + dx * k;
    double r = b_sqrt((x0 - cx) * (x0 - cx) + (y0 - cy) * (y0 - cy));
    double start = b_atan((y0 - cy) / ((x0 - cx) == 0 ? 1e-12 : x0 - cx));
    if (x0 - cx < 0)
        start += 3.14159265358979323846;
    int n = (int)(fa * r / 4) + 4;
    int ex = coords_x + dx, ey = coords_y + dy;
    for (int i = 1; i <= n; i++) {
        double t = start + a * i / n;
        int x = i == n ? ex : (int)(cx + r * b_cos(t) + (cx + r * b_cos(t) < 0 ? -0.5 : 0.5));
        int y = i == n ? ey : (int)(cy + r * b_sin(t) + (cy + r * b_sin(t) < 0 ? -0.5 : 0.5));
        line_to(x, y);
    }
}

void zxb_CIRCLE(int r, int y, int x)
{
    r &= 0xFF;
    y &= 0xFF;
    x &= 0xFF;
    int px = r, py = 0, err = 1 - r;
    while (px >= py) {
        int pts[8][2] = {
            { x + px, y + py }, { x + py, y + px }, { x - py, y + px }, { x - px, y + py },
            { x - px, y - py }, { x - py, y - px }, { x + py, y - px }, { x + px, y - py },
        };
        for (int i = 0; i < 8; i++)
            plot(pts[i][0], pts[i][1]);
        py++;
        if (err < 0) {
            err += 2 * py + 1;
        } else {
            px--;
            err += 2 * (py - px) + 1;
        }
    }
}

/* --- For the libraries -------------------------------------------------------- */

int zxb_lib_pos(void) { return col; }
int zxb_lib_csrlin(void) { return row; }

/* POINT (x, y): 1 if the pixel is set, -1 out of the screen */
int zxb_lib_point(int x, int y)
{
    x &= 0xFF;
    y &= 0xFF;
    if (y > 191)
        return -1;
    return (bitmap[(191 - y) * COLS + (x >> 3)] >> (7 - (x & 7))) & 1;
}

/* SCREEN$ (row, col): the character in that cell, normal or inverse, or "" */
zstr zxb_lib_screen(int r, int c)
{
    r &= 0xFF;
    c &= 0xFF;
    if (r >= ROWS || c >= COLS)
        return NULL;
    for (int ch = 32; ch < 128; ch++) {
        uint8_t g[8];
        glyph(ch, g);
        int normal = 1, inverse = 1;
        for (int i = 0; i < 8; i++) {
            uint8_t b = bitmap[(r * 8 + i) * COLS + c];
            normal &= b == g[i];
            inverse &= b == (uint8_t)~g[i];
        }
        if (normal || inverse) {
            uint8_t s = (uint8_t)ch;
            return zxb_newstr(&s, 1);
        }
    }
    return NULL;
}

/* The cells of a box, clipped to the screen */
#define FOR_BOX(x, y, w, h) \
    for (int r_ = (y); r_ < (y) + (h) && r_ < ROWS; r_++) \
        for (int c_ = (x); c_ < (x) + (w) && c_ < COLS; c_++)

void zxb_lib_clearbox(int x, int y, int w, int h)
{
    FOR_BOX(x & 0xFF, y & 0xFF, w & 0xFF, h & 0xFF) {
        for (int i = 0; i < 8; i++)
            bitmap[(r_ * 8 + i) * COLS + c_] = 0;
        paint_cell(c_, r_);
    }
}

/* putChars / getChars: 8 bytes per cell, column by column (each one from top to bottom) */
void zxb_lib_putchars(int x, int y, int w, int h, uint32_t data, int get)
{
    x &= 0xFF, y &= 0xFF, w &= 0xFF, h &= 0xFF;
    for (int c = x; c < x + w; c++)
        for (int r = y; r < y + h; r++, data += 8) {
            if (r >= ROWS || c >= COLS)
                continue;
            for (int i = 0; i < 8; i++) {
                uint8_t *p = &bitmap[(r * 8 + i) * COLS + c];
                if (get)
                    zxb_vpoke(data + (uint32_t)i, *p);
                else
                    *p = zxb_vpeek(data + (uint32_t)i);
            }
            if (!get)
                paint_cell(c, r);
        }
}

/* paint: one attribute for the box; paintData / getPaintData: one byte per cell, row by row */
void zxb_lib_paint(int x, int y, int w, int h, int attr)
{
    FOR_BOX(x & 0xFF, y & 0xFF, w & 0xFF, h & 0xFF) {
        attrs[r_ * COLS + c_] = (uint8_t)attr;
        paint_cell(c_, r_);
    }
}

void zxb_lib_paintdata(int x, int y, int w, int h, uint32_t data, int get)
{
    x &= 0xFF, y &= 0xFF, w &= 0xFF, h &= 0xFF;
    for (int r = y; r < y + h; r++)
        for (int c = x; c < x + w; c++, data++) {
            if (r >= ROWS || c >= COLS)
                continue;
            if (get) {
                zxb_vpoke(data, attrs[r * COLS + c]);
            } else {
                attrs[r * COLS + c] = zxb_vpeek(data);
                paint_cell(c, r);
            }
        }
}

/* scroll.bas: moves 1 pixel the window from (x1, y1) to (x2, y2), both included,
 * with the PLOT coordinates (y = 0 at the bottom). dir: 0 right, 1 left, 2 up,
 * 3 down. What comes in is blank; the attributes do not change.
 * A pixel row is 8 words, pixel 0 the highest bit of the first one. */
static void row_get(int y, uint32_t w[8])
{
    const uint8_t *p = &bitmap[y * COLS];
    for (int i = 0; i < 8; i++, p += 4)
        w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void row_put(int y, const uint32_t w[8], const uint32_t mask[8], int c1, int c2)
{
    uint8_t *p = &bitmap[y * COLS];
    for (int i = 0; i < 8; i++, p += 4) {
        uint32_t old = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
        uint32_t v = (old & ~mask[i]) | (w[i] & mask[i]);
        p[0] = (uint8_t)(v >> 24);
        p[1] = (uint8_t)(v >> 16);
        p[2] = (uint8_t)(v >> 8);
        p[3] = (uint8_t)v;
    }
    for (int c = c1; c <= c2; c++)
        paint_line(c, y);
}

void zxb_lib_scroll(int x1, int y1, int x2, int y2, int dir)
{
    x1 &= 0xFF, y1 &= 0xFF, x2 &= 0xFF, y2 &= 0xFF;
    if (x1 > x2 || y1 > y2 || y1 > 191)
        return;
    if (y2 > 191)
        y2 = 191;
    uint32_t mask[8] = { 0 }, w[8], zero[8] = { 0 };
    for (int x = x1; x <= x2; x++)
        mask[x >> 5] |= 0x80000000u >> (x & 31);
    int top = 191 - y2, bottom = 191 - y1, c1 = x1 >> 3, c2 = x2 >> 3;

    switch (dir) {
    case 0:                             /* Right: each pixel takes the one on its left */
    case 1:                             /* Left */
        for (int y = top; y <= bottom; y++) {
            row_get(y, w);
            if (dir == 0) {
                for (int i = 7; i > 0; i--)
                    w[i] = w[i] >> 1 | w[i - 1] << 31;
                w[0] >>= 1;
                w[x1 >> 5] &= ~(0x80000000u >> (x1 & 31));
            } else {
                for (int i = 0; i < 7; i++)
                    w[i] = w[i] << 1 | w[i + 1] >> 31;
                w[7] <<= 1;
                w[x2 >> 5] &= ~(0x80000000u >> (x2 & 31));
            }
            row_put(y, w, mask, c1, c2);
        }
        break;
    case 2:                             /* Up: each row takes the one below */
        for (int y = top; y < bottom; y++) {
            row_get(y + 1, w);
            row_put(y, w, mask, c1, c2);
        }
        row_put(bottom, zero, mask, c1, c2);
        break;
    case 3:                             /* Down */
        for (int y = bottom; y > top; y--) {
            row_get(y - 1, w);
            row_put(y, w, mask, c1, c2);
        }
        row_put(top, zero, mask, c1, c2);
        break;
    }
}

/* winscroll.bas: moves 1 cell the window of width x height cells at (row, col),
 * with its attributes. dir: 0 right, 1 left, 2 up, 3 down. The cells that are
 * left blank keep their attributes. */
static void copy_cell(int dc, int dr, int sc, int sr)
{
    for (int i = 0; i < 8; i++)
        bitmap[(dr * 8 + i) * COLS + dc] = bitmap[(sr * 8 + i) * COLS + sc];
    attrs[dr * COLS + dc] = attrs[sr * COLS + sc];
}

static void blank_cell(int c, int r)
{
    for (int i = 0; i < 8; i++)
        bitmap[(r * 8 + i) * COLS + c] = 0;
}

void zxb_lib_winscroll(int r0, int c0, int w, int h, int dir)
{
    r0 &= 0xFF, c0 &= 0xFF, w &= 0xFF, h &= 0xFF;
    if (r0 >= ROWS || c0 >= COLS || !w || !h)
        return;
    if (r0 + h > ROWS)
        h = ROWS - r0;
    if (c0 + w > COLS)
        w = COLS - c0;
    int r1 = r0 + h - 1, c1 = c0 + w - 1;

    for (int r = r0; r <= r1; r++) {
        if (dir == 0) {
            for (int c = c1; c > c0; c--)
                copy_cell(c, r, c - 1, r);
            blank_cell(c0, r);
        } else if (dir == 1) {
            for (int c = c0; c < c1; c++)
                copy_cell(c, r, c + 1, r);
            blank_cell(c1, r);
        }
    }
    if (dir == 2) {
        for (int r = r0; r < r1; r++)
            for (int c = c0; c <= c1; c++)
                copy_cell(c, r, c, r + 1);
        for (int c = c0; c <= c1; c++)
            blank_cell(c, r1);
    } else if (dir == 3) {
        for (int r = r1; r > r0; r--)
            for (int c = c0; c <= c1; c++)
                copy_cell(c, r, c, r - 1);
        for (int c = c0; c <= c1; c++)
            blank_cell(c, r0);
    }
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++)
            paint_cell(c, r);
}

/* The Spectrum ports: IN of the keyboard (xxFEh) and the Kempston joystick (1Fh);
 * OUT to FEh sets the border */
uint32_t zxb_IN(uint32_t port)
{
    port &= 0xFFFF;
    if ((port & 0xFF) == 0xFE) {
        uint8_t v = 0x1F;
        for (int i = 0; i < 8; i++)
            if (!(port & (0x100u << i)))
                v &= *(volatile uint8_t *)(uintptr_t)(ZXV_I_KEYROWS + (uint32_t)i);
        return v | 0xE0u;
    }
    if ((port & 0xFF) == 0x1F)
        return zxv_joy_read(0) & 0x1F;
    return 0xFF;
}

void zxb_OUT(uint32_t port, uint32_t v)
{
    if ((port & 0xFF) == 0xFE)
        zxb_BORDER((int)(v & 7));
}

/* --- The Spectrum memory below 64K ------------------------------------------ */

static int spec_screen_pos(uint32_t addr, int *y, int *xb)
{
    uint32_t o = addr - 16384u;
    *y = (int)(((o >> 5) & 0xC0) | ((o >> 8) & 0x07) | ((o >> 2) & 0x38));
    *xb = (int)(o & 31);
    return *y < 192;
}

uint8_t zxb_vpeek(uint32_t addr)
{
    if (addr >= 0x10000)
        return *(volatile uint8_t *)(uintptr_t)addr;
    if (addr < 16384)
        return addr >= CHARS_DEF + 256 ? zxb_font[addr - CHARS_DEF - 256] : 0;
    if (addr < 22528) {
        int y, xb;
        return spec_screen_pos(addr, &y, &xb) ? bitmap[y * COLS + xb] : 0;
    }
    if (addr < LOW_BASE)
        return attrs[addr - 22528];
    uint32_t f = *(volatile uint32_t *)(uintptr_t)ZXV_SV_FRAMES;
    switch (addr) {
    case 23624: return (uint8_t)(border << 3);
    case 23672: case 23673: case 23674: return (uint8_t)(f >> (8 * (addr - 23672)));
    case 23677: return (uint8_t)coords_x;
    case 23678: return (uint8_t)coords_y;
    case 23688: return (uint8_t)(33 - col);
    case 23689: return (uint8_t)(24 - row);
    case 23693: return attr_p;
    case 23694: return mask_p;
    case 23695: return attr_t;
    case 23696: return mask_t;
    }
    return LOW(addr);
}

void zxb_vpoke(uint32_t addr, uint8_t v)
{
    if (addr >= 0x10000) {
        *(volatile uint8_t *)(uintptr_t)addr = v;
        return;
    }
    if (addr < 16384)
        return;                         /* ROM */
    if (addr < 22528) {
        int y, xb;
        if (spec_screen_pos(addr, &y, &xb)) {
            bitmap[y * COLS + xb] = v;
            paint_line(xb, y);
        }
        return;
    }
    if (addr < LOW_BASE) {
        uint32_t i = addr - 22528;
        attrs[i] = v;
        paint_cell((int)(i % COLS), (int)(i / COLS));
        return;
    }
    switch (addr) {
    case 23624: zxb_BORDER(v >> 3); break;
    case 23677: coords_x = v; break;
    case 23678: coords_y = v; break;
    case 23693: attr_p = v; break;
    case 23694: mask_p = v; break;
    case 23695: attr_t = v; break;
    case 23696: mask_t = v; break;
    }
    LOW(addr) = v;
}

/* Accesses from the compiled code below 64K, or misaligned (zxb_core.S) */
uint64_t zxb_vread(uint32_t addr, int size)
{
    uint64_t v = 0;
    for (int i = 0; i < size; i++)
        v |= (uint64_t)zxb_vpeek(addr + (uint32_t)i) << (8 * i);
    return v;
}

void zxb_vwrite(uint32_t addr, uint32_t lo, uint32_t hi, int size)
{
    uint64_t v = lo | (uint64_t)hi << 32;
    for (int i = 0; i < size; i++)
        zxb_vpoke(addr + (uint32_t)i, (uint8_t)(v >> (8 * i)));
}
