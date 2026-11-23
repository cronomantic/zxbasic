/*
 * zxv_m7.h - Objetos sobre el suelo del modo 7 (sdk/lib/m7.c, con
 * sdk/lib/math.c).
 *
 * zxv_m7_camera dibuja el suelo visto desde una cámara; estas funciones dan,
 * con la misma cámara y las mismas cuentas, dónde se ve en la pantalla un
 * punto del suelo (o a cierta altura sobre él) y a qué tamaño: para dibujar
 * encima los coches, los árboles o los conos con zxv_spr_draw_scaled. Y al
 * revés: qué punto del suelo hay en un píxel de la pantalla.
 *
 *   zxv_m7pos p;
 *   if (zxv_m7_project(&cam, 256, tree_x, tree_y, 0, &p)) {
 *       int h = (int)((TREE_H * (int64_t)p.scale) >> 16);    // TREE_H texels de alto
 *       zxv_spr_draw_scaled(tree, p.x - h / 4, p.y - h, h / 2, h, 0, 0);
 *   }
 *
 * Para que unos tapen a otros, dibújalos de lejos a cerca (por depth).
 */
#ifndef ZXV_M7_H
#define ZXV_M7_H

#include <stdint.h>

#include "zxv_gfx.h"
#include "zxv_math.h"

typedef struct {
    int32_t x, y;           /* el píxel de la pantalla (y: la línea del suelo donde se apoya) */
    zxv_fix scale;          /* píxeles por texel a esa distancia (16.16) */
    zxv_fix depth;          /* distancia hacia delante (16.16): para ordenar */
} zxv_m7pos;

/* El punto (px, py) del suelo (en texels, 16.16), a la altura h sobre él
 * (16.16; 0 en el suelo), visto con la cámara cam en un destino de width
 * píxeles de ancho (el de zxv_m7_camera: 256 en el modo 0). Da 1 y rellena
 * *p si está delante de la cámara (a más de un texel); si no, 0. Puede caer
 * fuera de la pantalla: el sprite se recorta solo. */
int zxv_m7_project(const zxv_m7cam *cam, int width, zxv_fix px, zxv_fix py, zxv_fix h, zxv_m7pos *p);

/* El punto del suelo que se ve en el píxel (sx, sy): exactamente el texel
 * que dibuja zxv_m7_draw con las líneas de zxv_m7_camera. Da 0 si sy está
 * en el horizonte o por encima (allí no hay suelo). */
int zxv_m7_floor_at(const zxv_m7cam *cam, int width, int sx, int sy, zxv_fix *px, zxv_fix *py);

#endif /* ZXV_M7_H */
