/*
 * zxv_mapmove.h - Mover cosas por un mapa de tiles sin atravesar las
 * paredes (sdk/lib/mapmove.c).
 *
 * El juego dice de qué tipo es cada tile (vacío, sólido, plataforma o uno
 * suyo: escalera, pinchos, agua...) y zxv_map_move mueve un rectángulo
 * parando contra lo sólido, también a mucha velocidad (no salta las
 * paredes), y dice contra qué ha chocado. Las posiciones van en coma fija
 * 16.16 (zxv_math.h), en píxeles del mapa.
 *
 *   static const uint8_t kinds[] = { ZXV_TILE_EMPTY, ZXV_TILE_SOLID, ZXV_TILE_PLATFORM, LADDER };
 *   zxv_mapsolid ms = { &level, kinds, sizeof kinds, ZXV_TILE_SOLID };
 *   unsigned hit = zxv_map_move(&ms, &px, &py, 12, 16, vx, vy, 0);
 *   if (hit & ZXV_HIT_DOWN)
 *       vy = 0;                                         // en el suelo
 *   if (zxv_map_find(&ms, px, py, 12, 16, LADDER, 0, 0))
 *       ...                                             // en una escalera
 */
#ifndef ZXV_MAPMOVE_H
#define ZXV_MAPMOVE_H

#include <stdint.h>

#include "zxv_gfx.h"
#include "zxv_math.h"

/* Tipos de tile. Del 3 al 255 son del juego: no paran nada, pero
 * zxv_map_find los encuentra. */
#define ZXV_TILE_EMPTY      0
#define ZXV_TILE_SOLID      1           /* para por los cuatro lados */
#define ZXV_TILE_PLATFORM   2           /* solo para al bajar desde encima: se atraviesa
                                         * subiendo y de lado */

typedef struct {
    const zxv_tilemap *map;
    const uint8_t *kinds;   /* el tipo de cada tile, por su número; NULL: el tile 0
                             * vacío y los demás sólidos */
    uint16_t nkinds;        /* cuántos hay en kinds (los tiles de después: vacíos) */
    uint8_t outside;        /* el tipo de lo de fuera de un mapa que no se repite */
} zxv_mapsolid;

/* Lo que devuelve zxv_map_move: los lados que han chocado. */
#define ZXV_HIT_LEFT        0x01u
#define ZXV_HIT_RIGHT       0x02u
#define ZXV_HIT_UP          0x04u       /* con la cabeza */
#define ZXV_HIT_DOWN        0x08u       /* con los pies: está en el suelo */

/* Opciones de zxv_map_move. */
#define ZXV_MOVE_THROUGH    0x01u       /* atravesar las plataformas (bajar de una) */

/* Mueve el rectángulo de w × h píxeles con la esquina de arriba a la
 * izquierda en (*x, *y) primero dx en horizontal y luego dy en vertical
 * (todo en 16.16), parando cada eje en la primera celda que lo bloquea:
 * queda tocándola, sin solaparse. Las plataformas solo paran al bajar si
 * los pies estaban por encima de ellas. Devuelve los ZXV_HIT_ de los lados
 * que han chocado. Si el rectángulo ya estaba metido en lo sólido, sale
 * por donde se mueve. */
unsigned zxv_map_move(const zxv_mapsolid *ms, zxv_fix *x, zxv_fix *y, int w, int h, zxv_fix dx, zxv_fix dy,
                      unsigned flags);

/* 1 si el rectángulo está apoyado: justo debajo de los pies hay algo que
 * para al bajar (sólido, o una plataforma sin ZXV_MOVE_THROUGH). */
int zxv_map_on_ground(const zxv_mapsolid *ms, zxv_fix x, zxv_fix y, int w, int h, unsigned flags);

/* 1 si el rectángulo toca alguna celda del tipo kind; en *cx, *cy (si no
 * son NULL) la primera, de arriba abajo y de izquierda a derecha, en
 * celdas (dentro del mapa también si se repite): para quitar una moneda
 * cogida, por ejemplo. */
int zxv_map_find(const zxv_mapsolid *ms, zxv_fix x, zxv_fix y, int w, int h, int kind, int *cx, int *cy);

/* El tipo de la celda en el píxel (px, py) del mapa (enteros). */
int zxv_map_kind(const zxv_mapsolid *ms, int px, int py);

#endif /* ZXV_MAPMOVE_H */
