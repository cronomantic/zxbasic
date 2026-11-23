/*
 * zxv_inflate.h - Descomprimir datos en la memoria (sdk/lib/inflate.c).
 *
 * Para meter en el programa (o en un fichero) gráficos, mapas o textos
 * comprimidos y sacarlos cuando hagan falta. Vale lo de cualquier
 * compresor de deflate: un .gz de gzip o 7-Zip (zxv_gunzip, que comprueba
 * su CRC), o deflate sin más, como el de zlib.compressobj(9, zlib.DEFLATED,
 * -15) de Python (zxv_inflate). Comprimir se hace en el PC.
 *
 *   static uint8_t level[16384];
 *   int n = zxv_gunzip(level1_gz, level1_gz_len, level, sizeof level);
 *   if (n < 0)
 *       ...                                     // dañado, o no cabe
 *
 * Necesita unos 2 KB de pila y nada más: descomprime directamente en out.
 * Medido en el emulador, 64 KB salen en 11 ms (tiles repetidos), 23 ms
 * (texto) o 54 ms (texels al azar): de 1,2 a 6 MB por segundo.
 */
#ifndef ZXV_INFLATE_H
#define ZXV_INFLATE_H

#include <stdint.h>

#define ZXV_INFLATE_E_DATA  (-66)       /* no es deflate, o está dañado o cortado */
#define ZXV_INFLATE_E_SPACE (-67)       /* lo descomprimido no cabe en out */

/* Descomprime deflate (RFC 1951) de in (in_len bytes) en out (hasta
 * out_max). Devuelve lo que ocupa descomprimido, o un error. */
int zxv_inflate(const void *in, uint32_t in_len, void *out, uint32_t out_max);

/* Lo mismo con un .gz (RFC 1952: un solo miembro), comprobando su CRC y su
 * longitud. */
int zxv_gunzip(const void *in, uint32_t in_len, void *out, uint32_t out_max);

/* Lo que ocupará un .gz descomprimido (lo dice al final), sin
 * descomprimirlo; un error si no parece un .gz. */
int zxv_gunzip_size(const void *in, uint32_t in_len);

#endif /* ZXV_INFLATE_H */
