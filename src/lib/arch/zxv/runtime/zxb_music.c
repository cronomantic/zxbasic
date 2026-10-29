/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_music.c - The music and the sound effects of the trackers (zxv.bas:
 * MUSICLOAD, MUSICPLAY, SFXLOAD, SFX...), with the music library of the ZX-V
 * SDK (sdk/music, zxv_music.h), as the ZX-V BASIC does with MUSIC and SFX.
 *
 * It plays from the frame hook of zxb_screen.c while the program goes on.
 * The module and the bank go in the heap, which does not move them. While
 * something plays, BEEP goes as far ahead as the music (zxb_beeper_lead), so
 * the timed queue keeps its order.
 */
#include "zxb.h"
#include "zxv_music.h"

#define ERR_TAPE    26                  /* R Tape loading error */
#define ERR_RANGE   10                  /* B Integer out of range */
#define NAME_MAX    64

void zxb_ERROR(int code);
void zxb_fail(const char *msg);
extern void (*zxb_frame_music)(void);   /* zxb_screen.c */
extern volatile uint32_t zxb_beeper_lead;  /* zxb_misc.c */

static uint8_t *song, *bank;
static uint32_t song_n, bank_n;
static uint8_t paused;

static void music_frame(void)
{
    zxv_music_update();
    zxb_beeper_lead = zxv_music_playing() || zxv_sfx_playing(ZXV_SFX_AUTO) ? ZXV_MUSIC_AHEAD_US : 0;
}

/* The whole file s (without an extension, with ext) in the heap; its
 * length in *n. With sfx, an effect alone (.afx) gets the header of a bank
 * of one in front. */
static uint8_t *load(zstr s, const char *ext, int sfx, uint32_t *n)
{
    uint32_t len = zstr_len(s);
    char name[NAME_MAX + 5];
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
    if (dot < 0) {
        name[len] = '.';
        for (int i = 0; i < 4; i++)
            name[len + 1 + i] = ext[i];
    }
    int afx = sfx && dot >= 0 && len - (uint32_t)dot == 4 && (name[dot + 1] | 0x20) == 'a' &&
              (name[dot + 2] | 0x20) == 'f' && (name[dot + 3] | 0x20) == 'x';
    uint32_t pre = afx ? 3 : 0;
    int f = zxv_file_open(name, ZXV_O_READ);
    if (f < 0)
        zxb_fail(f == ZXV_E_NOENT ? "T File not found" : f == ZXV_E_NOCARD ? "U No SD card" : "V Disk error");
    int size = zxv_file_seek(f, 0, ZXV_SEEK_END);
    if (size <= 0 || zxv_file_seek(f, 0, ZXV_SEEK_SET) != 0) {
        zxv_file_close(f);
        zxb_ERROR(ERR_TAPE);
    }
    uint8_t *p = zxb_try_alloc((uint32_t)size + pre);
    if (!p) {
        zxv_file_close(f);
        zxb_ERROR(ERR_OUT_OF_MEMORY);
    }
    uint32_t got = 0;
    while (got < (uint32_t)size) {
        int r = zxv_file_read(f, p + pre + got, (uint32_t)size - got);
        if (r <= 0) {
            zxv_file_close(f);
            zxb_free(p);
            zxb_ERROR(ERR_TAPE);
        }
        got += (uint32_t)r;
    }
    zxv_file_close(f);
    if (afx) {
        p[0] = 1;                       /* one effect, after these 3 bytes */
        p[1] = 1;
        p[2] = 0;
    }
    *n = (uint32_t)size + pre;
    return p;
}

/* MUSICLOAD f$: a module (PT3, TurboSound, PT2, STC, ASC or PSG) */
void zxb_music_load(zstr s)
{
    zxv_music_stop();
    paused = 0;
    if (song)
        zxb_free(song);
    song = 0;
    uint32_t n;
    uint8_t *p = load(s, "pt3", 0, &n);
    zxv_music_info_t info;
    if (zxv_music_info(p, n, &info) == ZXV_MUSIC_NONE) {
        zxb_free(p);
        zxb_ERROR(ERR_TAPE);
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
    zxv_music_play(song, song_n, ZXV_MUSIC_LOOP);
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

/* SFXLOAD f$: an AYFX bank (.afb) or a single effect (.afx, as a bank of one) */
void zxb_sfx_load(zstr s)
{
    zxv_sfx_stop(ZXV_SFX_AUTO);
    if (bank)
        zxb_free(bank);
    bank = 0;
    uint32_t n;
    uint8_t *p = load(s, "afb", 1, &n);
    if (zxv_sfx_bank(p, n) <= 0) {
        zxb_free(p);
        zxb_ERROR(ERR_TAPE);
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
