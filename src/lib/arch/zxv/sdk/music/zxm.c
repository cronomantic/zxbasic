/*
 * zxm.c - El reproductor del formato propio, .zxm (zxm_format.h, D98): las
 * pistas de los dos AY y las de los canales de muestras, con un tempo y una
 * lista de posiciones. Sin la API de la máquina (también se compila en el
 * PC): da los registros de los AY de cada paso y lo que pide a cada canal de
 * muestras (mus_smp_t); music.c los escribe a su hora.
 *
 * Las pistas del AY son las del PT3: sus instrumentos y ornamentos son los
 * del PT3 byte a byte, y lo que hace cada orden y cada paso es el mismo
 * código (pt3.c). Un PT3 convertido suena igual registro a registro.
 */
#include "music_int.h"
#include "zxm_format.h"

#define B(i)    mus_b(s->m, s->n, i)
#define W(i)    mus_w(s->m, s->n, i)

static uint32_t L(const zxm_t *s, uint32_t i)
{
    return W(i) | (uint32_t)W(i + 2) << 16;
}

static void set_position(zxm_t *s, int pos, int row);

int zxm_init(zxm_t *s, const uint8_t *m, uint32_t n)
{
    static const char sig[] = ZXM_MAGIC;
    if (n < ZXM_HEADER)
        return -1;
    for (int i = 0; i < 8; i++)
        if (m[i] != (uint8_t)sig[i])
            return -1;
    uint8_t *p = (uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; i++)
        p[i] = 0;
    s->m = m;
    s->n = n;
    s->ay_tracks = B(ZXM_H_AY);
    s->smp_tracks = B(ZXM_H_SMP);
    if ((s->ay_tracks != 0 && s->ay_tracks != 3 && s->ay_tracks != 6) || s->smp_tracks > 12 ||
        s->ay_tracks + s->smp_tracks == 0)
        return -1;
    s->flags = B(ZXM_H_FLAGS);
    s->speed = B(ZXM_H_SPEED);
    s->tempo = B(ZXM_H_TEMPO);
    if (s->tempo < 32)
        s->tempo = 125;
    s->npos = W(ZXM_H_POSITIONS);
    s->loop = W(ZXM_H_LOOP);
    s->n_patterns = W(ZXM_H_PATTERNS);
    s->n_ayins = B(ZXM_H_AYINS);
    s->n_smpins = B(ZXM_H_SMPINS);
    s->n_orns = B(ZXM_H_ORNS);
    s->n_samples = B(ZXM_H_SAMPLES);
    for (int i = 0; i < 8; i++)
        s->o[i] = L(s, ZXM_H_OFFSETS + 4 * i);
    if (!s->npos || s->npos > 256)
        return -1;
    if (s->loop >= s->npos)
        s->loop = 0;

    for (int a = 0; a < 2; a++) {
        /* El motor del PT3 ve el fichero desde los instrumentos del AY: ellos
         * y los ornamentos caben en 64 KB (pt3_chan_t los cuenta en 16
         * bits). */
        pt3_t *ay = &s->ay[a];
        uint32_t base = s->o[ZXM_O_AYINS] < n ? s->o[ZXM_O_AYINS] : n;
        ay->m = m + base;
        ay->n = n - base;
        /* Cada AY, con su tabla de notas y su PT3 (las dos mitades de un
         * TurboSound pueden ser distintas). */
        if (a == 0)
            pt3_tables(ay, ZXM_F_PT3VER(s->flags), B(ZXM_H_AYTABLE));
        else
            pt3_tables(ay, ZXM_F_PT3VER(B(ZXM_H_AY2)), B(ZXM_H_AY2) & 3);
        for (int c = 0; c < 3; c++) {
            pt3_chan_t *ch = &ay->ch[c];
            if (s->n_ayins > 1)
                pt3_sample_at(ay, ch, L(s, s->o[ZXM_O_AYINS] + 4) - base);
            if (s->n_orns > 0)
                pt3_ornament_at(ay, ch, L(s, s->o[ZXM_O_ORNS]) - base);
            ch->volume = 15;
        }
    }
    s->gvol = B(ZXM_H_GLOBAL) > 64 ? 64 : B(ZXM_H_GLOBAL);
    for (int t = 0; t < s->smp_tracks; t++) {
        zxm_strk_t *c = &s->st[t];
        c->vol = 64;
        c->pan = B(ZXM_H_PAN + t);
        c->delay_tick = c->cut_tick = c->off_tick = 0xFF;
        c->fade = 65536;
        s->out[t].on = 1;
        s->out[t].pan = c->pan;
    }
    s->jump_pos = s->jump_row = -1;
    s->delay_cnt = 1;
    set_position(s, 0, 0);
    return 0;
}

/* --- Las posiciones y los patrones ----------------------------------------------- */

static int read_cell(zxm_t *s, zxm_reader_t *r, zxm_cell_t *c);

static void set_position(zxm_t *s, int pos, int row)
{
    s->pos = (uint16_t)pos;
    uint32_t p = s->o[ZXM_O_POSITIONS] + (uint32_t)pos * (2u + 2u * (s->ay_tracks + s->smp_tracks));
    s->rows = W(p);
    if (!s->rows)
        s->rows = 1;
    s->row = 0;
    for (int t = 0; t < s->ay_tracks + s->smp_tracks; t++) {
        zxm_reader_t *r = &s->rd[t];
        uint16_t pat = W(p + 2 + 2 * t);
        r->addr = 0;
        r->left = 0;
        r->wait = 0;
        if (pat < s->n_patterns) {
            uint32_t a = L(s, s->o[ZXM_O_PATTERNS] + 4u * pat);
            r->left = W(a + 1);
            r->addr = a + 3;
        }
    }
    /* Empezar en una fila (D): las de antes, leídas y sin hacer. */
    zxm_cell_t skip;
    for (; s->row < row && s->row < s->rows; s->row++)
        for (int t = 0; t < s->ay_tracks + s->smp_tracks; t++)
            read_cell(s, &s->rd[t], &skip);
}

/* La casilla de la fila de una pista: 0 si está vacía. */
static int read_cell(zxm_t *s, zxm_reader_t *r, zxm_cell_t *c)
{
    uint8_t *z = (uint8_t *)c;
    for (unsigned i = 0; i < sizeof *c; i++)
        z[i] = 0;
    if (!r->addr || !r->left)
        return 0;
    r->left--;
    if (r->wait) {
        r->wait--;
        return 0;
    }
    uint32_t a = r->addr;
    uint8_t b = B(a++);
    if (b < 0x80) {
        r->wait = b;
        r->addr = a;
        return 0;
    }
    c->has = b & 0x7F;
    if (c->has & ZXM_C_NOTE)
        c->note = B(a++);
    if (c->has & ZXM_C_INS)
        c->ins = B(a++);
    if (c->has & ZXM_C_VOL)
        c->vol = B(a++);
    if (c->has & ZXM_C_ORN)
        c->orn = B(a++);
    if (c->has & ZXM_C_ENV) {
        c->env = B(a);
        c->env_period = W(a + 1);
        a += 3;
    }
    for (int k = 0; k < 2; k++)
        if (c->has & (ZXM_C_FX1 << k)) {
            c->fx[k] = B(a++);
            int len = zxm_fx_len(c->fx[k]);
            for (int i = 0; i < 3; i++)
                c->fxp[k][i] = i < len ? B(a + i) : 0;
            a += (uint32_t)len;
        }
    r->addr = a;
    return 1;
}

/* Lo que vale para todas las pistas: el tempo y los saltos. */
static void global_fx(zxm_t *s, int code, const uint8_t *p)
{
    switch (code) {
    case 'F':                           /* el del MOD: velocidad, o desde 32 el tempo */
        if (p[0] >= 32)
            s->tempo = p[0];
        else if (p[0])
            s->speed = s->delay_cnt = p[0];
        break;
    case 't':                           /* el del PT3: la velocidad, 0-255 */
        s->speed = s->delay_cnt = p[0];
        break;
    case 'B':
        s->jump_pos = p[0];
        if (s->jump_row < 0)
            s->jump_row = 0;
        break;
    case 'D':
        if (s->jump_pos < 0)
            s->jump_pos = (int16_t)(s->pos + 1);
        s->jump_row = p[0];
        break;
    }
}

/* --- Las pistas del AY ------------------------------------------------------------ */

static void ay_cell(zxm_t *s, int t, const zxm_cell_t *cell)
{
    pt3_t *ay = &s->ay[t / 3];
    pt3_chan_t *c = &ay->ch[t % 3];
    int prev_note = c->note;
    int16_t prev_sliding = c->sliding;
    uint32_t base = s->o[ZXM_O_AYINS];
    if ((cell->has & ZXM_C_INS) && cell->ins < s->n_ayins)
        pt3_sample_at(ay, c, L(s, s->o[ZXM_O_AYINS] + 4u * cell->ins) - base);
    if ((cell->has & ZXM_C_ORN) && cell->orn < s->n_orns)
        pt3_ornament_at(ay, c, L(s, s->o[ZXM_O_ORNS] + 4u * cell->orn) - base);
    if ((cell->has & ZXM_C_VOL) && cell->vol >= 1 && cell->vol <= 15)
        c->volume = cell->vol;
    if (cell->has & ZXM_C_ENV) {
        if (cell->env & 15)
            pt3_env(ay, c, cell->env & 15, cell->env_period);
        if (!(cell->env & 15) || (cell->env & ZXM_ENV_SET_ONLY))
            pt3_env_off(c);
    }
    if (cell->has & ZXM_C_NOTE) {
        if (cell->note >= 1 && cell->note <= 96)
            pt3_note(c, cell->note - 1);
        else if (cell->note == ZXM_NOTE_OFF || cell->note == ZXM_NOTE_CUT)
            pt3_rest(c);
    }
    for (int k = 0; k < 2; k++) {
        if (!(cell->has & (ZXM_C_FX1 << k)))
            continue;
        const uint8_t *p = cell->fxp[k];
        int16_t step = (int16_t)(p[1] | p[2] << 8);
        switch (cell->fx[k]) {
        case 'g': pt3_fx_gliss(ay, c, p[0], step); break;
        case 'p': pt3_fx_porta(ay, c, p[0], step, prev_note, prev_sliding); break;
        case '9': c->pos_smp = p[0]; break;
        case 'o': c->pos_orn = p[0]; break;
        case 'i': pt3_fx_onoff(c, p[0], p[1]); break;
        case 'e': pt3_fx_env_slide(ay, p[0], step); break;
        case 'n': ay->noise_base = p[0]; break;
        default: global_fx(s, cell->fx[k], p); break;
        }
    }
}

/* --- Las pistas de muestras ------------------------------------------------------- */

/* 2^(s/12) y 2^(f/768) - 1, en 16.16: la altura en 1/64 de semitono, sin
 * coma flotante. */
static const uint32_t semi_tab[12] = {
    65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218, 116772, 123715,
};
static const uint16_t fine_tab[64] = {
    0, 59, 118, 178, 237, 296, 356, 415, 475, 535, 594, 654, 714, 773, 833, 893, 953, 1013, 1073,
    1134, 1194, 1254, 1314, 1375, 1435, 1496, 1556, 1617, 1677, 1738, 1799, 1859, 1920, 1981, 2042,
    2103, 2164, 2225, 2287, 2348, 2409, 2471, 2532, 2593, 2655, 2716, 2778, 2840, 2902, 2963, 3025,
    3087, 3149, 3211, 3273, 3335, 3397, 3460, 3522, 3584, 3647, 3709, 3772, 3834,
};
/* El cuarto de onda del vibrato y el trémolo del ProTracker. */
static const uint8_t sine_tab[32] = {
    0, 24, 49, 74, 97, 120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
    255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97, 74, 49, 24,
};

#define K_PAL   19370773u               /* 14.187.578 (el reloj del Amiga PAL x 4) x 65.536 / 48.000 */
#define K_S3M   19548100u               /* 8.363 x 1.712 x 65.536 / 48.000 */
#define NONE    0xFF

/* 2^(x/768) en 16.16. */
static uint32_t pow2_768(int32_t x)
{
    int32_t oct = x >= 0 ? x / 768 : -((-x + 767) / 768);
    int32_t r = x - oct * 768;
    uint64_t v = (uint64_t)semi_tab[r >> 6] * (65536u + fine_tab[r & 63]) >> 16;
    if (oct >= 0)
        v <<= oct > 16 ? 16 : oct;
    else
        v = oct < -31 ? 0 : v >> -oct;
    return v > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)v;
}

/* Una onda del vibrato o el trémolo (0 seno, 1 sierra, 2 cuadrada) en pos
 * (0-63): -255..255. */
static int wave(int w, int pos)
{
    pos &= 63;
    switch (w & 3) {
    case 1: return 255 - pos * 8;
    case 2: return pos < 32 ? 255 : -255;
    default: return pos < 32 ? sine_tab[pos] : -sine_tab[pos - 32];
    }
}

static int linear(const zxm_t *s)
{
    return (s->flags & ZXM_F_LINEAR) != 0;
}

/* La altura de una nota (0-95) con la afinación de la muestra: el periodo x
 * 4 (C-4: 1.712, el C-2 de un MOD) o la nota x 64 (C-4: 3.072). */
static int32_t note_pitch(const zxm_t *s, const zxm_strk_t *c, int note)
{
    int32_t x = (note - 48 + c->rel) * 64 + c->fine / 2;
    if (linear(s))
        return 3072 + x;
    uint32_t p = pow2_768(x);
    return p ? (int32_t)((((uint64_t)1712) << 16) / p) : 0x7FFF;
}

/* El paso del canal (8.16) con un desplazamiento en 1/64 de semitono. */
static uint32_t pitch_step(const zxm_t *s, int32_t pitch, int32_t semi64)
{
    uint64_t st;
    if (linear(s)) {
        st = (uint64_t)8363 * pow2_768(pitch + semi64 - 3072) / 48000u;
    } else {
        if (pitch < 1)
            pitch = 1;
        st = ((s->flags & ZXM_F_S3MCLOCK) ? K_S3M : K_PAL) / (uint32_t)pitch;
        if (semi64)
            st = st * pow2_768(semi64) >> 16;
    }
    return st > 0xFFFFFFu ? 0xFFFFFFu : (uint32_t)st;
}

/* Sube (d > 0) o baja la altura d unidades. */
static void slide(const zxm_t *s, zxm_strk_t *c, int32_t d)
{
    c->pitch += linear(s) ? d : -d;
    if (c->pitch < 1)
        c->pitch = 1;
    if (!linear(s) && c->pitch > 0x7FFF * 4)
        c->pitch = 0x7FFF * 4;
}

static void porta(zxm_strk_t *c, int32_t d)
{
    if (c->pitch < c->target)
        c->pitch = c->pitch + d > c->target ? c->target : c->pitch + d;
    else if (c->pitch > c->target)
        c->pitch = c->pitch - d < c->target ? c->target : c->pitch - d;
}

static void vol_slide(zxm_strk_t *c, uint8_t p)
{
    int v = c->vol + (p >> 4) - (p & 15);
    if (p >> 4 && p & 15)
        v = c->vol + (p >> 4);          /* los dos: cuenta el de subir, como en el MOD */
    c->vol = (uint8_t)(v < 0 ? 0 : v > 64 ? 64 : v);
}

static uint32_t sins_at(const zxm_t *s, int ins)
{
    return s->o[ZXM_O_SMPINS] + (uint32_t)(ins - 1) * ZXM_SINS_SIZE;
}

static uint32_t sample_at(const zxm_t *s, int smp)
{
    return s->o[ZXM_O_SAMPLES] + (uint32_t)smp * ZXM_SMP_SIZE;
}

/* La muestra de una nota con el instrumento de la pista; sin instrumentos
 * de muestras, el instrumento es la muestra (1, la 0). -1 si no hay. */
static int note_sample(const zxm_t *s, const zxm_strk_t *c, int note)
{
    int smp = -1;
    if (c->ins && c->ins <= s->n_smpins)
        smp = B(sins_at(s, c->ins) + ZXM_SI_MAP + (uint32_t)note);
    else if (c->ins && !s->n_smpins)
        smp = c->ins - 1;
    return smp >= 0 && smp < s->n_samples ? smp : -1;
}

/* Los de la muestra: afinación, y volumen y panorama por defecto. */
static void load_sample(const zxm_t *s, zxm_strk_t *c, int smp, int defaults)
{
    uint32_t a = sample_at(s, smp);
    c->smp = (uint8_t)smp;
    c->rel = (int8_t)B(a + ZXM_S_RELNOTE);
    c->fine = (int8_t)B(a + ZXM_S_FINE);
    if (defaults) {
        c->vol = B(a + ZXM_S_VOL) > 64 ? 64 : B(a + ZXM_S_VOL);
        if (B(a + ZXM_S_FLAGS) & ZXM_SF_PAN)
            c->pan = B(a + ZXM_S_PAN);
    }
}

static void trigger(zxm_strk_t *c, mus_smp_t *o, int offset)
{
    c->playing = 1;
    c->keyon = 1;
    c->venv.pos = c->penv.pos = 0;
    c->fade = 65536;
    c->av_pos = 0;
    c->av_sweep = 0;
    if (!(c->vib_wave & 4))
        c->vib_pos = 0;
    if (!(c->trem_wave & 4))
        c->trem_pos = 0;
    c->retrig_cnt = c->tremor_cnt = 0;
    c->orn_pos = 0;
    o->key = 1;
    o->slot = c->smp;
    o->offset = (uint8_t)offset;
}

/* Soltar la nota (97 o Kxx): sin envolvente de volumen, calla. */
static void key_off(const zxm_t *s, zxm_strk_t *c)
{
    c->keyon = 0;
    if (!(c->ins && c->ins <= s->n_smpins && (B(sins_at(s, c->ins) + ZXM_SI_FLAGS) & ZXM_SIF_VENV)))
        c->vol = 0;
}

static int has_fx(const zxm_cell_t *cell, int code)
{
    return ((cell->has & ZXM_C_FX1) && cell->fx[0] == code) || ((cell->has & ZXM_C_FX2) && cell->fx[1] == code);
}

/* La nota, el instrumento, el volumen y el ornamento de una casilla (en el
 * tick 0, o en el de EDx). */
static void smp_note(zxm_t *s, int t, const zxm_cell_t *cell)
{
    zxm_strk_t *c = &s->st[t];
    mus_smp_t *o = &s->out[t];
    int porta_fx = has_fx(cell, '3') || has_fx(cell, '5');
    if (cell->has & ZXM_C_INS)
        c->ins = cell->ins;
    if ((cell->has & ZXM_C_NOTE) && cell->note >= 1 && cell->note <= 96) {
        int n = cell->note - 1, smp = note_sample(s, c, n);
        if (porta_fx && c->playing) {
            c->target = note_pitch(s, c, n);
            if ((cell->has & ZXM_C_INS) && smp >= 0) {     /* con instrumento: su volumen */
                uint32_t a = sample_at(s, smp);
                c->vol = B(a + ZXM_S_VOL) > 64 ? 64 : B(a + ZXM_S_VOL);
            }
        } else if (smp >= 0) {
            load_sample(s, c, smp, (cell->has & ZXM_C_INS) != 0);
            c->note = (uint8_t)n;
            c->pitch = c->target = note_pitch(s, c, n);
            trigger(c, o, has_fx(cell, '9') ? c->m_offset : 0);
        }
    } else if ((cell->has & ZXM_C_NOTE) && cell->note == ZXM_NOTE_OFF) {
        key_off(s, c);
    } else if ((cell->has & ZXM_C_NOTE) && cell->note == ZXM_NOTE_CUT) {
        c->playing = 0;
        o->stop = 1;
    } else if ((cell->has & ZXM_C_INS) && c->playing) {
        load_sample(s, c, c->smp, 1);   /* el instrumento solo: su volumen y panorama otra vez */
        c->venv.pos = c->penv.pos = 0;
        c->fade = 65536;
        c->keyon = 1;
    }
    if (cell->has & ZXM_C_VOL)
        c->vol = cell->vol > 64 ? 64 : cell->vol;
    if ((cell->has & ZXM_C_ORN) && cell->orn < s->n_orns) {
        uint32_t a = L(s, s->o[ZXM_O_ORNS] + 4u * cell->orn);
        c->orn_loop = B(a);
        c->orn_len = B(a + 1);
        c->orn = a + 2;
        c->orn_pos = 0;
    }
}

/* Una casilla de una pista de muestras, en el tick 0. */
static void smp_cell(zxm_t *s, int t, const zxm_cell_t *cell)
{
    zxm_strk_t *c = &s->st[t];
    c->delay_tick = c->cut_tick = c->off_tick = NONE;
    c->fx[0] = c->fx[1] = 0;
    for (int k = 0; k < 2; k++)
        if (cell->has & (ZXM_C_FX1 << k)) {
            c->fx[k] = cell->fx[k];
            c->fxp[k] = cell->fxp[k][0];
        }
    /* Lo que la nota necesita antes: desde dónde (9xx) y si espera (EDx). */
    for (int k = 0; k < 2; k++) {
        uint8_t p = c->fxp[k];
        if (c->fx[k] == '9' && p)
            c->m_offset = p;
        if (c->fx[k] == 'E' && p >> 4 == 0xD && (p & 15))
            c->delay_tick = p & 15;
    }
    if (c->delay_tick != NONE)
        c->pend = *cell;
    else
        smp_note(s, t, cell);
    for (int k = 0; k < 2; k++) {
        uint8_t p = c->fxp[k], x = p >> 4, y = p & 15;
        switch (c->fx[k]) {
        case '3':
            if (p)
                c->m_porta = p;
            break;
        case '4':
        case 'U':
            if (x)
                c->m_vib = (uint8_t)((c->m_vib & 15) | x << 4);
            if (y)
                c->m_vib = (uint8_t)((c->m_vib & 0xF0) | y);
            break;
        case '7':
            if (x)
                c->m_trem = (uint8_t)((c->m_trem & 15) | x << 4);
            if (y)
                c->m_trem = (uint8_t)((c->m_trem & 0xF0) | y);
            break;
        case '1':
        case '2':
            if (p || (s->flags & ZXM_F_MODFX))
                c->m_slide = p;
            break;
        case '5':
        case '6':
        case 'A':
            if (p || (s->flags & ZXM_F_MODFX))  /* en el MOD, 00 no hace nada */
                c->m_vol = p;
            if (s->flags & ZXM_F_ST300)
                vol_slide(c, c->m_vol);
            break;
        case '8':
            c->pan = p;
            break;
        case 'C':
            c->vol = p > 64 ? 64 : p;
            break;
        case 'G':
            s->gvol = p > 64 ? 64 : p;
            break;
        case 'H':
            if (p)
                c->m_gvol = p;
            break;
        case 'I':
            if (p)
                c->m_tremor = p;
            break;
        case 'K':
            c->off_tick = p;
            break;
        case 'L':
            c->venv.pos = c->penv.pos = p;
            break;
        case 'P':
            if (p)
                c->m_pan = p;
            break;
        case 'R':
            if (p)
                c->m_retrig = p;
            break;
        case 'X':
            if (p)
                c->m_xfine = p;
            if (c->m_xfine >> 4 == 1)
                slide(s, c, c->m_xfine & 15);
            else if (c->m_xfine >> 4 == 2)
                slide(s, c, -(c->m_xfine & 15));
            break;
        case 'E':
            switch (x) {
            case 0x1: slide(s, c, 4 * y); break;           /* fino hacia arriba */
            case 0x2: slide(s, c, -4 * y); break;
            case 0x4: c->vib_wave = y; break;
            case 0x5: c->fine = (int8_t)((y - (y >= 8 ? 16 : 0)) * 16); break;
            case 0x6:                                       /* bucle de filas */
                if (!y) {
                    c->loop_row = (uint8_t)s->row;
                } else if (!c->loop_cnt) {
                    c->loop_cnt = y;
                    s->jump_pos = (int16_t)s->pos;
                    s->jump_row = c->loop_row;
                } else if (--c->loop_cnt) {
                    s->jump_pos = (int16_t)s->pos;
                    s->jump_row = c->loop_row;
                }
                break;
            case 0x7: c->trem_wave = y; break;
            case 0x8: c->pan = (uint8_t)(y * 17); break;
            case 0xA: vol_slide(c, (uint8_t)(y << 4)); break;
            case 0xB: vol_slide(c, y); break;
            case 0xC: c->cut_tick = y; break;
            case 0xE: s->row_delay = y; break;
            }
            break;
        default:
            global_fx(s, c->fx[k], cell->fxp[k]);
            break;
        }
    }
}

/* Un tick de una pista de muestras: los efectos y lo que pide al canal. */
static void smp_tick(zxm_t *s, int t)
{
    zxm_strk_t *c = &s->st[t];
    mus_smp_t *o = &s->out[t];
    int tick = s->tick;
    if (tick == c->delay_tick)
        smp_note(s, t, &c->pend);
    if (tick == c->cut_tick)
        c->vol = 0;
    if (tick == c->off_tick)
        key_off(s, c);
    c->vib = 0;
    c->vol_add = 0;
    c->arp = 0;
    int tremor_off = 0;
    for (int k = 0; k < 2; k++) {
        uint8_t p = c->fxp[k], x = p >> 4, y = p & 15;
        int later = tick > 0;
        switch (c->fx[k]) {
        case '0':
            if (p)
                c->arp = (uint8_t)(tick % 3 == 1 ? x : tick % 3 == 2 ? y : 0);
            break;
        case '1':
            if (p)
                c->m_slide = p;
            if (later)
                slide(s, c, 4 * c->m_slide);
            break;
        case '2':
            if (p)
                c->m_slide = p;
            if (later)
                slide(s, c, -4 * c->m_slide);
            break;
        case '3':
        case '5':
            if (later)
                porta(c, 4 * c->m_porta);
            if (c->fx[k] == '5' && later)
                vol_slide(c, c->m_vol);
            break;
        case '4':
        case 'U':
        case '6':
            c->vib = (int16_t)(wave(c->vib_wave, c->vib_pos) * (c->m_vib & 15) >> (c->fx[k] == 'U' ? 7 : 5));
            if (later)
                c->vib_pos = (uint8_t)(c->vib_pos + (c->m_vib >> 4));
            if (c->fx[k] == '6' && later)
                vol_slide(c, c->m_vol);
            break;
        case '7':
            c->vol_add = (int8_t)(wave(c->trem_wave, c->trem_pos) * (c->m_trem & 15) >> 6);
            if (later)
                c->trem_pos = (uint8_t)(c->trem_pos + (c->m_trem >> 4));
            break;
        case 'A':
            if (later)
                vol_slide(c, c->m_vol);
            break;
        case 'H':
            if (later) {
                int g = s->gvol + (c->m_gvol >> 4) - (c->m_gvol & 15);
                s->gvol = (uint8_t)(g < 0 ? 0 : g > 64 ? 64 : g);
            }
            break;
        case 'I': {
            int on = (c->m_tremor >> 4) + 1, off = (c->m_tremor & 15) + 1;
            tremor_off = c->tremor_cnt % (on + off) >= on;
            c->tremor_cnt++;
            break;
        }
        case 'P':
            if (later) {
                int v = c->pan + (c->m_pan >> 4) - (c->m_pan & 15);
                c->pan = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
            }
            break;
        case 'R':
            if (later && (c->m_retrig & 15) && ++c->retrig_cnt >= (c->m_retrig & 15)) {
                static const int8_t add[16] = { 0, -1, -2, -4, -8, -16, 0, 0, 0, 1, 2, 4, 8, 16, 0, 0 };
                int r = c->m_retrig >> 4, v = c->vol + add[r];
                if (r == 6)
                    v = v * 2 / 3;
                else if (r == 7)
                    v /= 2;
                else if (r == 14)
                    v = v * 3 / 2;
                else if (r == 15)
                    v *= 2;
                c->vol = (uint8_t)(v < 0 ? 0 : v > 64 ? 64 : v);
                c->retrig_cnt = 0;
                if (c->playing)
                    trigger(c, o, 0);
            }
            break;
        case 'E':
            if (x == 0x9 && y && later && tick % y == 0 && c->playing)
                trigger(c, o, 0);
            break;
        }
        (void)y;
    }

    /* El ornamento: un arpegio que sigue solo. */
    int semi = c->arp;
    if (c->orn && c->orn_len) {
        semi += (int8_t)B(c->orn + c->orn_pos);
        if (++c->orn_pos >= c->orn_len)
            c->orn_pos = c->orn_loop < c->orn_len ? c->orn_loop : 0;
    }

    /* Lo del instrumento: las envolventes, el desvanecer y el vibrato
     * automático. */
    int env_vol = 64, env_pan = 32, av = 0;
    if (c->ins && c->ins <= s->n_smpins && c->playing) {
        uint32_t a = sins_at(s, c->ins);
        uint8_t f = B(a + ZXM_SI_FLAGS);
        for (int e = 0; e < 2; e++) {
            uint8_t on = e ? ZXM_SIF_PENV : ZXM_SIF_VENV;
            if (!(f & on))
                continue;
            uint32_t pts = a + (e ? ZXM_SI_PENV : ZXM_SI_VENV), cnt = pts + 48;
            zxm_env_t *ev = e ? &c->penv : &c->venv;
            int n = B(cnt) > 12 ? 12 : B(cnt), sus = B(cnt + 1), ls = B(cnt + 2), le = B(cnt + 3);
            if (n < 1)
                continue;
            int i = 0;
            while (i + 1 < n && W(pts + 4u * (uint32_t)(i + 1)) <= ev->pos)
                i++;
            int v = W(pts + 4u * (uint32_t)i + 2);
            if (i + 1 < n) {
                int t0 = W(pts + 4u * (uint32_t)i), t1 = W(pts + 4u * (uint32_t)(i + 1));
                int v1 = W(pts + 4u * (uint32_t)(i + 1) + 2);
                if (t1 > t0)
                    v += (v1 - v) * (ev->pos - t0) / (t1 - t0);
            }
            int hold = c->keyon && (f & (e ? ZXM_SIF_PSUS : ZXM_SIF_VSUS)) && sus < n &&
                       ev->pos == W(pts + 4u * (uint32_t)sus);
            if (!hold) {
                ev->pos++;
                if ((f & (e ? ZXM_SIF_PLOOP : ZXM_SIF_VLOOP)) && le < n && ls <= le &&
                    ev->pos >= W(pts + 4u * (uint32_t)le))
                    ev->pos = W(pts + 4u * (uint32_t)ls);
            }
            if (e)
                env_pan = v;
            else
                env_vol = v > 64 ? 64 : v;
        }
        if (!c->keyon) {
            uint32_t fo = W(a + ZXM_SI_FADE);
            c->fade = c->fade > fo ? c->fade - fo : 0;
        }
        uint8_t vt = B(a + ZXM_SI_VIBRATO), sweep = B(a + ZXM_SI_VIBRATO + 1);
        uint8_t depth = B(a + ZXM_SI_VIBRATO + 2), rate = B(a + ZXM_SI_VIBRATO + 3);
        if (depth) {
            av = wave(vt, c->av_pos >> 2) * depth;
            if (sweep && c->av_sweep < sweep) {
                av = av * c->av_sweep / sweep;
                c->av_sweep++;
            }
            av >>= 8;
            c->av_pos = (uint8_t)(c->av_pos + rate);
        }
    }

    int vol = c->vol + c->vol_add;
    vol = vol < 0 ? 0 : vol > 64 ? 64 : vol;
    if (tremor_off)
        vol = 0;
    vol = (int)((uint32_t)vol * (uint32_t)env_vol / 64u * (c->fade >> 6) / 1024u * s->gvol / 64u);
    int pan = c->pan;
    if (env_pan != 32) {
        int room = 128 - (pan > 128 ? pan - 128 : 128 - pan);
        pan += (env_pan - 32) * room / 32;
        pan = pan < 0 ? 0 : pan > 255 ? 255 : pan;
    }
    o->on = 1;
    o->vol = (uint8_t)(c->playing ? vol : 0);
    o->pan = (uint8_t)pan;
    o->step = pitch_step(s, c->pitch + c->vib + av, semi * 64);
    o->ctrl = (s->flags & ZXM_F_NOINTERP) ? 0x04 : 0;    /* ZXV_SMP_NOINTERP */
}

/* --- Cada paso -------------------------------------------------------------------- */

static void do_row(zxm_t *s)
{
    if (s->row >= s->rows || s->jump_pos >= 0) {
        int pos = s->jump_pos >= 0 ? s->jump_pos : s->pos + 1;
        int row = s->jump_row >= 0 ? s->jump_row : 0;
        s->jump_pos = s->jump_row = -1;
        if (pos >= s->npos) {
            pos = s->loop;
            s->looped++;
        }
        set_position(s, pos, row);
        s->ay[0].noise_base = s->ay[1].noise_base = 0;     /* como el PT3, en cada posición */
    }
    zxm_cell_t cell;
    for (int t = 0; t < s->ay_tracks + s->smp_tracks; t++) {
        int got = read_cell(s, &s->rd[t], &cell);
        if (t < s->ay_tracks) {
            if (got)
                ay_cell(s, t, &cell);
        } else {
            if (!got)
                cell.has = 0;
            smp_cell(s, t - s->ay_tracks, &cell);   /* también vacía: se acaban los efectos */
        }
    }
    s->row++;
}

void zxm_step(zxm_t *s, mus_ay_t ay[2])
{
    s->ay[0].r13_write = s->ay[1].r13_write = 0;
    for (int t = 0; t < s->smp_tracks; t++)
        s->out[t].key = s->out[t].stop = 0;
    if (--s->delay_cnt == 0) {
        s->delay_cnt = s->speed;
        if (s->row_delay) {
            s->row_delay--;             /* EEx: la fila otra vez, sin leerla */
            s->tick++;
        } else {
            s->tick = 0;
            do_row(s);
        }
    } else {
        s->tick++;
    }
    for (int a = 0; a < s->ay_tracks / 3; a++)
        pt3_tick(&s->ay[a], &ay[a]);
    for (int t = 0; t < s->smp_tracks; t++)
        smp_tick(s, t);
}
