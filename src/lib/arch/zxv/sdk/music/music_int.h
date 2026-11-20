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

/* Las órdenes del PT3 y lo que hace cada paso, por separado: las usan el
 * PT3 (al leer sus patrones) y las pistas del AY de un .zxm (zxm.c), cuyos
 * instrumentos y ornamentos son los del PT3 byte a byte. p: dónde empieza
 * la muestra o el ornamento en s->m (su vuelta y su largo), en los primeros
 * 64 KB (en un .zxm, s->m empieza en sus instrumentos del AY). */
void pt3_tables(pt3_t *s, int version, int table);     /* las notas y los volúmenes */
void pt3_sample_at(pt3_t *s, pt3_chan_t *c, uint32_t p);
void pt3_ornament_at(pt3_t *s, pt3_chan_t *c, uint32_t p);
void pt3_note(pt3_chan_t *c, int note);                 /* una nota: lo de la nota anterior, a 0 */
void pt3_rest(pt3_chan_t *c);                           /* la apaga */
void pt3_env(pt3_t *s, pt3_chan_t *c, int shape, uint16_t base);
void pt3_env_off(pt3_chan_t *c);
void pt3_fx_gliss(pt3_t *s, pt3_chan_t *c, uint8_t delay, int16_t step);
void pt3_fx_porta(pt3_t *s, pt3_chan_t *c, uint8_t delay, int16_t step, int prev_note, int16_t prev_sliding);
void pt3_fx_onoff(pt3_chan_t *c, uint8_t on, uint8_t off);
void pt3_fx_env_slide(pt3_t *s, uint8_t delay, int16_t add);
/* Un paso de los tres canales, tras las filas: los registros del AY. */
void pt3_tick(pt3_t *s, mus_ay_t *ay);
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

/* --- Lo de music.c mientras suena una canción con su propio reloj -------------- */

/* El .zxm y el VGM: aquí, en el estado de la canción, y no en el fijo de
 * music.c, que en la ROM le quita memoria al BASIC. El reloj de la
 * canción, sus AY en el último paso, lo que dio cada efecto en el suyo, lo
 * último mandado a cada canal de muestras, en cuáles vale y cuáles suenan;
 * y los canales que tapa un efecto de muestras (zxv_music_smp_fx), y hasta
 * cuándo (los de fx_loop, hasta que se paren). */
typedef struct {
    uint32_t song_next;
    mus_ay_t last_ay[2];
    uint8_t sfx_how[6];
    uint8_t smp_sent[12][8];
    uint16_t smp_valid, smp_live;
    uint16_t fx_mask, fx_loop;
    uint32_t fx_end[12];
} mus_rt_t;

/* --- .zxm, el formato propio (zxm.c, zxm_format.h) ----------------------------- */

/* Lo que pide la canción a un canal de muestras en un paso: music.c escribe
 * lo que cambia, y arranca a la vez los que tienen key (zxv_smp_start). */
typedef struct {
    uint8_t on;                 /* la canción usa el canal */
    uint8_t key;                /* la nota empieza en este paso (desde offset) */
    uint8_t stop;               /* se calla */
    uint8_t slot, vol, pan, offset, ctrl;   /* vol 0-64; ctrl: ZXV_SMP_NOINTERP */
    uint32_t step;              /* 8.16 */
} mus_smp_t;

/* Dónde va una pista en su patrón. */
typedef struct {
    uint32_t addr;              /* el byte siguiente, o 0 sin patrón */
    uint16_t left;              /* filas que quedan en el patrón */
    uint8_t wait;               /* filas vacías que quedan antes del byte siguiente */
} zxm_reader_t;

/* Una casilla leída. */
typedef struct {
    uint8_t has;                /* ZXM_C_* */
    uint8_t note, ins, vol, orn, env;
    uint16_t env_period;
    uint8_t fx[2], fxp[2][3];
} zxm_cell_t;

/* Dónde va una envolvente de un instrumento de muestras: su tick. */
typedef struct {
    uint16_t pos;
} zxm_env_t;

/* Una pista de muestras. */
typedef struct {
    uint8_t note, ins, smp;     /* la nota (0-95), el instrumento (1-, 0 ninguno) y la muestra (0-) */
    int8_t rel, fine;           /* los de la muestra: nota relativa y afinación (1/128 de semitono) */
    uint8_t vol, pan;           /* los de la pista: 0-64 y 0-255 */
    uint8_t playing, keyon;     /* suena; la nota sigue sin soltar */
    int32_t pitch, target;      /* periodo x 4 (Amiga) o nota x 64 (lineal); y el del portamento */
    int16_t vib;                /* lo que pone el vibrato en este tick */
    int8_t vol_add;             /* lo que pone el trémolo */
    uint8_t arp;                /* semitonos del arpegio en este tick */
    uint8_t fx[2], fxp[2];      /* los efectos de la fila */
    uint8_t m_slide, m_porta, m_vib, m_trem, m_vol, m_fine, m_xfine, m_retrig, m_tremor, m_pan, m_offset, m_gvol;
    uint8_t vib_pos, trem_pos, vib_wave, trem_wave;
    uint8_t retrig_cnt, tremor_cnt;
    uint8_t delay_tick, cut_tick, off_tick;     /* EDx, ECx y Kxx (0xFF: no) */
    uint8_t loop_row, loop_cnt;                 /* E6x */
    zxm_cell_t pend;            /* la casilla de la nota retrasada (EDx) */
    zxm_env_t venv, penv;
    uint32_t fade;              /* 0-65536: lo que queda tras soltar la nota */
    uint8_t av_pos;
    uint16_t av_sweep;          /* el vibrato automático del instrumento */
    uint8_t orn_pos, orn_len, orn_loop;
    uint32_t orn;               /* el ornamento: su primera línea, o 0 */
} zxm_strk_t;

typedef struct {
    const uint8_t *m;
    uint32_t n;
    uint8_t ay_tracks, smp_tracks, flags, speed, tempo, delay_cnt, looped;
    uint8_t tick, row_delay, gvol;  /* el tick de la fila, EEx y el volumen global (0-64) */
    uint16_t npos, loop, pos, rows, row;
    int16_t jump_pos, jump_row;  /* B y D: a dónde va al acabar la fila, o -1 */
    uint32_t o[8];              /* ZXM_O_* */
    uint8_t n_ayins, n_smpins, n_orns, n_samples;
    uint16_t n_patterns;
    zxm_reader_t rd[18];        /* las del AY y luego las de muestras */
    pt3_t ay[2];                /* las pistas del AY: el motor del PT3 (pt3_tick) */
    zxm_strk_t st[12];
    mus_smp_t out[12];          /* lo del último paso */
    mus_rt_t rt;                /* lo de music.c mientras suena */
} zxm_t;

int zxm_init(zxm_t *s, const uint8_t *m, uint32_t n);
void zxm_step(zxm_t *s, mus_ay_t ay[2]);

/* --- Deflate (inflate.c): un .vgz, poco a poco --------------------------------- */

/* Un código de Huffman canónico: cuántos hay de cada largo y los símbolos
 * en su orden. */
typedef struct {
    uint16_t count[16];
    uint16_t sym[288];
} mus_huff_t;

/* Lo que va saliendo, en una ventana de 32 KB (la distancia más larga del
 * deflate): win[out & MUS_WIN_MASK]. */
#define MUS_WIN         32768u
#define MUS_WIN_MASK    (MUS_WIN - 1u)

typedef struct {
    const uint8_t *in;
    uint32_t n, pos;                    /* los datos comprimidos y por dónde va */
    uint32_t bits;
    int nbits;
    uint8_t *win;
    uint32_t out;                       /* los bytes que han salido */
    uint8_t state, last;                /* el bloque: cabecera, guardado, códigos, fin, error */
    uint32_t left;                      /* lo que queda de un bloque guardado */
    uint16_t mlen, mdist;               /* una copia a medias */
    mus_huff_t lit, dist;
} mus_inflate_t;

/* Los datos comprimidos de un .gz (m, n bytes): dónde empiezan, y en *size
 * lo que ocupan sin comprimir; -1 si no es un .gz. */
int mus_gzip_data(const uint8_t *m, uint32_t n, uint32_t *at, uint32_t *size);
void mus_inflate_init(mus_inflate_t *z, const uint8_t *in, uint32_t n, uint8_t *win);
/* Hasta want bytes más en la ventana: los que han salido (0 al final o con
 * un error). */
uint32_t mus_inflate_run(mus_inflate_t *z, uint32_t want);

/* --- VGM (vgm.c): los AY ------------------------------------------------------ */

typedef struct {
    mus_rt_t rt;                        /* lo de music.c mientras suena */
    const uint8_t *m;                   /* el fichero (en un .vgz, comprimido) */
    uint32_t n;
    uint8_t gz;
    mus_inflate_t z;                    /* un .vgz: lo que sale, en la ventana */
    uint32_t pos;                       /* por dónde va (sin comprimir) */
    uint32_t data, loop, end;           /* los comandos, la vuelta (0: sin ella) y el final */
    uint16_t version;
    uint8_t looped, stopped;
    uint32_t frac, step_us;             /* el tiempo: de 1/44.100 s a microsegundos */
    /* Los AY: lo que escribe el fichero y la escala de sus periodos a nuestro
     * reloj (16.16). */
    uint8_t ays;                        /* 0, 1 o 2 */
    uint32_t ay_k;
    uint8_t ay_raw[2][16], ay_r13;
} vgm_t;

/* Un VGM (o un .vgz, con work: la ventana, MUS_WIN bytes) en v: 0, o -1 si
 * no es uno que tenga AY. */
int vgm_init(vgm_t *v, const uint8_t *m, uint32_t n, uint8_t *work);
void vgm_step(vgm_t *v, mus_ay_t ay[2]);
/* Si parece un VGM con AY (1, o 2 con dos AY; o un .gz, que puede serlo):
 * sin memoria, para reconocerlo. 0 si no. */
int vgm_probe(const uint8_t *m, uint32_t n);

/* --- Cualquier formato (core.c) ----------------------------------------------- */

typedef struct {
    uint8_t format;             /* ZXV_MUSIC_... */
    uint8_t chips;              /* los AY que usa: bit 0 el primero, bit 1 el segundo */
    /* El estado de un .zxm no cabe en el de los demás (y casi nadie lo usa):
     * la memoria la pone quien toca la canción, antes de mus_song_init. Sin
     * ella, un .zxm se reconoce pero no suena. zxm_moves: está detrás del
     * módulo y se mueve con él (en la ROM, en el almacén del BASIC). */
    zxm_t *zxm;
    uint8_t zxm_moves;
    /* Lo mismo para un VGM (en la misma memoria), y la ventana de un .vgz. */
    vgm_t *vgm;
    uint8_t *work;
    union {
        pt3_t pt3[2];
        pt2_t pt2;
        stc_t stc;
        asc_t asc;
        psg_t psg;
    } u;
} mus_song_t;

/* Lo que dura el paso siguiente, en microsegundos (20.000 salvo en un .zxm,
 * donde lo da el tempo). */
uint32_t mus_song_step_us(const mus_song_t *s);
/* Los canales de muestras tras el último paso (12), o NULL si no usa ninguno. */
const mus_smp_t *mus_song_samples(const mus_song_t *s);
/* Sube len bytes (múltiplo de 4) de src a la memoria de sonido, en dst
 * (zxv_smp_upload, en music.c): para la ROM. */
int mus_smp_upload(uint32_t dst, const void *src, uint32_t len);

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
    uint32_t next;              /* la hora del siguiente paso (con un .zxm, el de los efectos) */
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
