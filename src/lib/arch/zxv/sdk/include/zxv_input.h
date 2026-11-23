/*
 * zxv_input.h - Teclado y mandos a la vez, para juegos (sdk/lib/input.c).
 *
 * El juego piensa en acciones (arriba, fuego, pausa...), hasta 16, un bit
 * cada una. Un mapa dice qué teclas (hasta 3) y qué botones del mando hacen
 * cada acción; zxv_input_update, una vez por frame, mira el teclado y el
 * mando y dice qué acciones están pulsadas, cuáles se acaban de pulsar o
 * de soltar y cuánto llevan. Con dos mapas, dos jugadores.
 *
 *   zxv_input in;
 *   zxv_input_init(&in, 0);                     // el mapa de siempre: cursores, QAOP, mando 0
 *   for (;;) {
 *       zxv_input_update(&in);
 *       if (in.held & ZXV_IN_LEFT) x--;
 *       if (in.pressed & ZXV_IN_FIRE) shoot();
 *       ...
 *   }
 *
 * Para redefinir las teclas: zxv_input_new_key() da la que se acaba de
 * pulsar y zxv_input_key_name() su nombre; el mapa es de datos, así que se
 * puede copiar, cambiar y guardar (zxv_save.h).
 */
#ifndef ZXV_INPUT_H
#define ZXV_INPUT_H

#include <stdint.h>

#define ZXV_IN_ACTIONS      16
#define ZXV_IN_KEYS         3

/* Las acciones del mapa de siempre: los mismos bits que los botones del
 * mando (ZXV_JOY_...), más Esc. Un juego puede usar los suyos. */
#define ZXV_IN_RIGHT        0x0001u
#define ZXV_IN_LEFT         0x0002u
#define ZXV_IN_DOWN         0x0004u
#define ZXV_IN_UP           0x0008u
#define ZXV_IN_FIRE         0x0010u
#define ZXV_IN_FIRE2        0x0020u
#define ZXV_IN_FIRE3        0x0040u
#define ZXV_IN_START        0x0080u
#define ZXV_IN_SELECT       0x0100u
#define ZXV_IN_ESC          0x0200u

typedef struct {
    uint8_t keys[ZXV_IN_ACTIONS][ZXV_IN_KEYS];  /* teclas (códigos HID) de cada acción; 0: ninguna */
    uint16_t joy[ZXV_IN_ACTIONS];               /* botones del mando que la hacen (ZXV_JOY_...) */
    int8_t pad;                                 /* el mando: 0, 1, o -1 (ninguno) */
    uint8_t flags;                              /* ZXV_IN_KEEP_QUEUE */
} zxv_input_map;

/* zxv_input_update vacía la cola de teclas (zxv_key_get) para que lo
 * tecleado durante la partida no llegue al BASIC al volver; con esto, no. */
#define ZXV_IN_KEEP_QUEUE   0x01u

/* El mapa de siempre: cursores o Q A O P, fuego con Espacio o M (y los
 * botones 1 del mando), fuego 2 con N o Z (botón 2), fuego 3 con B o X
 * (botón 3), Intro para START, Tab para SELECT, Esc; el mando 0. */
extern const zxv_input_map zxv_input_default;

typedef struct {
    const zxv_input_map *map;
    uint16_t held;                              /* pulsadas ahora */
    uint16_t pressed;                           /* las que se han pulsado en este frame */
    uint16_t released;                          /* las que se han soltado en este frame */
    uint16_t frames[ZXV_IN_ACTIONS];            /* cuántos frames lleva pulsada cada una */
} zxv_input;

/* Empieza sin nada pulsado; map NULL: zxv_input_default. El mapa no se
 * copia: tiene que seguir existiendo. */
void zxv_input_init(zxv_input *in, const zxv_input_map *map);

/* Una vez por frame: lee el teclado y el mando. */
void zxv_input_update(zxv_input *in);

/* 1 si alguna de 'actions' se acaba de pulsar, o lleva pulsada más de
 * 'delay' frames y toca repetir (cada 'rate'): para moverse por un menú. */
int zxv_input_repeat(const zxv_input *in, uint16_t actions, int delay, int rate);

/* 1 si la tecla hid (código USB HID) está pulsada ahora. */
int zxv_input_key_down(int hid);

/* La tecla que se ha pulsado desde la llamada anterior (la de código más
 * bajo si son varias), o -1. La primera llamada solo toma nota de lo que
 * ya estaba pulsado y da -1. */
int zxv_input_new_key(void);

/* El nombre de una tecla para enseñarlo («Q», «Espacio», «Cursor
 * arriba»...), en el idioma de la máquina. Las teclas de símbolos cambian
 * de sitio según el teclado: salen como «Tecla 33» (su código). */
const char *zxv_input_key_name(int hid);

#endif /* ZXV_INPUT_H */
