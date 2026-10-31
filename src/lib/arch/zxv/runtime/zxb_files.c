/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_files.c - SAVE, LOAD and VERIFY on the SD card. ZX Basic turns CODE,
 * SCREEN$ and DATA into a block of memory (an address and a length); they
 * are written as the ZX-V BASIC does (its decision D88): the bytes as they
 * are, and a name without extension gets .scr for the screen (the block
 * 16384, 6912) and .bin for the rest. LOAD without extension tries that one
 * and then .tap. A .tap is a Spectrum tape (a CODE header and the data), for
 * Spectrum emulators and to load it back where it was saved; its header has
 * 16 bits for the address: above 64K, its second parameter (32768 for CODE
 * in the Spectrum) is the high half.
 */
#include "zxb.h"

#define ERR_TAPE    26                  /* R Tape loading error */
#define ERR_RANGE   10                  /* B Integer out of range */
#define NAME_MAX    64

void zxb_ERROR(int code);
void zxb_fail(const char *msg);

/* The C name of the file, from the BASIC string (freed), and the name of a
 * tape header: without the folder or the extension, as in the ZX-V BASIC.
 * Returns 1 if the name has no extension. */
static int file_name(zstr s, char name[NAME_MAX + 8], char tname[10])
{
    uint32_t n = zstr_len(s);
    if (n == 0 || n >= NAME_MAX) {
        zxb_free(s);
        zxb_ERROR(14);                  /* F Invalid file name */
    }
    uint32_t from = 0, to = n;
    for (uint32_t i = 0; i < n; i++) {
        name[i] = (char)s[2 + i];
        if (name[i] == '.')
            to = i;
        else if (name[i] == '/' || name[i] == ':')
            from = i + 1, to = n;
    }
    for (uint32_t i = 0; i < 10; i++)
        tname[i] = from + i < to ? (char)s[2 + from + i] : ' ';
    zxb_free(s);
    name[n] = 0;
    return to == n;
}

/* Adds .ext (three letters) to a name without extension */
static void add_ext(char *name, const char *ext)
{
    uint32_t n = 0;
    while (name[n])
        n++;
    name[n] = '.', name[n + 1] = ext[0], name[n + 2] = ext[1], name[n + 3] = ext[2], name[n + 4] = 0;
}

static int is_tap(const char *name)
{
    uint32_t n = 0;
    while (name[n])
        n++;
    return n > 4 && name[n - 4] == '.' && (name[n - 3] | 0x20) == 't' && (name[n - 2] | 0x20) == 'a' &&
           (name[n - 1] | 0x20) == 'p';
}

/* The extension of a block without one: the screen, .scr; the rest, .bin */
static const char *block_ext(uint32_t start, uint32_t len)
{
    return start == 16384 && len == 6912 ? "scr" : "bin";
}

static void file_error(int e)
{
    zxb_fail(e == ZXV_E_NOENT ? "T File not found" : e == ZXV_E_NOCARD ? "U No SD card" : "V Disk error");
}

static void check(int r, int f)
{
    if (r < 0) {
        if (f >= 0)
            zxv_file_close(f);
        file_error(r);
    }
}

/* A byte of memory: below 64K, the emulated Spectrum memory */
static int valid(uint32_t a) { return a < 65536 || (a >= ZXV_VRAM0_BASE && a < ZXV_URAM_END); }

static uint8_t mem_get(uint32_t a) { return a < 65536 ? zxb_vpeek(a) : *(volatile uint8_t *)(uintptr_t)a; }

static void mem_put(uint32_t a, uint8_t v)
{
    if (a < 65536)
        zxb_vpoke(a, v);
    else
        *(volatile uint8_t *)(uintptr_t)a = v;
}

static void check_range(uint32_t start, uint32_t len)
{
    if (len && (!valid(start) || !valid(start + len - 1) || (start < 65536) != (start + len - 1 < 65536)))
        zxb_ERROR(ERR_RANGE);
}

/* A tape block: its length, the flag, the data and the checksum */
static void write_block(int f, uint8_t flag, const uint8_t *hdr, uint32_t start, uint32_t len)
{
    uint8_t buf[256];
    uint8_t sum = flag;
    buf[0] = (uint8_t)(len + 2), buf[1] = (uint8_t)((len + 2) >> 8), buf[2] = flag;
    check(zxv_file_write(f, buf, 3), f);
    for (uint32_t i = 0; i < len;) {
        uint32_t n = 0;
        for (; n < sizeof(buf) && i < len; n++, i++) {
            buf[n] = hdr ? hdr[i] : mem_get(start + i);
            sum ^= buf[n];
        }
        check(zxv_file_write(f, buf, n), f);
    }
    check(zxv_file_write(f, &sum, 1), f);
}

/* SAVE name CODE start, len (and SCREEN$ and DATA) */
void zxb_SAVE_CODE(uint32_t len, uint32_t start, zstr s)
{
    char name[NAME_MAX + 8], tname[10];
    if (file_name(s, name, tname))
        add_ext(name, block_ext(start, len));
    int tape = is_tap(name);
    if (len == 0)
        zxb_ERROR(9);                   /* A Invalid argument */
    if (tape && len > 65535)            /* a tape block, at most */
        zxb_ERROR(ERR_RANGE);
    check_range(start, len);
    int f = zxv_file_open(name, ZXV_O_WRITE | ZXV_O_CREATE | ZXV_O_TRUNC);
    check(f, -1);
    if (tape) {
        uint8_t hdr[17] = { 3 };        /* CODE */
        for (int i = 0; i < 10; i++)
            hdr[1 + i] = (uint8_t)tname[i];
        hdr[11] = (uint8_t)len, hdr[12] = (uint8_t)(len >> 8);
        hdr[13] = (uint8_t)start, hdr[14] = (uint8_t)(start >> 8);
        /* The second parameter is 32768 in the Spectrum; above 64K, the high
         * half of the address (the header only has 16 bits for it) */
        uint32_t p2 = start < 65536 ? 32768 : start >> 16;
        hdr[15] = (uint8_t)p2, hdr[16] = (uint8_t)(p2 >> 8);
        write_block(f, 0x00, hdr, 0, 17);
        write_block(f, 0xFF, 0, start, len);
    } else {
        uint8_t buf[256];
        for (uint32_t i = 0; i < len;) {
            uint32_t n = 0;
            for (; n < sizeof(buf) && i < len; n++, i++)
                buf[n] = mem_get(start + i);
            check(zxv_file_write(f, buf, n), f);
        }
    }
    zxv_file_close(f);
}

static int read_n(int f, uint8_t *buf, uint32_t n)
{
    int r = zxv_file_read(f, buf, n);
    check(r, f);
    return r == (int)n;
}

/* LOAD (is_load) or VERIFY name CODE [start [, len]] (and SCREEN$ and DATA).
 * Without start (0), the address of the tape header; without len (0), all. */
void zxb_LOAD_CODE(uint32_t is_load, uint32_t len, uint32_t start, zstr s)
{
    char name[NAME_MAX + 8], tname[10];
    int f;
    if (file_name(s, name, tname)) {
        /* Without extension: .scr or .bin, and then a tape */
        uint32_t n = 0;
        while (name[n])
            n++;
        add_ext(name, block_ext(start, len));
        f = zxv_file_open(name, ZXV_O_READ);
        if (f == ZXV_E_NOENT) {
            name[n] = 0;
            add_ext(name, "tap");
            f = zxv_file_open(name, ZXV_O_READ);
        }
    } else {
        f = zxv_file_open(name, ZXV_O_READ);
    }
    check(f, -1);
    int tape = is_tap(name);
    uint32_t avail = 0xFFFFFFFFu;
    if (tape) {
        /* The first CODE header, and the data block after it */
        uint8_t b[19];
        for (;;) {
            if (!read_n(f, b, 2))
                goto bad;
            uint32_t n = b[0] | (uint32_t)b[1] << 8;
            if (n == 19) {
                if (!read_n(f, b, 19))
                    goto bad;
                if (b[0] == 0x00 && b[1] == 3) {
                    if (!start) {
                        uint32_t p2 = b[16] | (uint32_t)b[17] << 8;
                        start = b[14] | (uint32_t)b[15] << 8 | (p2 != 32768 ? p2 << 16 : 0);
                    }
                    if (!read_n(f, b, 3) || b[2] != 0xFF)
                        goto bad;
                    avail = (b[0] | (uint32_t)b[1] << 8) - 2;
                    break;
                }
            } else if (zxv_file_seek(f, (int32_t)n, ZXV_SEEK_CUR) < 0) {
                goto bad;
            }
        }
    } else if (!start) {
        zxv_file_close(f);
        zxb_ERROR(9);                   /* A Invalid argument: raw bytes need an address */
    }
    if (len == 0 || len > avail)
        len = avail;
    uint8_t buf[256];
    uint32_t done = 0;
    while (done < len) {
        uint32_t want = len - done < sizeof(buf) ? len - done : sizeof(buf);
        int n = zxv_file_read(f, buf, want);
        check(n, f);
        if (n == 0)
            break;
        check_range(start + done, (uint32_t)n);
        for (int i = 0; i < n; i++) {
            if (is_load)
                mem_put(start + done + i, buf[i]);
            else if (mem_get(start + done + i) != buf[i])
                goto bad;
        }
        done += (uint32_t)n;
    }
    if (tape && done < len)
        goto bad;
    zxv_file_close(f);
    return;
bad:
    zxv_file_close(f);
    zxb_ERROR(ERR_TAPE);
}
