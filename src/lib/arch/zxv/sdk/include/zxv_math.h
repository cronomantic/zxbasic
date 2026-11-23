/*
 * zxv_math.h - Cuentas para juegos sin coma flotante (sdk/lib/math.c).
 *
 * Coma fija 16.16 (zxv_fix: 65536 = 1), ángulos en vueltas de 0x10000 (como
 * el motor gráfico: 0x4000 es un cuarto de vuelta, hacia y creciente), seno
 * y coseno con la misma tabla que el modo 7 de la ROM, atan2, raíces
 * cuadradas enteras y números al azar. Todo con enteros: da lo mismo en el
 * emulador y en la máquina.
 *
 *   zxv_fix vx = zxv_fmul(speed, zxv_cos(angle));     // velocidad en x
 *   int32_t to_player = zxv_atan2(py - ey, px - ex);   // hacia el jugador
 *   int d = (int)zxv_hypot(px - ex, py - ey);          // a qué distancia
 *   int roll = zxv_rand_range(1, 6);                   // un dado
 */
#ifndef ZXV_MATH_H
#define ZXV_MATH_H

#include <stdint.h>

typedef int32_t zxv_fix;                /* coma fija 16.16 */

#define ZXV_FIX_ONE         0x10000
/* Una constante: ZXV_FIX(1.5), ZXV_FIX(-3). Con un número de coma
 * flotante, el compilador hace la cuenta; no la uses con variables. */
#define ZXV_FIX(n)          ((zxv_fix)((n) * 65536))
#define ZXV_FIX_INT(f)      ((int32_t)(f) >> 16)                    /* hacia abajo */
#define ZXV_FIX_ROUND(f)    ((int32_t)((f) + 0x8000) >> 16)         /* al más cercano */

/* Ángulos: una vuelta es 0x10000. */
#define ZXV_TURN            0x10000
#define ZXV_DEG(d)          ((int32_t)((d) * 65536 / 360))          /* ZXV_DEG(90) = 0x4000 */

/* a · b y a / b en 16.16 (con 64 bits en medio: no se desborda antes de
 * tiempo). a / 0 no está definido. */
static inline zxv_fix zxv_fmul(zxv_fix a, zxv_fix b) { return (zxv_fix)(((int64_t)a * b) >> 16); }
static inline zxv_fix zxv_fdiv(zxv_fix a, zxv_fix b) { return (zxv_fix)(((int64_t)a * 65536) / b); }

/* Seno y coseno en 16.16 (de -65536 a 65536): una tabla de un cuarto de
 * vuelta con 256 pasos e interpolación. Exactamente los del motor gráfico
 * (m7_camera, m7_affine). */
zxv_fix zxv_sin(int32_t angle);
zxv_fix zxv_cos(int32_t angle);

/* El ángulo (de 0 a 0xFFFF) del vector (x, y): 0 hacia x creciente, 0x4000
 * hacia y creciente (abajo en la pantalla). Con un error de unas pocas
 * unidades de 1/65536 de vuelta; atan2(0, 0) es 0. */
int32_t zxv_atan2(int32_t y, int32_t x);

/* Raíces cuadradas enteras, hacia abajo: isqrt(10) = 3. */
uint32_t zxv_isqrt(uint32_t v);
uint32_t zxv_isqrt64(uint64_t v);
/* La raíz de un 16.16 (v >= 0), en 16.16. */
zxv_fix zxv_fsqrt(zxv_fix v);
/* La longitud de (x, y), hacia abajo, sin desbordar con valores grandes. */
uint32_t zxv_hypot(int32_t x, int32_t y);

/* Números al azar (xorshift32): la misma secuencia para la misma semilla.
 * zxv_srand(0) vale como zxv_srand(1). Para varios generadores
 * independientes, zxv_rand_next con un estado propio (que no sea 0). */
void zxv_srand(uint32_t seed);
uint32_t zxv_rand(void);                        /* 32 bits */
int zxv_rand_range(int lo, int hi);             /* de lo a hi, los dos incluidos */
uint32_t zxv_rand_next(uint32_t *state);

#endif /* ZXV_MATH_H */
