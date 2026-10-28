/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_mem.c - Dynamic memory for strings and arrays: first fit, with the
 * free neighbours joined when a block is freed.
 *
 * The heap goes from the end of the program (__heap_start, zxv.ld) to
 * __heap_end, which leaves 32 KB for the stack.
 */
#include "zxb.h"

extern uint8_t __heap_start[], __heap_end[];

/* Every block starts with a header: its size (header included, multiple of
 * 8) and, in bit 0, whether it is in use. The last header has size 0. */
typedef uint32_t header_t;
#define USED            1u
#define HDR             8u      /* Header + padding: the data is 8-aligned */
#define SIZE(h)         ((h) & ~7u)

static header_t *first;

void zxb_mem_init(void)
{
    uintptr_t a = ((uintptr_t)__heap_start + 7) & ~(uintptr_t)7;
    uintptr_t b = (uintptr_t)__heap_end & ~(uintptr_t)7;
    first = (header_t *)a;
    *first = (uint32_t)(b - a - HDR);
    *(header_t *)(b - HDR) = 0;
}

static header_t *next(header_t *h)
{
    return (header_t *)((uint8_t *)h + SIZE(*h));
}

void *zxb_alloc(uint32_t n)
{
    uint32_t need = (n + HDR + 7) & ~7u;
    for (header_t *h = first; *h; h = next(h)) {
        if (*h & USED)
            continue;
        /* Join the free blocks that follow */
        header_t *f;
        while (*(f = next(h)) && !(*f & USED))
            *h += SIZE(*f);
        if (SIZE(*h) < need)
            continue;
        if (SIZE(*h) - need >= 2 * HDR) {
            header_t *rest = (header_t *)((uint8_t *)h + need);
            *rest = SIZE(*h) - need;
            *h = need;
        }
        *h |= USED;
        return (uint8_t *)h + HDR;
    }
    zxb_ERROR(ERR_OUT_OF_MEMORY);
    return NULL;
}

/* allocate: NULL if there is no room */
void *zxb_try_alloc(uint32_t n)
{
    uint32_t need = (n + HDR + 7) & ~7u;
    for (header_t *h = first; *h; h = next(h)) {
        if (*h & USED)
            continue;
        header_t *f;
        while (*(f = next(h)) && !(*f & USED))
            *h += SIZE(*f);
        if (SIZE(*h) >= need)
            return zxb_alloc(n);
    }
    return NULL;
}

/* The free memory: all of it, or the largest block */
uint32_t zxb_mem_avail(int largest)
{
    uint32_t total = 0, max = 0;
    for (header_t *h = first; *h; h = next(h)) {
        if (*h & USED)
            continue;
        header_t *f;
        while (*(f = next(h)) && !(*f & USED))
            *h += SIZE(*f);
        uint32_t n = SIZE(*h) - HDR;
        total += n;
        if (n > max)
            max = n;
    }
    return largest ? max : total;
}

void *zxb_realloc(void *p, uint32_t n)
{
    if (!p)
        return zxb_try_alloc(n);
    uint32_t old = SIZE(*(header_t *)((uint8_t *)p - HDR)) - HDR;
    if (n <= old)
        return p;
    void *q = zxb_try_alloc(n);
    if (q) {
        __builtin_memcpy(q, p, old);
        zxb_free(p);
    }
    return q;
}

void *zxb_lib_callocate(uint32_t n)
{
    void *p = zxb_try_alloc(n);
    if (p)
        __builtin_memset(p, 0, n);
    return p;
}

void zxb_free(void *p)
{
    if (p)
        *(header_t *)((uint8_t *)p - HDR) &= ~USED;
}

/* MEM_FREE (fparam: the pointer) */
void zxb_MEM_FREE(void *p)
{
    zxb_free(p);
}
