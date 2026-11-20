/*
 * zxv_music.h - Música y efectos para los dos AY: reproduce los módulos de
 * los trackers del Spectrum y los efectos AYFX (sdk/music).
 *
 * Suenan a su tempo, 50 pasos por segundo como en el Spectrum, aunque la
 * máquina vaya a 60 imágenes por segundo: cada paso va al AY a su hora con
 * la cola con marca de tiempo (spec §5.4), así que la música no se acelera
 * ni da tirones.
 *
 *   zxv_music_play(modulo, tamaño, ZXV_MUSIC_LOOP);
 *   zxv_sfx_bank(banco, tamaño);
 *   for (;;) {
 *       ...
 *       if (disparo)
 *           zxv_sfx_play(0, ZXV_SFX_AUTO);
 *       zxv_music_update();
 *       zxv_wait_vblank();
 *   }
 *
 * zxv_music_update() prepara los pasos de los próximos ZXV_MUSIC_AHEAD_US;
 * hay que llamarla al menos con esa frecuencia: en cada imagen, o desde el
 * gancho de frame (y entonces solo desde él), que la hace sonar también
 * mientras el programa está ocupado. Los datos (el módulo, el banco) deben
 * seguir en la memoria mientras suenan: la biblioteca no los copia.
 *
 * La biblioteca usa la cola con marca de tiempo, donde las horas tienen que
 * ir en orden: un programa que también la use debe mandar las suyas por
 * delante de ZXV_MUSIC_AHEAD_US. Por ejemplo, el beeper mientras suena la
 * música: zxv_beeper_event_at(zxv_time_us() + ZXV_MUSIC_AHEAD_US, nivel), y
 * no zxv_beeper_event, que lo pone en la hora actual.
 */
#ifndef ZXV_MUSIC_H
#define ZXV_MUSIC_H

#include <stdint.h>

/* Los formatos. */
#define ZXV_MUSIC_NONE      0
#define ZXV_MUSIC_PT3       1   /* Pro Tracker 3.x y Vortex Tracker II */
#define ZXV_MUSIC_TS        2   /* TurboSound: dos PT3, uno por AY */
#define ZXV_MUSIC_PSG       3   /* volcado de los registros del AY, paso a paso */
#define ZXV_MUSIC_PT2       4   /* Pro Tracker 2 */
#define ZXV_MUSIC_STC       5   /* Sound Tracker 1.x, compilado */
#define ZXV_MUSIC_ASC       6   /* ASC Sound Master 0.x y 1.x */
#define ZXV_MUSIC_ZXM       7   /* el propio de ZX-V (.zxm): los dos AY y los canales de muestras */
#define ZXV_MUSIC_VGM       8   /* VGM (o .vgz): los AY (uno o dos) */

/* Opciones de zxv_music_play. */
#define ZXV_MUSIC_LOOP      1   /* al acabar, sigue por su posición de vuelta */
#define ZXV_MUSIC_AY2       2   /* un módulo de un AY suena en el segundo */

#define ZXV_MUSIC_AHEAD_US  35000u

typedef struct {
    int format;                 /* ZXV_MUSIC_... */
    int chips;                  /* los AY que usa: 1 o 2 */
    char title[33];
    char author[33];
} zxv_music_info_t;

/* El formato de un módulo, su título y su autor, sin tocarlo. Da el formato
 * (ZXV_MUSIC_NONE si no es ninguno conocido). */
int zxv_music_info(const void *data, uint32_t size, zxv_music_info_t *info);

/* Empieza a tocar un módulo; da su formato o, si no lo reconoce,
 * ZXV_MUSIC_NONE y no toca nada. */
int zxv_music_play(const void *data, uint32_t size, int flags);
void zxv_music_stop(void);
/* Dónde pone zxv_music_play las muestras de un .zxm: en la memoria de sonido
 * desde base, y en la tabla de muestras desde la entrada slot (por defecto,
 * 0 y 0). Las del programa, detrás. */
void zxv_music_samples(uint32_t base, int slot);
/* Para un .vgz, 32 KB de trabajo (la ventana con lo que se descomprime,
 * mientras suena): sin ellos, un .vgz no suena. */
void zxv_music_work(void *buf);
/* Una muestra como efecto, a la vez que la canción (D99): en el canal ch
 * (0-11), la entrada slot de la tabla, con su paso (zxv_smp_step), volumen
 * (0-64) y panorama (0-255), durante dur_us (0: hasta zxv_music_smp_fx_stop;
 * una muestra con bucle). Con la canción en marcha va a su paso, con su
 * mismo adelanto; si el canal es de la canción, la tapa mientras suena y
 * luego la canción sigue en él en su siguiente nota. */
void zxv_music_smp_fx(int ch, int slot, uint32_t step, int vol, int pan, uint32_t dur_us);
void zxv_music_smp_fx_stop(int ch);
/* Los canales de muestras de la canción que suena (los primeros): 0 si no
 * suena un .zxm. Los efectos, en los de arriba. */
int zxv_music_smp_tracks(void);
void zxv_music_pause(int on);
/* 1 mientras suena (sin ZXV_MUSIC_LOOP, hasta el final). */
int zxv_music_playing(void);
/* Los pasos de 1/50 s que lleva sonando. */
uint32_t zxv_music_steps(void);

/* El módulo y el banco que suenan se han movido, enteros, a otra dirección
 * (song o bank, NULL si ese no): la biblioteca sigue por donde iba. Para un
 * programa que compacta su memoria mientras suenan (como el BASIC). */
void zxv_music_moved(const void *song, const void *bank);

/* Lleva la música y los efectos hasta ZXV_MUSIC_AHEAD_US por delante. */
void zxv_music_update(void);

/* Lo mismo, pero solo los pasos de antes de t_us (en el reloj de
 * zxv_time_us; nunca más allá de ZXV_MUSIC_AHEAD_US por delante). Para
 * mezclar la música con otros zxv_ay_event, que van en orden de hora: antes
 * de cada uno, zxv_music_update_until(su hora). */
void zxv_music_update_until(uint32_t t_us);

/* --- Efectos AYFX --------------------------------------------------------- */

/* Los canales: 0-2 los A, B y C del primer AY y 3-5 los del segundo. */
#define ZXV_SFX_AUTO        (-1)

/* El banco de efectos (.afb) de zxv_sfx_play; da cuántos efectos tiene. */
int zxv_sfx_bank(const void *data, uint32_t size);
/* El efecto n del banco en un canal (o ZXV_SFX_AUTO: uno libre de los de
 * zxv_sfx_channels o, si no hay, el que lleva más tiempo sonando). Da el
 * canal, o -1 si no existe el efecto. */
int zxv_sfx_play(int n, int channel);
/* Lo mismo, con un efecto suelto (.afx). */
int zxv_sfx_play_data(const void *data, uint32_t size, int channel);
/* Para el efecto de un canal (o todos, con ZXV_SFX_AUTO). */
void zxv_sfx_stop(int channel);
/* Los canales para ZXV_SFX_AUTO (bit n, el canal n). Por defecto, los del
 * AY que no usa la música (o todos, si usa los dos). Un efecto en un canal
 * de la música lo tapa mientras suena. */
void zxv_sfx_channels(int mask);
/* 1 si suena algún efecto en el canal (o en alguno, con ZXV_SFX_AUTO). */
int zxv_sfx_playing(int channel);

#endif
