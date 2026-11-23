/*
 * zxv_fade.h - Fundidos de paleta (sdk/lib/fade.c).
 *
 * Lleva los 16 colores de las paletas (ZXV_PALETTE(t), spec §4.6) de lo que
 * tienen ahora a otros en n frames: a negro al acabar una fase, de negro a
 * la paleta al empezar la siguiente, un destello blanco al chocar... Los
 * píxeles de la pantalla no se tocan: solo cambia cómo se ven.
 *
 * El sistema tiene un solo gancho de frame (y la música suele usarlo), así
 * que la biblioteca no lo toma: llama a zxv_fade_step() una vez por frame,
 * en tu bucle o en tu gancho. Para un fundido entre escenas que espera a
 * acabar, zxv_fade_wait().
 *
 *   zxv_fade_to_colour(0, 30, ZXV_FADE_ALL);     // a negro en medio segundo
 *   zxv_fade_wait();
 *   load_next_level();
 *   zxv_fade_to(level_palette, 30, ZXV_FADE_ALL);  // y de negro a sus colores
 *   while (zxv_fade_busy()) { zxv_fade_step(); game_frame(); }
 */
#ifndef ZXV_FADE_H
#define ZXV_FADE_H

#include <stdint.h>

/* Qué tablas de paleta (0 a 3) cambia: bit t, la tabla t. */
#define ZXV_FADE_ALL        0x0Fu

/* Empieza a llevar las tablas de 'tables' a los colores 'to' (16 RGB565;
 * NULL: los de siempre) en 'frames' frames (0: ya). Si había otro fundido,
 * empieza desde donde iba. */
void zxv_fade_to(const uint16_t to[16], int frames, unsigned tables);

/* Lo mismo con los 16 colores iguales: ZXV_RGB(0, 0, 0) es a negro,
 * ZXV_RGB(255, 255, 255) a blanco. */
void zxv_fade_to_colour(uint16_t rgb, int frames, unsigned tables);

/* Un frame más del fundido (nada si no hay ninguno). */
void zxv_fade_step(void);

/* 1 mientras queda fundido. */
int zxv_fade_busy(void);

/* Avanza el fundido un frame en cada VBLANK hasta acabarlo (con
 * zxv_wait_vblank: no lo llames desde un gancho de frame). */
void zxv_fade_wait(void);

/* Un color a medio camino: a + (b - a)·k / n en cada componente (k de 0 a
 * n), al más cercano. Sirve también para tus propias mezclas. */
uint16_t zxv_rgb_mix(uint16_t a, uint16_t b, int k, int n);

#endif /* ZXV_FADE_H */
