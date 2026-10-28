/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_str.c - Strings: a 16-bit length and the characters, in the heap.
 *
 * The compiler says which strings are temporary (the result of an
 * expression): those belong to the function that gets them, which frees
 * them. The others (variables, constants) are only read.
 */
#include "zxb.h"

zstr zxb_newstr(const void *chars, uint32_t len)
{
    if (len == 0)
        return NULL;
    if (len > 65535)
        len = 65535;
    zstr s = zxb_alloc(len + 2);
    s[0] = (uint8_t)len;
    s[1] = (uint8_t)(len >> 8);
    if (chars)
        __builtin_memcpy(s + 2, chars, len);
    return s;
}

/* A copy of a string that is not temporary */
zstr zxb_loadstr(const uint8_t *s)
{
    return s ? zxb_newstr(s + 2, zstr_len(s)) : NULL;
}

/* *dest = src: a temporary src is kept, the others are copied */
void zxb_storestr(zstr *dest, zstr src, int owned)
{
    zstr old = *dest;
    *dest = owned ? src : zxb_loadstr(src);
    zxb_free(old);
}

zstr zxb_addstr(zstr a, zstr b, int flags)
{
    uint32_t la = zstr_len(a), lb = zstr_len(b);
    zstr r = zxb_newstr(NULL, la + lb);
    if (r) {
        if (la)
            __builtin_memcpy(r + 2, a + 2, la);
        if (lb)
            __builtin_memcpy(r + 2 + la, b + 2, lb);
    }
    if (flags & 1)
        zxb_free(a);
    if (flags & 2)
        zxb_free(b);
    return r;
}

int zxb_strcmp(zstr a, zstr b, int flags)
{
    uint32_t la = zstr_len(a), lb = zstr_len(b);
    int r = 0;
    for (uint32_t i = 0; i < la && i < lb && !r; i++)
        r = (int)a[2 + i] - (int)b[2 + i];
    if (!r)
        r = la < lb ? -1 : la > lb;
    if (flags & 1)
        zxb_free(a);
    if (flags & 2)
        zxb_free(b);
    return r < 0 ? -1 : r > 0;
}

uint32_t zxb_lenstr(zstr s, int owned)
{
    uint32_t n = zstr_len(s);
    if (owned)
        zxb_free(s);
    return n;
}

/* s$(lower TO upper) */
zstr zxb_STRSLICE(int owned, uint32_t upper, uint32_t lower, zstr s)
{
    uint32_t n = zstr_len(s);
    zstr r = NULL;
    if (n && upper >= n)
        upper = n - 1;
    if (n && lower <= upper)
        r = zxb_newstr(s + 2 + lower, upper - lower + 1);
    if (owned)
        zxb_free(s);
    return r;
}

/* LET s$(lower TO upper) = src: s$ keeps its length; src is cut or padded with spaces */
void zxb_LETSUBSTR(zstr s, uint32_t upper, uint32_t lower, int owned, zstr src)
{
    uint32_t n = zstr_len(s), m = zstr_len(src);
    if (n && upper >= n)
        upper = n - 1;
    for (uint32_t i = lower, j = 0; n && i <= upper; i++, j++)
        s[2 + i] = j < m ? src[2 + j] : ' ';
    if (owned)
        zxb_free(src);
}

/* a$ = b$ with arrays of strings: each element of dst gets a copy of the one of src */
void zxb_STR_ARRAYCOPY(uint32_t count, const zstr *src, zstr *dst)
{
    for (uint32_t i = 0; i < count; i++) {
        zstr old = dst[i];
        dst[i] = zxb_loadstr(src[i]);
        zxb_free(old);
    }
}

zstr zxb_CHR(uint32_t count, const uint32_t *codes)
{
    zstr r = zxb_newstr(NULL, count);
    for (uint32_t i = 0; i < count; i++)
        r[2 + i] = (uint8_t)codes[i];
    return r;
}

uint32_t zxb_ASC(zstr s, int owned)
{
    uint32_t c = zstr_len(s) ? s[2] : 0;
    if (owned)
        zxb_free(s);
    return c;
}

zstr zxb_STR_FAST(double x)
{
    char buf[24];
    return zxb_newstr(buf, (uint32_t)fp_format(x, buf));
}

/* VAL: a number, with spaces and a sign before it */
double zxb_VAL(zstr s, int owned)
{
    char buf[64];
    uint32_t n = zstr_len(s), o = 0;
    for (uint32_t i = 0; i < n && o < sizeof(buf) - 1; i++)
        if (s[2 + i] != ' ')
            buf[o++] = (char)s[2 + i];
    buf[o] = 0;
    if (owned)
        zxb_free(s);

    const uint8_t *p = (const uint8_t *)buf;
    int neg = 0;
    while (*p == '+' || *p == '-')
        neg ^= *p++ == '-';
    double v = 0;
    if (!fp_parse(&p, &v))
        return 0;
    return neg ? -v : v;
}

zstr zxb_INKEY(void)
{
    int k = zxb_key();
    if (k <= 0 || k > 255)
        return NULL;
    uint8_t c = (uint8_t)k;
    return zxb_newstr(&c, 1);
}
