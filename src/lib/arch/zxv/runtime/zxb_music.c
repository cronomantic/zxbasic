/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_music.c - The music, the sound effects of the trackers and the
 * recorded sounds (zxv.bas: MUSICLOAD, MUSICPLAY, SFXLOAD, SFX, SAMPLELOAD,
 * SAMPLE...), with the music library of the ZX-V SDK (sdk/music,
 * zxv_music.h), as the ZX-V BASIC does with MUSIC, SFX and SAMPLE.
 *
 * It plays from the frame hook of zxb_screen.c while the program goes on.
 * The module and the bank go in the heap, which does not move them. While
 * something plays, BEEP goes as far ahead as the music (zxb_beeper_lead), so
 * the timed queue keeps its order.
 *
 * The sound memory of the sample channels: the samples of a .zxm from the
 * bottom (with its table entries from 0), the SAMPLE sounds from the top
 * (sound n in entry 224 + n), as in the ZX-V BASIC (its D99).
 */
#include "zxb.h"
#include "zxv_music.h"
#include "zxm_format.h"

#define ERR_LOADING 26                  /* R Loading error */
#define ERR_RANGE   10                  /* B Integer out of range */
#define NAME_MAX    64

void zxb_ERROR(int code);
void zxb_fail(const char *msg);
extern void (*zxb_frame_music)(void);   /* zxb_screen.c */
extern volatile uint32_t zxb_beeper_lead;  /* zxb_misc.c */

static uint8_t *song, *bank, *work;
static uint32_t song_n, bank_n;
static uint8_t paused;

/* --- The SAMPLE sounds ------------------------------------------------------- */

#define FX_N        32
#define FX_SLOT     224

typedef struct {
    uint32_t addr, frames, loop, rate;  /* frames 0: not loaded; loop = frames: no loop */
} fx_smp_t;

typedef struct {
    uint32_t start, end;                /* when it started and ends (with a loop: until stopped) */
    uint8_t on, loop;
} fx_ch_t;

static fx_smp_t fx_s[FX_N];
static fx_ch_t fx_ch[12];
static uint32_t fx_floor = ZXV_SMP_MEM; /* where the SAMPLE sounds begin */

static int fx_busy(const fx_ch_t *c, uint32_t now)
{
    return c->on && (c->loop || (int32_t)(now - c->end) < 0);
}

int zxb_sample_on(void);

static void music_frame(void)
{
    zxv_music_update();
    zxb_beeper_lead =
        zxv_music_playing() || zxv_sfx_playing(ZXV_SFX_AUTO) || zxb_sample_on() ? ZXV_MUSIC_AHEAD_US : 0;
}

static uint32_t get32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static int is_id(const uint8_t *p, const char *id)
{
    return p[0] == (uint8_t)id[0] && p[1] == (uint8_t)id[1] && p[2] == (uint8_t)id[2] && p[3] == (uint8_t)id[3];
}

/* The samples of a .zxm (m, n bytes): how many, and in *top up to where
 * they go in the sound memory. 0 if it is not a .zxm or has none. */
static uint32_t song_samples(const uint8_t *m, uint32_t n, uint32_t *top)
{
    *top = 0;
    if (!m || n < ZXM_H_OFFSETS + 32 || !is_id(m, "ZXVM") || !is_id(m + 4, "US01"))
        return 0;
    uint32_t ns = m[ZXM_H_SAMPLES], at = get32(m + ZXM_H_OFFSETS + 4 * ZXM_O_SAMPLES);
    for (uint32_t i = 0; i < ns; i++) {
        uint32_t a = at + i * ZXM_SMP_SIZE;
        if (a + ZXM_SMP_SIZE > n)
            break;
        uint32_t len = get32(m + a + ZXM_S_LEN);
        uint32_t bytes = m[a + ZXM_S_FORMAT] ? (len + 63) / 64 * 36 : len;
        uint32_t end = get32(m + a + ZXM_S_START) + ((bytes + 3) & ~3u);
        if (end > *top)
            *top = end;
    }
    return ns;
}

/* --- Loading ------------------------------------------------------------------ */

/* The name s in name; without an extension, the first of exts (each of 3
 * letters and its zero, one after the other, ending with an empty one) that
 * exists, or the first one. Gives where the dot is. */
static int file_name(zstr s, const char *exts, char *name)
{
    uint32_t len = zstr_len(s);
    if (len == 0 || len >= NAME_MAX)
        zxb_ERROR(14);                  /* F Invalid file name */
    int dot = -1;
    for (uint32_t i = 0; i < len; i++) {
        name[i] = (char)s[2 + i];
        if (name[i] == '.')
            dot = (int)i;
        else if (name[i] == '/' || name[i] == ':')
            dot = -1;
    }
    name[len] = 0;
    if (dot >= 0)
        return dot;
    for (const char *e = exts; *e; e += 4) {
        name[len] = '.';
        for (int i = 0; i < 4; i++)
            name[len + 1 + i] = e[i];
        int f = zxb_rd_open(name);
        if (f >= 0) {
            zxb_rd_close(f);
            return (int)len;
        }
    }
    name[len] = '.';                    /* none: the first, for the report */
    for (int i = 0; i < 4; i++)
        name[len + 1 + i] = exts[i];
    return (int)len;
}

static int open_file(const char *name)
{
    int f = zxb_rd_open(name);
    if (f < 0)
        zxb_fail(f == ZXV_E_NOENT ? "T File not found" : f == ZXV_E_NOCARD ? "U No SD card" : "V Disk error");
    return f;
}

/* The whole file s in the heap; its length in *n. With sfx, an effect
 * alone (.afx) gets the header of a bank of one in front. */
static uint8_t *load(zstr s, const char *exts, int sfx, uint32_t *n)
{
    char name[NAME_MAX + 5];
    int dot = file_name(s, exts, name);
    uint32_t len = 0;
    while (name[len])
        len++;
    int afx = sfx && len - (uint32_t)dot == 4 && (name[dot + 1] | 0x20) == 'a' &&
              (name[dot + 2] | 0x20) == 'f' && (name[dot + 3] | 0x20) == 'x';
    uint32_t pre = afx ? 3 : 0;
    int f = open_file(name);
    int size = zxb_rd_seek(f, 0, ZXV_SEEK_END);
    if (size <= 0 || zxb_rd_seek(f, 0, ZXV_SEEK_SET) != 0) {
        zxb_rd_close(f);
        zxb_ERROR(ERR_LOADING);
    }
    uint8_t *p = zxb_try_alloc((uint32_t)size + pre);
    if (!p) {
        zxb_rd_close(f);
        zxb_ERROR(ERR_OUT_OF_MEMORY);
    }
    uint32_t got = 0;
    while (got < (uint32_t)size) {
        int r = zxb_rd_read(f, p + pre + got, (uint32_t)size - got);
        if (r <= 0) {
            zxb_rd_close(f);
            zxb_free(p);
            zxb_ERROR(ERR_LOADING);
        }
        got += (uint32_t)r;
    }
    zxb_rd_close(f);
    if (afx) {
        p[0] = 1;                       /* one effect, after these 3 bytes */
        p[1] = 1;
        p[2] = 0;
    }
    *n = (uint32_t)size + pre;
    return p;
}

/* --- The music ---------------------------------------------------------------- */

/* MUSICLOAD f$: a module (.zxm, PT3, TurboSound, PT2, STC, ASC, PSG or VGM,
 * also compressed: .vgz). Without an extension, the first there is of
 * f$.zxm, f$.pt3, f$.vgm and f$.vgz. */
void zxb_music_load(zstr s)
{
    zxv_music_stop();
    paused = 0;
    if (song)
        zxb_free(song);
    song = 0;
    uint32_t n, top;
    uint8_t *p = load(s, "zxm\0pt3\0vgm\0vgz\0", 0, &n);
    zxv_music_info_t info;
    if (zxv_music_info(p, n, &info) == ZXV_MUSIC_NONE) {
        zxb_free(p);
        zxb_ERROR(ERR_LOADING);
    }
    /* Its samples, below those of SAMPLE (and in table entries below 224
     * if there are any). */
    uint32_t ns = song_samples(p, n, &top);
    if (top > fx_floor || (fx_floor < ZXV_SMP_MEM && ns > FX_SLOT)) {
        zxb_free(p);
        zxb_ERROR(ERR_OUT_OF_MEMORY);
    }
    /* A .vgz: 32 KB to decompress it while it plays (once, for good). */
    if (n >= 2 && p[0] == 0x1F && p[1] == 0x8B && !work) {
        work = zxb_try_alloc(32768);
        if (!work) {
            zxb_free(p);
            zxb_ERROR(ERR_OUT_OF_MEMORY);
        }
        zxv_music_work(work);
    }
    song = p;
    song_n = n;
    zxb_frame_music = music_frame;
}

/* MUSICPLAY: from the start, and back to its loop position at the end */
void zxb_music_play(void)
{
    if (!song)
        zxb_ERROR(ERR_INVALID_ARG);
    paused = 0;
    if (zxv_music_play(song, song_n, ZXV_MUSIC_LOOP) == ZXV_MUSIC_NONE)
        zxb_ERROR(ERR_LOADING);         /* a .gz that is not a VGM with an AY */
}

void zxb_music_stop(void)
{
    paused = 0;
    zxv_music_stop();
}

void zxb_music_pause(int on)
{
    paused = on != 0;
    zxv_music_pause(on);
}

/* MUSIC: 1 while it plays */
int zxb_music_on(void) { return zxv_music_playing() && !paused; }

/* --- The effects -------------------------------------------------------------- */

/* SFXLOAD f$: an AYFX bank (.afb) or a single effect (.afx, as a bank of one) */
void zxb_sfx_load(zstr s)
{
    zxv_sfx_stop(ZXV_SFX_AUTO);
    if (bank)
        zxb_free(bank);
    bank = 0;
    uint32_t n;
    uint8_t *p = load(s, "afb\0", 1, &n);
    if (zxv_sfx_bank(p, n) <= 0) {
        zxb_free(p);
        zxb_ERROR(ERR_LOADING);
    }
    bank = p;
    bank_n = n;
    zxb_frame_music = music_frame;
}

/* SFX n[, channel]: effect n of the bank, on a free channel (channel -1) or
 * on that one (0-5) */
void zxb_sfx(int n, int channel)
{
    n &= 0xFF;                          /* the UByte and the Byte of zxv.bas */
    channel = (int8_t)channel;
    if (!bank)
        zxb_ERROR(ERR_INVALID_ARG);
    if (channel < -1 || channel > 5 || n < 0 || zxv_sfx_play(n, channel) < 0)
        zxb_ERROR(ERR_RANGE);
}

void zxb_sfx_stop(void) { zxv_sfx_stop(ZXV_SFX_AUTO); }
int zxb_sfx_on(void) { return zxv_sfx_playing(ZXV_SFX_AUTO); }

/* --- The recorded sounds (SAMPLE) --------------------------------------------- */

static void fx_stop(int ch)
{
    if (fx_ch[ch].on) {
        zxv_music_smp_fx_stop(ch);
        fx_ch[ch].on = 0;
    }
}

static void fail_close(int f, int code)
{
    zxb_rd_close(f);
    zxb_ERROR(code);
}

/* n bytes of the file from at: how many (fewer at its end), or -1. */
static int read_at(int f, uint32_t at, uint8_t *buf, uint32_t n)
{
    if (zxb_rd_seek(f, (int32_t)at, ZXV_SEEK_SET) != (int)at)
        return -1;
    uint32_t got = 0;
    while (got < n) {
        int r = zxb_rd_read(f, buf + got, n - got);
        if (r < 0)
            return -1;
        if (!r)
            break;
        got += (uint32_t)r;
    }
    return (int)got;
}

#define WAV_BUF 4096u                   /* bytes of the file at a time */

/* SAMPLELOAD f$, n: a WAV (without an extension, f$.wav) as sound n (0-31):
 * 8 or 16 bits, mono or stereo (mixed), at any rate, to signed 8 bits in the
 * sound memory, in pieces (nothing stays in the RAM); with its loop if it
 * has a smpl chunk. */
void zxb_sample_load(zstr s, int n)
{
    n &= 0xFF;
    if (n >= FX_N)
        zxb_ERROR(ERR_RANGE);
    char name[NAME_MAX + 5];
    file_name(s, "wav\0", name);
    int f = open_file(name);
    int size = zxb_rd_seek(f, 0, ZXV_SEEK_END);
    uint8_t h[64];
    if (size < 12 || read_at(f, 0, h, 12) != 12 || !is_id(h, "RIFF") || !is_id(h + 8, "WAVE"))
        fail_close(f, ERR_LOADING);
    uint32_t pos = 12, data_at = 0, data_len = 0, ls = 0, le = 0, rate = 0;
    int fmt = 0, chans = 0, bits = 0, has_loop = 0;
    while (pos + 8 <= (uint32_t)size && read_at(f, pos, h, 8) == 8) {
        uint32_t len = get32(h + 4);
        if (is_id(h, "fmt ") && len >= 16 && read_at(f, pos + 8, h, len < 40 ? len : 40) >= 16) {
            fmt = get16(h);
            if (fmt == 0xFFFE && len >= 26)     /* WAVE_FORMAT_EXTENSIBLE: its subformat */
                fmt = get16(h + 24);
            chans = get16(h + 2);
            rate = get32(h + 4);
            bits = get16(h + 14);
        } else if (is_id(h, "data")) {
            data_at = pos + 8;
            data_len = len;
        } else if (is_id(h, "smpl") && len >= 60 && read_at(f, pos + 8, h, 60) == 60 && get32(h + 28)) {
            ls = get32(h + 36 + 8);             /* the first loop: from and to (included) */
            le = get32(h + 36 + 12);
            has_loop = 1;
        }
        if (len > (uint32_t)size)
            break;
        pos += 8 + len + (len & 1);
    }
    if (fmt != 1 || chans < 1 || chans > 2 || (bits != 8 && bits != 16) || !rate || rate > 1000000 || !data_at)
        fail_close(f, ERR_LOADING);
    uint32_t fsz = (uint32_t)(chans * bits / 8), frames = data_len / fsz;
    if (data_at + frames * fsz > (uint32_t)size)
        frames = ((uint32_t)size - data_at) / fsz;
    uint32_t loop = frames;
    if (has_loop && ls < frames && le >= ls) {
        if (le + 1 < frames)
            frames = le + 1;
        loop = ls;
    }
    if (!frames)
        fail_close(f, ERR_LOADING);

    /* Where: in its place if it fits there; if not, below the others, above
     * the samples of the song. */
    uint32_t bytes = (frames + 3) & ~3u, top, addr;
    if (song_samples(song, song_n, &top) > FX_SLOT)
        fail_close(f, ERR_OUT_OF_MEMORY);
    for (int c = 0; c < 12; c++)
        fx_stop(c);
    if (fx_s[n].frames && ((fx_s[n].frames + 3) & ~3u) >= bytes)
        addr = fx_s[n].addr;
    else if (bytes <= fx_floor && fx_floor - bytes >= top)
        addr = fx_floor - bytes;
    else
        fail_close(f, ERR_OUT_OF_MEMORY);
    uint8_t *buf = zxb_try_alloc(WAV_BUF);
    if (!buf)
        fail_close(f, ERR_OUT_OF_MEMORY);
    fx_s[n].frames = 0;                 /* half done: not there */

    uint32_t chunk = WAV_BUF / fsz / 4 * 4;     /* samples at a time: a multiple of 4 */
    for (uint32_t done = 0; done < frames;) {
        uint32_t m = frames - done < chunk ? frames - done : chunk;
        int r = read_at(f, data_at + done * fsz, buf, m * fsz);
        if (r < 0) {
            zxb_free(buf);
            fail_close(f, ERR_LOADING);
        }
        for (uint32_t i = (uint32_t)r; i < m * fsz; i++)
            buf[i] = bits == 8 ? 128 : 0;
        for (uint32_t i = 0; i < m; i++) {  /* in place: the output does not overtake the input */
            const uint8_t *q = buf + i * fsz;
            int32_t v;
            if (bits == 8)
                v = chans == 2 ? ((int)q[0] + q[1] - 256) * 128 : ((int)q[0] - 128) * 256;
            else
                v = chans == 2 ? ((int16_t)get16(q) + (int16_t)get16(q + 2)) / 2 : (int16_t)get16(q);
            v = (v + 128) >> 8;
            buf[i] = (uint8_t)(int8_t)(v > 127 ? 127 : v < -128 ? -128 : v);
        }
        uint32_t up = (m + 3) & ~3u;
        for (uint32_t i = m; i < up; i++)
            buf[i] = 0;
        if (zxv_smp_upload(addr + done, buf, up)) {
            zxb_free(buf);
            fail_close(f, ERR_OUT_OF_MEMORY);
        }
        done += m;
    }
    zxb_free(buf);
    zxb_rd_close(f);
    uint32_t e[3] = { addr, frames, loop };         /* signed 8 bits (format 0) */
    if (zxv_smp_upload(ZXV_SMP_TABLE + (uint32_t)(FX_SLOT + n) * ZXV_SMP_SLOT_SIZE, e, sizeof e))
        zxb_ERROR(ERR_OUT_OF_MEMORY);
    if (addr < fx_floor)
        fx_floor = addr;
    fx_s[n].addr = addr;
    fx_s[n].frames = frames;
    fx_s[n].loop = loop;
    fx_s[n].rate = rate;
    zxb_frame_music = music_frame;
}

/* The channel for a sound: one the song and the other sounds do not use
 * (from the top down); if there is none, the one that has had one longest. */
static int fx_pick(uint32_t now)
{
    int tracks = zxv_music_smp_tracks(), old = -1;
    for (int c = 11; c >= tracks; c--)
        if (!fx_busy(&fx_ch[c], now))
            return c;
    for (int c = 0; c < 12; c++)
        if (fx_ch[c].on && (c >= tracks || tracks >= 12) &&
            (old < 0 || (int32_t)(fx_ch[c].start - fx_ch[old].start) < 0))
            old = c;
    return old >= 0 ? old : 11;
}

/* SAMPLE n[, tone, vol, pan, c]: sound n, at its pitch times mul / 65536
 * (zxv.bas gives 2 ^ (tone / 12)), with its volume (0-64) and pan (0-255),
 * on a free channel (c -1) or on channel c (0-11), without waiting. */
void zxb_sample(int n, uint32_t mul, int vol, int pan, int ch)
{
    n &= 0xFF;
    vol &= 0xFF;
    pan &= 0xFF;
    ch = (int8_t)ch;
    if (n >= FX_N || vol > 64 || ch < -1 || ch > 11)
        zxb_ERROR(ERR_RANGE);
    const fx_smp_t *s = &fx_s[n];
    if (!s->frames)
        zxb_ERROR(ERR_INVALID_ARG);
    uint64_t st = ((uint64_t)s->rate * mul + 24000u) / 48000u;     /* 8.16: rate x mul / 65536 x 65536 / 48000 */
    if (st < 1 || st > 16777215u)
        zxb_ERROR(ERR_RANGE);
    uint32_t dur = 0;                   /* with a loop: until it is stopped */
    if (s->loop >= s->frames) {
        uint64_t us = (uint64_t)s->frames * 1000000u * 65536u / ((uint64_t)s->rate * mul) + 1u;
        dur = us > 2000000000u ? 2000000000u : (uint32_t)us;
    }
    uint32_t now = zxv_time_us();
    if (ch < 0)
        ch = fx_pick(now);
    fx_ch[ch].start = now;
    fx_ch[ch].end = now + ZXV_MUSIC_AHEAD_US + dur;    /* with the music, it starts with its lead */
    fx_ch[ch].on = 1;
    fx_ch[ch].loop = dur == 0;
    zxb_frame_music = music_frame;
    zxv_music_smp_fx(ch, FX_SLOT + n, (uint32_t)st, vol, pan, dur);
}

/* SAMPLESTOP [c]: the sound of channel c, or all (c -1) */
void zxb_sample_stop(int ch)
{
    ch = (int8_t)ch;
    if (ch > 11 || ch < -1)
        zxb_ERROR(ERR_RANGE);
    for (int c = 0; c < 12; c++)
        if (ch < 0 || c == ch)
            fx_stop(c);
}

/* SAMPLEPLAYING: 1 while any sound of SAMPLE plays */
int zxb_sample_on(void)
{
    uint32_t now = zxv_time_us();
    for (int c = 0; c < 12; c++)
        if (fx_busy(&fx_ch[c], now))
            return 1;
    return 0;
}
