/*
 * zxv_bank.h - Los bancos de un programa .ZXV (D109): datos y código que van
 * al final de su fichero y se cargan cuando hacen falta.
 *
 * Se ponen al convertir el ELF:
 *
 *   elf2zxv.py juego.elf juego.zxv --bank etapa1=etapa1.gfx --bank etapa2=etapa2.gfx --pack
 *
 * y se cargan con:
 *
 *   int n = zxv_bank_load("etapa2", grafs, sizeof grafs);
 *
 * Se leen del propio fichero del programa, que la ROM deja abierto mientras
 * va (ZXV_SV_PROG_FILE): da igual la carpeta actual o la unidad (SD:, EXT:
 * o ROM:). Lo que puede salir mal, y lo que devuelven entonces:
 *
 *   ZXV_E_NOENT    no hay un banco con ese nombre (o el programa no tiene)
 *   ZXV_E_ACCESS   la ROM es anterior a los bancos (spec 0.2)
 *   ZXV_E_RANGE    no cabe en max
 *   ZXV_E_CHANGED  el fichero ya no es el del programa en marcha (otra
 *                  versión en la tarjeta o recompilado en el PC): hay que
 *                  volver a cargar el programa
 *   ZXV_E_FORMAT   el banco está dañado (su CRC no coincide)
 *   ZXV_E_NOCARD, ZXV_E_OFFLINE, ZXV_E_IO  los de leer el fichero: sin
 *                  tarjeta, sin servidor de EXT:, un error de lectura
 *
 * Con un error, lo que hubiera en el destino no vale. MIT, como el SDK.
 */
#ifndef ZXV_BANK_H
#define ZXV_BANK_H

#include <stdint.h>

/* Carga el banco de datos name en dst (hasta max bytes), descomprimido si
 * hace falta y comprobado: lo que ocupa, o un error ZXV_E_*. */
int zxv_bank_load(const char *name, void *dst, uint32_t max);

/* Lo que ocupa el banco name (descomprimido), para reservarle sitio; o un
 * error ZXV_E_*. */
int zxv_bank_size(const char *name);

/* Los bancos de código (tools/zxvbanks.py --code): sus funciones se llaman
 * como siempre, y el banco se carga solo la primera vez. Esto lo carga ya,
 * para elegir cuándo se nota (el cartel de una fase): 0, o un error
 * ZXV_E_* (ZXV_E_ACCESS si en su zona hay otro banco en uso). */
int zxv_bank_preload(const char *name);

#endif /* ZXV_BANK_H */
