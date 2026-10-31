/*
 * music.c - La música y los efectos en la máquina: cada paso de 1/50 s se
 * calcula por delante y sus registros van al AY a su hora exacta con
 * zxv_ay_event (spec §5.4), solo los que cambian.
 */
#include <stddef.h>

#include "music_int.h"
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

static int active(void)
{
    if (S.playing && !S.paused)
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

/* Un paso: la música, los efectos encima y lo que cambia, a la hora t, en
 * los AY que se usan (y, en el paso en que se dejan, su silencio). */
static void step(uint32_t t)
{
    mus_ay_t ay[2], m[2];
    uint8_t own = S.playing ? S.song_ays : 0;
    silent(&ay[0]);
    silent(&ay[1]);
    if (S.playing && !S.paused) {
        mus_song_step(&S.song, m);
        if (!S.loop && mus_song_looped(&S.song)) {
            S.playing = 0;                /* el paso que vuelve ya no suena */
        } else {
            if (S.song.chips == 3) {
                ay[0] = m[0];
                ay[1] = m[1];
            } else {
                ay[S.ay2] = m[0];
            }
            S.steps++;
        }
    }
    for (int i = 0; i < 6; i++) {
        int how = afx_step(&S.sfx[i]);
        if (how) {
            overlay(&ay[i / 3], i % 3, &S.sfx[i], how);
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
    if (S.busy || (S.running && (int32_t)(S.next - t_us) >= 0))
        return;                         /* ya va hasta ahí (sin leer el reloj) */
    uint32_t now = zxv_time_us();
    if ((int32_t)(t_us - (now + ZXV_MUSIC_AHEAD_US)) > 0)
        t_us = now + ZXV_MUSIC_AHEAD_US;
    if (!S.running) {
        if (!active())
            return;
        S.running = 1;
        S.next = now;
    }
    /* Muy retrasado (el programa no ha llamado en mucho tiempo): no se
     * recupera de golpe, se sigue desde ahora. */
    if ((int32_t)(now - S.next) > (int32_t)(2 * ZXV_MUSIC_AHEAD_US))
        S.next = now;
    while ((int32_t)(S.next - t_us) < 0) {
        int was = active();
        step(S.next);
        S.next += STEP_US;
        if (!was) {                     /* el paso de silencio ya ha salido */
            S.running = 0;
            break;
        }
    }
}

int zxv_music_play(const void *data, uint32_t size, int flags)
{
    S.busy = 1;
    S.playing = 0;
    int format = mus_song_init(&S.song, data, size);
    S.song_data = data;
    if (format != ZXV_MUSIC_NONE) {
        S.loop = flags & ZXV_MUSIC_LOOP ? 1 : 0;
        S.ay2 = S.song.chips == 1 && (flags & ZXV_MUSIC_AY2) ? 1 : 0;
        S.song_ays = S.song.chips == 3 ? 3 : 1 << S.ay2;
        S.steps = 0;
        S.paused = 0;
        S.playing = 1;
    }
    S.busy = 0;
    return format;
}

void zxv_music_stop(void)
{
    S.playing = 0;
}

void zxv_music_pause(int on)
{
    S.paused = on ? 1 : 0;
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
