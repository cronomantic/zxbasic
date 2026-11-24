/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_embed.c - The files the program carries inside (zxbc --embed FILE),
 * as the C games of ZX-V carry theirs (ZX-V decision D82).
 *
 * What reads a file (LOAD, VERIFY, MUSICLOAD, SFXLOAD, SAMPLELOAD) opens it with
 * zxb_rd_open: the file in the current folder if there is one, so a file on
 * the card takes the place of the one inside, and if not (it is not there,
 * or there is no card) the embedded file with the same name. The name is
 * compared without its folder or drive and without telling capitals apart:
 * "music/song.pt3" finds an embedded "song.pt3". That is what makes a .zxv
 * work alone, from the ROM: drive too, where the current folder is the card.
 *
 * zxb_rd_read, zxb_rd_seek and zxb_rd_close take both kinds of handle: those
 * of the ROM (0 or more) and those of the embedded files (EMBED + slot).
 *
 * The compiler makes the table, zxb_embed_table: the number of files and,
 * for each one, its name, its bytes and its length. Without --embed this
 * empty one (weak) is the one linked.
 */
#include "zxb.h"

typedef struct {
    const char *name;
    const uint8_t *data;
    uint32_t size;
} embed_t;

typedef struct {
    uint32_t count;
    embed_t files[];
} embed_table_t;

__attribute__((weak)) const embed_table_t zxb_embed_table = {0};

#define EMBED   0x40000000              /* Handles of the embedded files */
#define SLOTS   4

static struct {
    const embed_t *e;                   /* NULL: free */
    uint32_t pos;
} slot[SLOTS];

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* The embedded file called like name, without its folder; NULL if none. */
static const embed_t *find(const char *name)
{
    const char *base = name;
    for (const char *p = name; *p; p++)
        if (*p == '/' || *p == ':')
            base = p + 1;
    for (uint32_t i = 0; i < zxb_embed_table.count; i++) {
        const char *a = zxb_embed_table.files[i].name, *b = base;
        while (*a && lower(*a) == lower(*b))
            a++, b++;
        if (!*a && !*b)
            return &zxb_embed_table.files[i];
    }
    return 0;
}

int zxb_rd_open(const char *name)
{
    int f = zxv_file_open(name, ZXV_O_READ);
    if (f >= 0 || (f != ZXV_E_NOENT && f != ZXV_E_NOCARD))
        return f;
    const embed_t *e = find(name);
    if (!e)
        return f;
    for (int i = 0; i < SLOTS; i++) {
        if (!slot[i].e) {
            slot[i].e = e;
            slot[i].pos = 0;
            return EMBED + i;
        }
    }
    return ZXV_E_MFILE;
}

static int embedded(int f) { return f >= EMBED && f < EMBED + SLOTS && slot[f - EMBED].e; }

int zxb_rd_read(int f, void *buf, uint32_t n)
{
    if (!embedded(f))
        return zxv_file_read(f, buf, n);
    const embed_t *e = slot[f - EMBED].e;
    uint32_t pos = slot[f - EMBED].pos;
    if (n > e->size - pos)
        n = e->size - pos;
    for (uint32_t i = 0; i < n; i++)
        ((uint8_t *)buf)[i] = e->data[pos + i];
    slot[f - EMBED].pos = pos + n;
    return (int)n;
}

int zxb_rd_seek(int f, int32_t offset, int whence)
{
    if (!embedded(f))
        return zxv_file_seek(f, offset, whence);
    const embed_t *e = slot[f - EMBED].e;
    int64_t base = whence == ZXV_SEEK_SET ? 0 : whence == ZXV_SEEK_CUR ? slot[f - EMBED].pos : e->size;
    int64_t pos = base + offset;
    if (pos < 0 || pos > e->size)
        return ZXV_E_INVAL;
    slot[f - EMBED].pos = (uint32_t)pos;
    return (int)pos;
}

int zxb_rd_close(int f)
{
    if (!embedded(f))
        return zxv_file_close(f);
    slot[f - EMBED].e = 0;
    return 0;
}
