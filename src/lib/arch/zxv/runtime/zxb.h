/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb.h - Runtime of ZX Basic programs for ZX-V.
 *
 * The compiler (src/arch/zxv) calls these functions with the ilp32 ABI:
 * first the fparam values, then the stack parameters (top of the stack
 * first). A string fparam takes two arguments: the string and 1 if it
 * must be freed.
 *
 * Strings are pointers to a 16-bit length followed by the characters
 * (not terminated); NULL is the empty string.
 */
#ifndef ZXB_H
#define ZXB_H

#include <stddef.h>
#include <stdint.h>

#include "zxv_api.h"
#include "zxv_gfx.h"
#include "zxb_math.h"

typedef uint8_t *zstr;

static inline uint32_t zstr_len(const uint8_t *s) { return s ? (uint32_t)(s[0] | s[1] << 8) : 0; }

/* Dynamic memory (zxb_mem.c) */
void *zxb_alloc(uint32_t n);
void *zxb_try_alloc(uint32_t n);        /* NULL if there is no room */
void zxb_free(void *p);
void zxb_mem_init(void);

/* The engine of the ROM draws on the drawing page, with CLIP and PATTERN
 * (zxb_zxv.c), between these two; and that page as a bitmap */
void zxb_gfx_begin(void);
void zxb_gfx_end(void);
/* The same with CLIP but without the PATTERN of the program, for what sets
 * its own patterns (DRAW3D: its shading); the end takes away any pattern */
void zxb_gfx_begin_plain(void);
void zxb_gfx_end_plain(void);
zxv_bitmap zxb_page_bitmap(void);

/* The tile maps and the camera of the last FLOOR (zxb_maps.c) */
zxv_tilemap zxb_maps_tilemap(int m);
const zxv_m7cam *zxb_maps_floor_cam(int *width);

/* Strings (zxb_str.c) */
zstr zxb_newstr(const void *chars, uint32_t len);
zstr zxb_loadstr(const uint8_t *s);

/* The screen, as a ZX Spectrum (zxb_screen.c) */
void zxb_screen_init(void);
void zxb_screen_stop(void);             /* Before going back to the BASIC */
void zxb_CLS(void);
void zxb_print_char(int c);
void zxb_print_text(const char *s, int n);
uint8_t zxb_vpeek(uint32_t addr);
void zxb_vpoke(uint32_t addr, uint8_t v);
void zxb_report(const char *msg);       /* On the bottom line, as the Spectrum reports */
void zxb_BORDER(int n);
int zxb_paper(void);                    /* The PAPER colour, 0-15 */
extern volatile uint32_t *zxb_vram;     /* The drawing page, where the Spectrum screen goes */
/* Before writing on the drawing page: if it is the page shown, wait while
 * the system is still sending it to the screen (spec 4.8, LINK_BUSY). */
void zxb_vram_wait(void);
extern int zxb_wpl;                     /* Its 32-bit words per line */
uint32_t zxb_IN(uint32_t port);         /* The Spectrum ports: keyboard and Kempston */

/* Reading files: the one in the current folder or, if it is not there,
 * the one the program carries inside (zxbc --embed; zxb_embed.c). The
 * handles are only for these functions. */
int zxb_rd_open(const char *name);
int zxb_rd_read(int f, void *buf, uint32_t n);
int zxb_rd_seek(int f, int32_t offset, int whence);
int zxb_rd_close(int f);

/* Others */
int zxb_key(void);              /* Next key of the queue, or 0 */
void zxb_echo(const char *s, int n);   /* -DZXB_TEST: to the zxv-cli console */
/* Spectrum time: fiftieths of a second since the program started, although
 * ZX-V draws 60 frames a second (ZX-V decision D81). For PAUSE and FRAMES. */
uint32_t zxb_ticks50(void);

/* ZX Basic error codes (ERROR n): the report is n + 1 */
#define ERR_OUT_OF_MEMORY       3
#define ERR_INVALID_ARG         9       /* A Invalid argument */
#define ERR_OUT_OF_SCREEN       4       /* 5 Out of screen */
#define ERR_SUBSCRIPT           2       /* 3 Subscript wrong */

#endif
