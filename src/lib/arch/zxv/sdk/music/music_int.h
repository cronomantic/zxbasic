/*
 * music_int.h - Lo que comparten los reproductores de cada formato y
 * music.c. Los reproductores no usan la API de la máquina: dan los
 * registros del AY de cada paso de 1/50 s, así que también se compilan en el
 * PC para las pruebas (emulator/tests/music).
 */
#ifndef ZXV_MUSIC_INT_H
#define ZXV_MUSIC_INT_H

#include <stdint.h>
#include <stddef.h>

/* Los registros de un AY tras un paso. */
typedef struct {
    uint8_t r[14];
    uint8_t r13;                /* 1: el paso escribe R13 (reinicia la envolvente) */
} mus_ay_t;

/* Un byte (o una palabra) del módulo, o 0 fuera de él: un fichero dañado no
 * lee fuera. */
static inline uint8_t mus_b(const uint8_t *m, uint32_t n, uint32_t i)
{
    return i < n ? m[i] : 0;
}

static inline uint16_t mus_w(const uint8_t *m, uint32_t n, uint32_t i)
{
    return mus_b(m, n, i) | mus_b(m, n, i + 1) << 8;
}

/* --- PT3 (Pro Tracker 3.x y Vortex Tracker II) ------------------------------- */

typedef struct {
    uint16_t addr;              /* por dónde va en el patrón */
    uint16_t smp, orn;          /* la muestra y el ornamento: su primera línea */
    uint8_t smp_loop, smp_len, orn_loop, orn_len;
    uint8_t pos_smp, pos_orn;
    uint8_t note, slide_to;
    uint8_t volume, amp;
    uint8_t skip, skip_cnt;     /* filas por nota y las que faltan */
    uint8_t enabled, env_on, gliss;
    int8_t amp_slide;
    uint8_t noise_slide;
    int16_t env_slide;          /* sin recortar a 8 bits, como en zxtune */
    uint8_t onoff, on_delay, off_delay;
    uint8_t slide_delay, slide_cnt;
    int16_t slide_step, delta, sliding, acc;
    uint16_t ton;
} pt3_chan_t;

typedef struct {
    const uint8_t *m;
    uint32_t n;
    const uint16_t *notes;
    const uint8_t *vols;
    uint8_t version, delay, delay_cnt, pos, npos, loop;
    uint8_t noise_base, add_noise;
    uint8_t env_delay, env_delay_cnt, r13, r13_write;
    uint16_t env_base;
    int16_t env_slide_add, env_slide, add_env;
    uint8_t looped;             /* veces que ha vuelto al principio */
    pt3_chan_t ch[3];
} pt3_t;

int pt3_init(pt3_t *s, const uint8_t *m, uint32_t n);
void pt3_step(pt3_t *s, mus_ay_t *ay);
/* TurboSound: los tamaños de los dos módulos, o 0 si no es uno. */
int pt3_ts_split(const uint8_t *m, uint32_t n, uint32_t *n1, uint32_t *n2);

/* --- PT2 (Pro Tracker 2) ------------------------------------------------------ */

typedef struct {
    uint16_t addr, smp, orn;
    uint8_t smp_len, smp_loop, orn_len, orn_loop, pos_smp, pos_orn;
    uint8_t enabled, env_on, note, target, volume;
    uint8_t skip, skip_cnt;
    int8_t noise_add;
    int16_t sliding, glissade;
} pt2_chan_t;

typedef struct {
    const uint8_t *m;
    uint32_t n;
    uint8_t delay, delay_cnt, pos, npos, loop, looped;
    uint8_t r[14], r13_write;
    pt2_chan_t ch[3];
} pt2_t;

int pt2_init(pt2_t *s, const uint8_t *m, uint32_t n);
void pt2_step(pt2_t *s, mus_ay_t *ay);

/* --- STC (Sound Tracker) ------------------------------------------------------ */

typedef struct {
    uint16_t addr, smp, orn;
    uint8_t note, pos, env, skip, skip_cnt;
    int16_t count;              /* pasos que quedan de la muestra, -1 en silencio */
} stc_chan_t;

typedef struct {
    const uint8_t *m;
    uint32_t n;
    uint8_t delay, delay_cnt, pos, npos, looped;
    uint8_t row, end_row, ended;
    int8_t transpose;
    uint8_t env_type, env_tone;
    uint8_t r[14];
    stc_chan_t ch[3];
} stc_t;

int stc_init(stc_t *s, const uint8_t *m, uint32_t n);
void stc_step(stc_t *s, mus_ay_t *ay);

/* --- ASC (ASC Sound Master) --------------------------------------------------- */

typedef struct {
    uint16_t addr;
    uint8_t len, loop, limit;
} asc_obj_t;

typedef struct {
    uint16_t addr;
    uint8_t skip, skip_cnt, env_parse, env, enabled, brk;
    uint8_t volume, vol_cnt, vol_delay, note, target, smp_num, orn_num, pos_smp, pos_orn;
    int8_t vol_add, vol_slide, note_add;
    uint8_t base_noise;
    int16_t noise, steps;
    int32_t tone_dev, sliding, glissade;
    asc_obj_t smp, orn;
} asc_chan_t;

typedef struct {
    const uint8_t *m;
    uint32_t n;
    uint8_t delay, delay_cnt, pos, npos, loop, looped, row, end_row, ended;
    uint16_t hdr, pats, smps, orns, env_tone;
    uint8_t r[14], r13_write;
    asc_chan_t ch[3];
} asc_t;

int asc_init(asc_t *s, const uint8_t *m, uint32_t n);
void asc_step(asc_t *s, mus_ay_t *ay);

/* --- PSG (volcados de registros) --------------------------------------------- */

typedef struct {
    const uint8_t *m;
    uint32_t n, pos, wait;      /* wait: pasos sin escrituras que quedan */
    uint8_t r[14];
    uint8_t end, looped;
} psg_t;

int psg_init(psg_t *s, const uint8_t *m, uint32_t n);
void psg_step(psg_t *s, mus_ay_t *ay);

/* --- Cualquier formato (core.c) ----------------------------------------------- */

typedef struct {
    uint8_t format;             /* ZXV_MUSIC_... */
    uint8_t chips;              /* los AY que usa: bit 0 el primero, bit 1 el segundo */
    union {
        pt3_t pt3[2];
        pt2_t pt2;
        stc_t stc;
        asc_t asc;
        psg_t psg;
    } u;
} mus_song_t;

/* El formato, o ZXV_MUSIC_NONE si no es ninguno conocido. */
int mus_song_init(mus_song_t *s, const uint8_t *m, uint32_t n);
/* Un paso: ay[0] el primer AY del módulo y, con dos, ay[1] el segundo. */
void mus_song_step(mus_song_t *s, mus_ay_t ay[2]);
/* Las veces que ha llegado al final y ha vuelto a su posición de vuelta. */
int mus_song_looped(const mus_song_t *s);

/* --- AYFX (afx.c) ---------------------------------------------------------- */

typedef struct {
    const uint8_t *p;           /* el siguiente frame, o NULL si no suena */
    const uint8_t *end;
    uint16_t tone;
    uint8_t vol, mix;           /* mix: bit 0 sin tono, bit 1 sin ruido */
    uint8_t noise;
    uint8_t has_tone, has_noise; /* el efecto ya ha puesto el tono, el ruido */
    uint32_t age;               /* pasos que lleva sonando */
} afx_t;

/* El efecto fx de un banco .afb (y en *count cuántos tiene), o NULL. */
const uint8_t *afx_find(const uint8_t *m, uint32_t n, int fx, int *count);
void afx_start(afx_t *a, const uint8_t *p, const uint8_t *end);
/* Un paso: 1 si el canal suena en él, 2 si es el último (solo cuenta su
 * volumen; p pasa a NULL), 0 si el efecto ya había terminado. */
int afx_step(afx_t *a);

/* --- Lo que suena (music.c) ------------------------------------------------- */

typedef struct {
    mus_song_t song;
    const uint8_t *song_data;   /* el módulo que suena (zxv_music_moved) */
    uint8_t playing, paused, loop, ay2, song_ays;
    uint32_t steps;
    const uint8_t *bank;
    uint32_t bank_size;
    afx_t sfx[6];
    int sfx_mask;               /* -1: el de por defecto */
    uint8_t running;            /* hay pasos en marcha */
    uint32_t next;              /* la hora del siguiente paso */
    uint8_t sent[32];           /* lo último mandado a cada registro */
    /* Los AY (bit 0 el primero, bit 1 el segundo) que se usaron en el último
     * paso, y en los que sent[] es lo que tienen: en los demás puede
     * escribir el programa, y no se tocan. */
    uint8_t owned, valid;
    /* Lo pone el programa mientras cambia el estado: zxv_music_update, si se
     * llama desde el gancho de frame justo entonces, no hace nada esa vez. */
    volatile uint8_t busy;
} mus_state_t;

/* El módulo de s, que se ha movido d bytes (core.c). */
void mus_song_move(mus_song_t *s, intptr_t d);

#endif
