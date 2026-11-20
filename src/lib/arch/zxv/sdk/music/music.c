/*
 * music.c - La música y los efectos en la máquina: cada paso se calcula por
 * delante y sus registros van a su hora exacta con zxv_ay_event (spec §5.4),
 * solo los que cambian: los de los AY y, con un .zxm, los de los canales de
 * muestras, cuyas notas nuevas de cada paso arrancan a la vez (KEYON).
 *
 * Dos relojes: los efectos (AYFX) van a 50 pasos por segundo, y la canción
 * al suyo (en un .zxm lo da el tempo; en un VGM, cada espera; si no,
 * también 50).
 */
#include <stddef.h>

#include "music_int.h"
#include "zxm_format.h"
#include "zxv_api.h"
#include "zxv_music.h"

#define STEP_US     20000u

/* Todo el estado, en una estructura: en un programa, aquí mismo; en la ROM
 * (con MUS_STATE_EXTERN), donde lo pone ella (la memoria del BASIC). */
#ifdef MUS_STATE_EXTERN
extern mus_state_t *mus_state;
#define S (*mus_state)
#else
static mus_state_t mus_state_ = { .sfx_mask = -1 };
#define S mus_state_
#endif

/* El estado de un .zxm o de un VGM: en la ROM, detrás del módulo en su
 * objeto del almacén (bmusic.c lo reserva, alineado a 4; en un .vgz,
 * también su ventana, detrás); en un programa, aquí (y la ventana de un
 * .vgz, la que dé con zxv_music_work). */
#ifdef MUS_STATE_EXTERN
#define SONG_MEM(data, size) ((void *)(((uintptr_t)(data) + (size) + 3u) & ~(uintptr_t)3u))
#define SONG_MOVES 1
#define SONG_WORK(mem) ((uint8_t *)(mem) + sizeof(vgm_t))
#else
static union {
    zxm_t z;
    vgm_t v;
} song_mem_;
static uint8_t *work_;
#define SONG_MEM(data, size) ((void *)&song_mem_)
#define SONG_MOVES 0
#define SONG_WORK(mem) work_
#endif

/* Dónde pone zxv_music_play las muestras de un .zxm (zxv_music_samples). */
static uint32_t smp_base;
static uint8_t smp_slot;

static int ticking(void)
{
    return S.playing && !S.paused;
}

/* El estado del .zxm que es la canción, o NULL si no lo es. */
static zxm_t *Z(void)
{
    return S.song.format == ZXV_MUSIC_ZXM ? S.song.zxm : NULL;
}

/* Lo mismo de un VGM. */
static vgm_t *V(void)
{
    return S.song.format == ZXV_MUSIC_VGM ? S.song.vgm : NULL;
}

/* Lo de una canción con su propio reloj (un .zxm o un VGM), o NULL. */
static mus_rt_t *R(void)
{
    return Z() ? &Z()->rt : V() ? &V()->rt : NULL;
}

static int active(void)
{
    if (ticking() || (R() && R()->smp_live))
        return 1;
    for (int i = 0; i < 6; i++)
        if (S.sfx[i].p)
            return 1;
    return 0;
}

static void silent(mus_ay_t *ay)
{
    for (int i = 0; i < 14; i++)
        ay->r[i] = 0;
    ay->r[7] = 0x3F;
    ay->r13 = 0;
}

/* Lo que tapa un efecto en su canal. */
static void overlay(mus_ay_t *ay, int ch, const afx_t *a, int how)
{
    ay->r[8 + ch] = a->vol;
    if (how == 2)
        return;
    if (a->has_tone) {
        ay->r[ch * 2] = a->tone & 0xFF;
        ay->r[ch * 2 + 1] = a->tone >> 8;
    }
    if (a->has_noise)
        ay->r[6] = a->noise;
    uint8_t bits = (uint8_t)(1u << ch | 8u << ch);
    ay->r[7] = (ay->r[7] & ~bits) | ((a->mix & 1) << ch) | ((a->mix >> 1 & 1) << (ch + 3));
}

static void emit(uint32_t t, int base, const mus_ay_t *ay, int all)
{
    for (int r = 0; r < 13; r++) {      /* R13 solo cuando se escribe */
        uint8_t v = ay->r[r];
        if (all || S.sent[base + r] != v) {
            S.sent[base + r] = v;
            zxv_ay_event(t, base + r, v);
        }
    }
    if (ay->r13) {
        S.sent[base + 13] = ay->r[13];
        zxv_ay_event(t, base + 13, ay->r[13]);
    }
}

/* --- Los canales de muestras de la canción ------------------------------------------ */

static void smp_reg(mus_rt_t *z, uint32_t t, int ch, int reg, uint8_t v, int force)
{
    uint8_t *sent = &z->smp_sent[ch][reg];
    if (force || !(z->smp_valid >> ch & 1) || *sent != v) {
        *sent = v;
        zxv_ay_event(t, (int)ZXV_SMP_REG(ch, reg), v);
    }
}

/* Lo que pide la canción a sus canales en este paso (o, sin ella, que se
 * callen): lo que cambia, y las notas nuevas, arrancadas a la vez. */
static void emit_samples(mus_rt_t *z, uint32_t t)
{
    const mus_smp_t *o = ticking() ? mus_song_samples(&S.song) : NULL;
    uint32_t key = 0;
    for (int ch = 0; ch < 12; ch++) {
        uint16_t bit = (uint16_t)(1u << ch);
        if (z->fx_mask & bit) {                 /* lo tapa un efecto (D99) */
            if ((z->fx_loop & bit) || (int32_t)(t - z->fx_end[ch]) < 0)
                continue;
            z->fx_mask &= (uint16_t)~bit;       /* ya acabó: otra vez de la canción, entero */
            z->smp_valid &= (uint16_t)~bit;
        }
        int live = z->smp_live >> ch & 1;
        if (!o || !o[ch].on) {
            if (live) {                         /* la canción se para o se pausa */
                smp_reg(z, t, ch, ZXV_SMP_CTRL, ZXV_SMP_STOP, 1);
                z->smp_live &= (uint16_t)~(1u << ch);
            }
            continue;
        }
        const mus_smp_t *c = &o[ch];
        uint8_t slot = (uint8_t)(smp_slot + c->slot);
        if (c->key) {
            smp_reg(z, t, ch, ZXV_SMP_SLOT, slot, 0);
            smp_reg(z, t, ch, ZXV_SMP_OFFSET, c->offset, 0);
        }
        smp_reg(z, t, ch, ZXV_SMP_VOL, c->vol, 0);
        smp_reg(z, t, ch, ZXV_SMP_PAN, c->pan, 0);
        for (int i = 0; i < 3; i++)
            smp_reg(z, t, ch, ZXV_SMP_STEP + i, (uint8_t)(c->step >> 8 * i), 0);
        if (!(z->smp_valid >> ch & 1) || z->smp_sent[ch][ZXV_SMP_CTRL] != c->ctrl || c->stop)
            smp_reg(z, t, ch, ZXV_SMP_CTRL, (uint8_t)(c->ctrl | (c->stop ? ZXV_SMP_STOP : 0)), 1);
        z->smp_sent[ch][ZXV_SMP_CTRL] = c->ctrl;
        z->smp_valid |= (uint16_t)(1u << ch);
        z->smp_live |= (uint16_t)(1u << ch);
        if (c->key)
            key |= 1u << ch;
    }
    if (key >> 8)
        zxv_ay_event(t, (int)ZXV_SMP_KEYON_HI, (int)(key >> 8));
    if (key)
        zxv_ay_event(t, (int)ZXV_SMP_KEYON, (int)(key & 0xFF));
}

/* Las muestras de la canción, a la memoria de sonido y a su tabla (antes
 * de que suene). Espera a cada subida: unas pocas imágenes. */
static void load_samples(const zxm_t *z)
{
    uint32_t size = z->o[ZXM_O_DATA_SIZE], base = smp_base;
    if (!z->n_samples || base >= ZXV_SMP_MEM)
        return;
    if (size > ZXV_SMP_MEM - base)
        size = ZXV_SMP_MEM - base;
    if (size && z->o[ZXM_O_DATA] + size <= z->n)
        zxv_smp_upload(base, z->m + z->o[ZXM_O_DATA], (size + 3) & ~3u);
    uint32_t e[16 * 3];
    int k = 0, first = smp_slot;
    for (int i = 0; i < z->n_samples && smp_slot + i < ZXV_SMP_SLOTS; i++) {
        uint32_t p = z->o[ZXM_O_SAMPLES] + (uint32_t)i * ZXM_SMP_SIZE;
        const uint8_t *m = z->m;
        uint32_t start = (uint32_t)mus_w(m, z->n, p) | (uint32_t)mus_w(m, z->n, p + 2) << 16;
        uint32_t len = (uint32_t)mus_w(m, z->n, p + 4) | (uint32_t)mus_w(m, z->n, p + 6) << 16;
        uint32_t loop = (uint32_t)mus_w(m, z->n, p + 8) | (uint32_t)mus_w(m, z->n, p + 10) << 16;
        e[3 * k] = (base + start) | (uint32_t)mus_b(m, z->n, p + ZXM_S_FORMAT) << 24;
        e[3 * k + 1] = len;
        e[3 * k + 2] = loop;
        if (++k == 16 || i + 1 == z->n_samples) {
            zxv_smp_upload(ZXV_SMP_TABLE + (uint32_t)first * ZXV_SMP_SLOT_SIZE, e, (uint32_t)k * 12u);
            first += k;
            k = 0;
        }
    }
}

/* --- Los pasos -------------------------------------------------------------------- */

/* Un paso a la hora t: la canción si le toca (song) y los efectos si les
 * toca (sfx), encima de ella, y lo que cambia en los AY que se usan (y, en
 * el paso en que se dejan, su silencio). */
static void step(uint32_t t, int song, int sfx)
{
    /* Sin un .zxm, la canción y los efectos van siempre en el mismo paso, y
     * no hace falta guardar nada del anterior. */
    mus_rt_t *z = R();
    mus_ay_t ay[2], here[2];
    uint8_t how_here[6];
    mus_ay_t *last = z ? z->last_ay : here;
    uint8_t *how = z ? z->sfx_how : how_here;
    uint8_t own = S.playing ? S.song_ays : 0;
    silent(&ay[0]);
    silent(&ay[1]);
    if (song) {
        mus_song_step(&S.song, last);
        if (!S.loop && mus_song_looped(&S.song))
            S.playing = 0;                /* el paso que vuelve ya no suena */
        else
            S.steps++;
    }
    if (ticking()) {
        if (S.song.chips == 3) {
            ay[0] = last[0];
            ay[1] = last[1];
        } else if (S.song.chips) {
            ay[S.ay2] = last[0];
        }
    }
    if (z && (song || z->smp_live))
        emit_samples(z, t);
    last[0].r13 = last[1].r13 = 0;      /* R13, solo en el paso que lo escribe */
    for (int i = 0; i < 6; i++) {
        if (sfx)
            how[i] = (uint8_t)afx_step(&S.sfx[i]);
        if (how[i]) {
            overlay(&ay[i / 3], i % 3, &S.sfx[i], how[i]);
            own |= 1 << (i / 3);
        }
    }
    for (int a = 0; a < 2; a++)
        if ((own | S.owned) >> a & 1)
            emit(t, a ? ZXV_AY2 : 0, &ay[a], !(S.valid >> a & 1));
    S.valid = own;
    S.owned = own;
}

void zxv_music_update(void)
{
    zxv_music_update_until(zxv_time_us() + ZXV_MUSIC_AHEAD_US);
}

void zxv_music_update_until(uint32_t t_us)
{
    mus_rt_t *z = R();
    if (S.busy || (S.running && (int32_t)(S.next - t_us) >= 0 &&
                   (!z || !ticking() || (int32_t)(z->song_next - t_us) >= 0)))
        return;                         /* ya va hasta ahí (sin leer el reloj) */
    uint32_t now = zxv_time_us();
    if ((int32_t)(t_us - (now + ZXV_MUSIC_AHEAD_US)) > 0)
        t_us = now + ZXV_MUSIC_AHEAD_US;
    if (!S.running) {
        if (!active())
            return;
        S.running = 1;
        S.next = now;
        if (z)
            z->song_next = now;
    }
    /* Muy retrasado (el programa no ha llamado en mucho tiempo): no se
     * recupera de golpe, se sigue desde ahora. */
    if ((int32_t)(now - S.next) > (int32_t)(2 * ZXV_MUSIC_AHEAD_US)) {
        S.next = now;
        if (z)
            z->song_next = now;
    }
    for (;;) {
        /* El paso siguiente. Con un .zxm, el de los efectos o el de la
         * canción, el que llegue antes (o los dos a la vez); sin él, los dos
         * van juntos. */
        uint32_t t = S.next;
        int song = ticking(), sfx = 1;
        if (z && !ticking()) {
            z->song_next = S.next;      /* parada o en pausa: sigue con el otro reloj */
        } else if (z) {
            song = (int32_t)(z->song_next - S.next) <= 0;
            if (song && z->song_next != S.next) {
                t = z->song_next;
                sfx = 0;
            }
        }
        if ((int32_t)(t - t_us) >= 0)
            break;
        int was = active();
        step(t, song, sfx);
        if (song && z)
            z->song_next += mus_song_step_us(&S.song);
        if (sfx)
            S.next += STEP_US;
        if (!was) {                     /* el paso de silencio ya ha salido */
            S.running = 0;
            break;
        }
    }
}

/* Los canales de muestras de la canción que suenan, callados ya (por la
 * cola sin hora: la canción deja de ser esta). */
static void smp_silence(void)
{
    mus_rt_t *z = R();
    for (int ch = 0; z && ch < 12; ch++)
        if (z->smp_live >> ch & 1)
            zxv_ay_write((int)ZXV_SMP_REG(ch, ZXV_SMP_CTRL), ZXV_SMP_STOP);
    if (z)
        z->smp_live = 0;
}

int zxv_music_play(const void *data, uint32_t size, int flags)
{
    S.busy = 1;
    S.playing = 0;
    smp_silence();
    void *mem = SONG_MEM(data, size);
    S.song.zxm = mem;
    S.song.vgm = mem;
    S.song.work = SONG_WORK(mem);
    S.song.zxm_moves = SONG_MOVES;
    int format = mus_song_init(&S.song, data, size);
    S.song_data = data;
    if (format != ZXV_MUSIC_NONE) {
        if (format == ZXV_MUSIC_ZXM)
            load_samples(S.song.zxm);
        S.loop = flags & ZXV_MUSIC_LOOP ? 1 : 0;
        S.ay2 = S.song.chips == 1 && (flags & ZXV_MUSIC_AY2) ? 1 : 0;
        S.song_ays = S.song.chips == 3 ? 3 : S.song.chips ? 1 << S.ay2 : 0;
        S.steps = 0;
        S.paused = 0;
        if (R())
            R()->song_next = S.next;
        S.playing = 1;
    }
    S.busy = 0;
    return format;
}

/* Para la ROM, que no ve zxv_api.h: LOAD MUSIC y SAMPLE suben las
 * muestras al cargar. */
int mus_smp_upload(uint32_t dst, const void *src, uint32_t len)
{
    return zxv_smp_upload(dst, src, len);
}

void zxv_music_samples(uint32_t base, int slot)
{
    smp_base = base;
    smp_slot = (uint8_t)slot;
}

void zxv_music_work(void *buf)
{
#ifndef MUS_STATE_EXTERN
    work_ = buf;
#else
    (void)buf;
#endif
}

/* --- Las muestras como efectos (D99) ------------------------------------------------ */

/* Una escritura de un efecto: con la música en marcha, a su paso (con su
 * adelanto, por la cola con hora, que tiene que ir en orden); si no, ya. */
static void fx_reg(uint32_t t, int reg, int v)
{
    if (t)
        zxv_ay_event(t, reg, v);
    else
        zxv_ay_write(reg, v);
}

static uint32_t fx_time(void)
{
    return S.running ? zxv_time_us() + ZXV_MUSIC_AHEAD_US : 0;
}

void zxv_music_smp_fx(int ch, int slot, uint32_t step, int vol, int pan, uint32_t dur_us)
{
    if (ch < 0 || ch > 11)
        return;
    S.busy = 1;
    uint32_t t = fx_time();
    uint16_t bit = (uint16_t)(1u << ch);
    mus_rt_t *z = R();
    if (z) {
        z->fx_mask |= bit;
        z->smp_valid &= (uint16_t)~bit;
        if (dur_us) {
            z->fx_loop &= (uint16_t)~bit;
            z->fx_end[ch] = (t ? t : zxv_time_us()) + dur_us;
        } else {
            z->fx_loop |= bit;
        }
    }
    fx_reg(t, (int)ZXV_SMP_REG(ch, ZXV_SMP_SLOT), slot);
    fx_reg(t, (int)ZXV_SMP_REG(ch, ZXV_SMP_VOL), vol);
    fx_reg(t, (int)ZXV_SMP_REG(ch, ZXV_SMP_PAN), pan);
    for (int i = 0; i < 3; i++)
        fx_reg(t, (int)ZXV_SMP_REG(ch, ZXV_SMP_STEP + i), (int)(step >> 8 * i & 0xFF));
    fx_reg(t, (int)ZXV_SMP_REG(ch, ZXV_SMP_OFFSET), 0);
    fx_reg(t, (int)ZXV_SMP_REG(ch, ZXV_SMP_CTRL), 0);
    if (ch >= 8)
        fx_reg(t, (int)ZXV_SMP_KEYON_HI, 1 << (ch - 8));
    fx_reg(t, (int)ZXV_SMP_KEYON, ch < 8 ? 1 << ch : 0);
    S.busy = 0;
}

void zxv_music_smp_fx_stop(int ch)
{
    if (ch < 0 || ch > 11)
        return;
    S.busy = 1;
    fx_reg(fx_time(), (int)ZXV_SMP_REG(ch, ZXV_SMP_CTRL), ZXV_SMP_STOP);
    mus_rt_t *z = R();
    if (z && (z->fx_mask >> ch & 1)) {
        z->fx_mask &= (uint16_t)~(1u << ch);
        z->smp_valid &= (uint16_t)~(1u << ch);
    }
    S.busy = 0;
}

int zxv_music_smp_tracks(void)
{
    return ticking() && Z() ? Z()->smp_tracks : 0;
}

void zxv_music_stop(void)
{
    S.playing = 0;
}

void zxv_music_pause(int on)
{
    S.paused = on ? 1 : 0;
    if (R())
        R()->smp_valid = 0;             /* al seguir, todo otra vez */
}

int zxv_music_playing(void)
{
    return S.playing;
}

uint32_t zxv_music_steps(void)
{
    return S.steps;
}

void zxv_music_moved(const void *song, const void *bank)
{
    S.busy = 1;
    if (S.song_data && song) {
        mus_song_move(&S.song, (const uint8_t *)song - S.song_data);
        S.song_data = song;
    }
    if (S.bank && bank) {
        intptr_t d = (const uint8_t *)bank - S.bank;
        for (int i = 0; i < 6; i++)
            if (S.sfx[i].p && S.sfx[i].p >= S.bank && S.sfx[i].end <= S.bank + S.bank_size) {
                S.sfx[i].p += d;
                S.sfx[i].end += d;
            }
        S.bank = bank;
    }
    S.busy = 0;
}

/* --- Efectos ----------------------------------------------------------------- */

int zxv_sfx_bank(const void *data, uint32_t size)
{
    int n = 0;
    S.bank = data;
    S.bank_size = size;
    afx_find(S.bank, size, 0, &n);
    return n;
}

static int pick(int channel)
{
    if (channel >= 0)
        return channel < 6 ? channel : -1;
    int mask = S.sfx_mask;
    if (mask < 0)
        mask = S.playing && S.song_ays != 3 ? 7 << 3 * (S.song_ays == 1) : 0x3F;
    int best = -1;
    uint32_t age = 0;
    for (int i = 0; i < 6; i++) {
        if (!(mask >> i & 1))
            continue;
        if (!S.sfx[i].p)
            return i;
        if (best < 0 || S.sfx[i].age >= age) {
            best = i;
            age = S.sfx[i].age;
        }
    }
    return best;
}

static int start(const uint8_t *p, const uint8_t *end, int channel)
{
    int ch = pick(channel);
    if (!p || ch < 0)
        return -1;
    S.busy = 1;
    afx_start(&S.sfx[ch], p, end);
    S.busy = 0;
    return ch;
}

int zxv_sfx_play(int n, int channel)
{
    return start(afx_find(S.bank, S.bank_size, n, NULL), S.bank + S.bank_size, channel);
}

int zxv_sfx_play_data(const void *data, uint32_t size, int channel)
{
    const uint8_t *p = data;
    return start(size ? p : NULL, p + size, channel);
}

void zxv_sfx_stop(int channel)
{
    for (int i = 0; i < 6; i++)
        if (channel < 0 || channel == i)
            S.sfx[i].p = NULL;
}

void zxv_sfx_channels(int mask)
{
    S.sfx_mask = mask & 0x3F;
}

int zxv_sfx_playing(int channel)
{
    for (int i = 0; i < 6; i++)
        if ((channel < 0 || channel == i) && S.sfx[i].p)
            return 1;
    return 0;
}
