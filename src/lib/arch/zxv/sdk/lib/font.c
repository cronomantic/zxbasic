/*
 * font.c - Texto con fuentes propias (include/zxv_font.h).
 *
 * Opcional: añade $(SDK)/lib/font.c y $(SDK)/lib/font_rom.c a tu programa
 * (font_rom.c hace falta siempre: font.c lo usa aunque no llames a
 * zxv_font_rom).
 *
 * Cada fila de un carácter se dibuja por tramos del mismo color con
 * zxv_gfx_rect: ampliar es hacer los rectángulos más grandes, y el motor
 * recorta y pone la trama. En las proporcionales, cada carácter empieza en
 * su primera columna con tinta y avanza lo que mide (el espacio, media
 * celda).
 */
#include "zxv_font.h"

#ifdef ZXV_LIB_HOST
/* Las pruebas en el PC (emulator/tests/sdklib/font_host.c): el motor de la
 * ROM compilado para el PC. */
#include "gfx.h"
#define zxv_gfx_rect gfx_rect
#endif

extern const unsigned zxv_font_rom_n;
extern const uint16_t zxv_font_rom_codes[];
extern const uint8_t zxv_font_rom_bits[];

/* El color del píxel (c, r) del carácter i; 8 (ZXV_TRANSPARENT) si no hay. */
static int pixel(const zxv_font *f, int i, int c, int r)
{
    if (f->bits)
        return (f->bits[i * f->h + r] >> (7 - c)) & 1 ? 15 : ZXV_TRANSPARENT;
    int cols = f->sheet.w / f->w;
    int x = (i % cols) * f->w + c, y = (i / cols) * f->h + r;
    uint8_t b = f->sheet.pix[(uint32_t)y * f->sheet.stride + (uint32_t)(x >> 1)];
    return (x & 1) ? b >> 4 : b & 15;
}

/* Las medidas de cada carácter. */
static void metrics(zxv_font *f)
{
    int fixed = f->flags & ZXV_FONT_FIXED;
    f->spacing = fixed ? 0 : 1;
    for (int i = 0; i < f->count; i++) {
        int lo = f->w, hi = -1;
        if (!fixed)
            for (int r = 0; r < f->h; r++)
                for (int c = 0; c < f->w; c++)
                    if (pixel(f, i, c, r) != ZXV_TRANSPARENT) {
                        lo = c < lo ? c : lo;
                        hi = c > hi ? c : hi;
                    }
        if (fixed) {
            f->left[i] = 0;
            f->adv[i] = f->w;
        } else if (hi < 0) {                            /* sin tinta: el espacio */
            f->left[i] = 0;
            f->adv[i] = (uint8_t)(f->w / 2 > 1 ? f->w / 2 : 1);
        } else {
            f->left[i] = (uint8_t)lo;
            f->adv[i] = (uint8_t)(hi - lo + 1);
        }
    }
}

void zxv_font_rom(zxv_font *f, unsigned flags)
{
    zxv_font_bits(f, zxv_font_rom_bits, zxv_font_rom_codes, (int)zxv_font_rom_n, 8, flags);
}

int zxv_font_bits(zxv_font *f, const uint8_t *bits, const uint16_t *codes, int n, int h, unsigned flags)
{
    if (!f || !bits || n <= 0 || n > ZXV_FONT_CHARS || h < 1 || h > 255)
        return -1;
    f->sheet.pix = 0;
    f->bits = bits;
    f->codes = codes;
    f->first = codes ? codes[0] : 32;
    f->count = (uint16_t)n;
    f->w = 8;
    f->h = (uint8_t)h;
    f->flags = (uint8_t)flags;
    metrics(f);
    return 0;
}

int zxv_font_sheet(zxv_font *f, const zxv_bitmap *sheet, int cell_w, int cell_h, int first, unsigned flags)
{
    if (!f || !sheet || !sheet->pix || cell_w < 1 || cell_h < 1 || cell_w > 255 || cell_h > 255 || first < 0 ||
        sheet->w < cell_w || sheet->h < cell_h)
        return -1;
    int n = (sheet->w / cell_w) * (sheet->h / cell_h);
    f->sheet = *sheet;
    f->bits = 0;
    f->codes = 0;
    f->first = (uint16_t)first;
    f->count = (uint16_t)(n < ZXV_FONT_CHARS ? n : ZXV_FONT_CHARS);
    f->w = (uint8_t)cell_w;
    f->h = (uint8_t)cell_h;
    f->flags = (uint8_t)flags;
    metrics(f);
    return 0;
}

/* El siguiente carácter de un texto en UTF-8 (lo que no es UTF-8 válido,
 * byte a byte, como Latin-1). */
static uint32_t next_char(const char **s)
{
    const uint8_t *p = (const uint8_t *)*s;
    uint32_t c = p[0];
    int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    uint32_t v = n == 3 ? c & 7 : n == 2 ? c & 15 : c & 31;
    for (int k = 1; k <= n; k++) {
        if ((p[k] & 0xC0) != 0x80) {
            *s += 1;
            return c;
        }
        v = v << 6 | (p[k] & 0x3F);
    }
    *s += 1 + n;
    return n ? v : c;
}

/* El carácter de la fuente para el código c, o -1. */
static int glyph(const zxv_font *f, uint32_t c)
{
    if (!f->codes)
        return c >= f->first && c < (uint32_t)f->first + f->count ? (int)(c - f->first) : -1;
    int lo = 0, hi = f->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->codes[mid] == c)
            return mid;
        if (f->codes[mid] < c)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

static int glyph_or_q(const zxv_font *f, uint32_t c)
{
    int i = glyph(f, c);
    return i >= 0 ? i : glyph(f, '?');
}

static int clamp_scale(int s) { return s < 1 ? 1 : s > 8 ? 8 : s; }

int zxv_font_draw(const zxv_font *f, int x, int y, const char *text, int colour, int scale)
{
    if (!f || !text)
        return x;
    int s = clamp_scale(scale), x0 = x;
    while (*text) {
        uint32_t ch = next_char(&text);
        if (ch == '\n') {
            x = x0;
            y += f->h * s;
            continue;
        }
        int i = glyph_or_q(f, ch);
        if (i < 0) {                                    /* ni '?': un hueco */
            x += (f->w / 2 + f->spacing) * s;
            continue;
        }
        int gx = x - f->left[i] * s;
        for (int r = 0; r < f->h; r++)
            for (int c = 0; c < f->w;) {
                int col = pixel(f, i, c, r);
                if (col == ZXV_TRANSPARENT) {
                    c++;
                    continue;
                }
                int e = c + 1;
                while (e < f->w && pixel(f, i, e, r) == col)
                    e++;
                int paint = colour >= 0 && colour <= 15 ? colour : col;
                zxv_gfx_rect(gx + c * s, y + r * s, (e - c) * s, s, paint);
                c = e;
            }
        x += (f->adv[i] + f->spacing) * s;
    }
    return x;
}

int zxv_font_width(const zxv_font *f, const char *text, int scale)
{
    if (!f || !text)
        return 0;
    int s = clamp_scale(scale), w = 0, best = 0;
    while (*text) {
        uint32_t ch = next_char(&text);
        if (ch == '\n') {
            w = 0;
            continue;
        }
        int i = glyph_or_q(f, ch);
        int add = i < 0 ? f->w / 2 + f->spacing : f->adv[i] + f->spacing;
        w += add * s;
        /* Sin el espaciado detrás de la última letra. */
        int line = w - f->spacing * s;
        best = line > best ? line : best;
    }
    return best;
}

int zxv_font_height(const zxv_font *f, const char *text, int scale)
{
    if (!f || !text || !*text)
        return 0;
    int lines = 1;
    for (const char *p = text; *p; p++)
        lines += *p == '\n';
    return lines * f->h * clamp_scale(scale);
}
