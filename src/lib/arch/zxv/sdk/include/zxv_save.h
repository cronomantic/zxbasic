/*
 * zxv_save.h - Guardar los datos de un juego: récords, opciones, partidas
 * (sdk/lib/save.c).
 *
 * zxv_save escribe una estructura del juego con una cabecera (la versión
 * de sus datos, su longitud y un CRC) en un fichero temporal y luego lo
 * cambia por el de verdad: si la máquina se apaga a medias, se queda el de
 * antes. zxv_load solo la da por buena si el CRC, la versión y la longitud
 * cuadran; si no, devuelve un error y el juego sigue con sus valores de
 * siempre. Sin tarjeta (ZXV_E_NOCARD), desde ROM: o sin sitio, el juego
 * sigue igual: solo que no se guarda.
 *
 *   static struct { uint32_t best; uint8_t music; zxv_score top[5]; } data;
 *   if (zxv_load("juego.sav", 1, &data, sizeof data) < 0)
 *       defaults(&data);                                // la primera vez
 *   ...
 *   if (zxv_score_add(data.top, 5, score, "SCH") >= 0)
 *       zxv_save("juego.sav", 1, &data, sizeof data);
 *
 * Cambia la versión cuando cambie la estructura: los ficheros de antes
 * dejan de valer (ZXV_SAVE_E_VERSION) en vez de leerse mal.
 */
#ifndef ZXV_SAVE_H
#define ZXV_SAVE_H

#include <stdint.h>

/* Errores propios (los demás son los ZXV_E_ de los ficheros, zxv.h). */
#define ZXV_SAVE_E_DAMAGED  (-64)       /* no es un fichero de zxv_save, o está dañado */
#define ZXV_SAVE_E_VERSION  (-65)       /* de otra versión o de otra longitud */

/* Guarda len bytes de data en name (una ruta; el nombre del fichero, hasta
 * 62 caracteres, porque mientras se escribe se usa name~). ZXV_OK (0) o un
 * error negativo. */
int zxv_save(const char *name, uint16_t version, const void *data, uint32_t len);

/* Lee en data lo que guardó zxv_save con la misma versión y len. ZXV_OK, o
 * un error negativo: ZXV_E_NOENT (no hay), ZXV_E_NOCARD, ZXV_SAVE_E_DAMAGED,
 * ZXV_SAVE_E_VERSION... Si falla, data no cambia. */
int zxv_load(const char *name, uint16_t version, void *data, uint32_t len);

/* El CRC-32 (el de zip y PNG) de n bytes: el que usan estos ficheros. */
uint32_t zxv_crc32(const void *data, uint32_t n);

/* Una tabla de récords, de mayor a menor. */
typedef struct {
    uint32_t score;
    char name[8];                       /* hasta 7 caracteres y el 0 */
} zxv_score;

/* Mete la puntuación en su sitio de la tabla de n (de mayor a menor; con
 * empates, detrás de los que ya estaban) y quita la última. Devuelve su
 * posición (0 es la mejor), o -1 si no entra. */
int zxv_score_add(zxv_score *table, int n, uint32_t score, const char *name);

#endif /* ZXV_SAVE_H */
