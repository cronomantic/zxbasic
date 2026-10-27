/*
 * zxb_lib.c - What the ZX-V versions of the libraries (stdlib/*.bas) call:
 * their ASM blocks call these functions with zxb_ccall.
 */
#include "zxb.h"

/* string.bas */
static void change_case(zstr s, int upper)
{
    uint32_t n = zstr_len(s);
    for (uint32_t i = 0; i < n; i++) {
        uint8_t c = s[2 + i];
        if (upper ? c >= 'a' && c <= 'z' : c >= 'A' && c <= 'Z')
            s[2 + i] = c ^ 0x20;
    }
}

void zxb_lib_ucase(zstr s) { change_case(s, 1); }
void zxb_lib_lcase(zstr s) { change_case(s, 0); }

/* hex.bas: n digits in hexadecimal */
zstr zxb_lib_hex(uint32_t v, int digits)
{
    char buf[8];
    for (int i = digits - 1; i >= 0; i--, v >>= 4)
        buf[i] = "0123456789ABCDEF"[v & 15];
    return zxb_newstr(buf, (uint32_t)digits);
}

/* memcopy.bas: below 64K, through the Spectrum memory */
void zxb_lib_memmove(uint32_t src, uint32_t dst, uint32_t n)
{
    if (src >= 0x10000 && dst >= 0x10000) {
        __builtin_memmove((void *)(uintptr_t)dst, (const void *)(uintptr_t)src, n);
    } else if (dst <= src) {
        for (uint32_t i = 0; i < n; i++)
            zxb_vpoke(dst + i, zxb_vpeek(src + i));
    } else {
        for (uint32_t i = n; i--;)
            zxb_vpoke(dst + i, zxb_vpeek(src + i));
    }
}

void zxb_lib_memset(uint32_t dst, int v, uint32_t n)
{
    if (dst >= 0x10000) {
        __builtin_memset((void *)(uintptr_t)dst, v, n);
        return;
    }
    for (uint32_t i = 0; i < n; i++)
        zxb_vpoke(dst + i, (uint8_t)v);
}

/* random.bas: xorshift32 */
static uint32_t rnd_state = 2463534242u;

uint32_t zxb_lib_rand(void)
{
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;
    return rnd_state;
}

/* A number from 0 to limit (both included) */
uint32_t zxb_lib_rand_limit(uint32_t limit)
{
    limit &= 0xFF;
    return limit ? zxb_lib_rand() % (limit + 1) : 0;
}

/* keys.bas: the keyboard matrix as the Spectrum ports; scancode = port high byte << 8 | bits */
uint32_t zxb_lib_multikeys(uint32_t scancode)
{
    return ~zxb_IN((scancode & 0xFF00) | 0xFE) & scancode & 0x1F;
}

uint32_t zxb_lib_scancode(void)
{
    for (int i = 0; i < 8; i++) {
        uint32_t hi = 0xFF & ~(1u << i);
        uint32_t bits = ~zxb_IN(hi << 8 | 0xFE) & 0x1F;
        if (bits)
            return hi << 8 | bits;
    }
    return 0;
}

/* input.bas: a line of up to max characters, with a cursor; Enter ends it.
 * As the ZX Basic one, it is erased from the screen at the end. */
zstr zxb_lib_input(uint32_t max)
{
    char buf[256];
    uint32_t n = 0;
    max &= 0xFFFF;
    if (max > sizeof(buf))
        max = sizeof(buf);
    for (;;) {
        zxb_print_char('_');
        zxb_print_char(8);
        int k;
        while (!(k = zxb_key()))
            zxv_wait_vblank();
        zxb_print_char(' ');
        zxb_print_char(8);
        if (k == 13)
            break;
        if (k == 8 && n) {
            n--;
            zxb_print_char(8);
        } else if (k >= 32 && k < 128 && n < max) {
            buf[n++] = (char)k;
            zxb_print_char(k);
        }
    }
    for (uint32_t i = 0; i < n; i++) {
        zxb_print_char(8);
        zxb_print_char(' ');
        zxb_print_char(8);
    }
    return zxb_newstr(buf, n);
}

void zxb_lib_vblank(void)
{
    zxv_wait_vblank();
}

uint32_t zxb_lib_hmirror(uint32_t b)
{
    uint32_t r = 0;
    for (int i = 0; i < 8; i++)
        r |= ((b >> i) & 1) << (7 - i);
    return r;
}
