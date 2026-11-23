/*
 * zxv_font.h - Texto con fuentes propias, de cualquier tamaño y color
 * (sdk/lib/font.c; la de la ROM, con sdk/lib/font_rom.c).
 *
 * Una fuente sale de:
 * - una hoja: un bitmap de 4 bpp (un sprite de un .gfx, por ejemplo) con un
 *   carácter en cada celda, fila a fila desde 'first', de cualquier tamaño
 *   y en color (el 8 es transparente). Así se hacen las de los marcadores y
 *   los títulos, con zxv-gfx o png2gfx;
 * - una tabla de 1 bpp de hasta 8 de ancho, como la de sdk/tools/fontconv.py;
 * - la 8 × 8 de la ROM, ya hecha: ASCII, las letras con tilde, ñ, ü, ¿ y ¡.
 *
 * El texto va en UTF-8 y puede tener '\n'. Se dibuja con zxv_gfx_rect (así
 * que respeta el recorte y la trama de zxv_gfx_pattern) en un color o con
 * los de la hoja, ampliado (de 1 a 8), y proporcional o de ancho fijo.
 *
 *   static zxv_font f;
 *   zxv_font_rom(&f, 0);                                  // la de la ROM, proporcional
 *   int w = zxv_font_width(&f, "¡Fin!", 3);
 *   zxv_font_draw(&f, (256 - w) / 2, 80, "¡Fin!", 14, 3); // centrado, el triple de grande
 */
#ifndef ZXV_FONT_H
#define ZXV_FONT_H

#include <stdint.h>

#include "zxv_gfx.h"

#define ZXV_FONT_CHARS      224         /* caracteres por fuente, como mucho */
#define ZXV_FONT_FIXED      0x01u       /* todos avanzan el ancho de la celda */
#define ZXV_FONT_OWN        (-1)        /* zxv_font_draw: los colores de la hoja */

typedef struct {
    zxv_bitmap sheet;                   /* la hoja (4 bpp); pix NULL en las de 1 bpp */
    const uint8_t *bits;                /* 1 bpp: h bytes por carácter, el bit 7 a la izquierda */
    const uint16_t *codes;              /* el código de cada carácter, de menor a mayor;
                                         * NULL: first, first + 1... */
    uint16_t first, count;
    uint8_t w, h;                       /* la celda */
    uint8_t spacing;                    /* píxeles entre letras (cámbialo si quieres) */
    uint8_t flags;
    uint8_t left[ZXV_FONT_CHARS];       /* columnas vacías a la izquierda de cada uno */
    uint8_t adv[ZXV_FONT_CHARS];        /* lo que avanza cada uno (sin el espaciado) */
} zxv_font;

/* La 8 × 8 de la ROM: flags 0, proporcional (1 píxel entre letras);
 * ZXV_FONT_FIXED, de 8 en 8 como la de la máquina. */
void zxv_font_rom(zxv_font *f, unsigned flags);

/* De una hoja con celdas de cell_w × cell_h (los caracteres de first en
 * adelante, de izquierda a derecha y de arriba abajo; hasta 224). La hoja
 * no se copia: tiene que seguir existiendo. 0 bien, -1 si no vale. */
int zxv_font_sheet(zxv_font *f, const zxv_bitmap *sheet, int cell_w, int cell_h, int first, unsigned flags);

/* De una tabla de 1 bpp: n caracteres de h filas, con sus códigos (de menor
 * a mayor; NULL: de 32 en adelante). 0 bien, -1 si no vale. */
int zxv_font_bits(zxv_font *f, const uint8_t *bits, const uint16_t *codes, int n, int h, unsigned flags);

/* Dibuja el texto con la esquina de arriba a la izquierda en (x, y), del
 * color colour (0-15; ZXV_FONT_OWN, los de la hoja, y el 15 en las de
 * 1 bpp) y scale veces más grande. Un '\n' vuelve a x una línea más abajo
 * (h·scale). Lo que no está en la fuente sale como '?'. Devuelve la x en
 * la que acaba la última línea. */
int zxv_font_draw(const zxv_font *f, int x, int y, const char *text, int colour, int scale);

/* Lo que mide el texto: el ancho de su línea más larga y el alto de todas. */
int zxv_font_width(const zxv_font *f, const char *text, int scale);
int zxv_font_height(const zxv_font *f, const char *text, int scale);

#endif /* ZXV_FONT_H */
