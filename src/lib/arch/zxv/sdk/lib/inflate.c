/*
 * inflate.c - Descomprimir datos en la memoria (include/zxv_inflate.h).
 *
 * Opcional: añade $(SDK)/lib/inflate.c a tu programa.
 *
 * Deflate (RFC 1951) con los códigos de Huffman canónicos descodificados
 * bit a bit con lo que hay de cada largo: poca memoria (unos 2 KB de pila)
 * y bastante deprisa para cargar entre fases. El mismo descodificador que
 * zxvbank.c (los bancos, que leen del fichero) y music/inflate.c (que va
 * poco a poco con una ventana), aquí de la memoria a la memoria. MIT.
 */
#include <stdint.h>

#include "zxv_inflate.h"

typedef struct {
    uint16_t count[16];
    uint16_t sym[288];
} huff_t;

typedef struct {
    const uint8_t *in;
    uint32_t pos, n;
    uint32_t bits;
    int nbits;
    uint8_t *out;
    uint32_t done, max;
    int space;                          /* se paró porque no cabía */
    huff_t lit, dist;
} inf_t;

static const uint16_t len_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                       35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t len_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                       3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t dist_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                        8193, 12289, 16385, 24577 };
static const uint8_t dist_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

/* El byte siguiente de la entrada, o -1. */
static int next_byte(inf_t *z)
{
    if (z->pos == z->n)
        return -1;
    return z->in[z->pos++];
}

/* n bits (hasta 16), del menos significativo; -1 si se acaban. */
static int32_t get(inf_t *z, int n)
{
    while (z->nbits < n) {
        int b = next_byte(z);
        if (b < 0)
            return -1;
        z->bits |= (uint32_t)b << z->nbits;
        z->nbits += 8;
    }
    int32_t v = (int32_t)(z->bits & ((1u << n) - 1u));
    z->bits >>= n;
    z->nbits -= n;
    return v;
}

static int decode(inf_t *z, const huff_t *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        int32_t b = get(z, 1);
        if (b < 0)
            return -1;
        code |= b;
        int count = h->count[len];
        if (code - first < count)
            return h->sym[index + code - first];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

static int build(huff_t *h, const uint8_t *lens, int n)
{
    uint16_t offs[16];
    for (int i = 0; i < 16; i++)
        h->count[i] = 0;
    for (int i = 0; i < n; i++)
        h->count[lens[i]]++;
    h->count[0] = 0;
    int left = 1;
    for (int len = 1; len < 16; len++) {
        left = (left << 1) - h->count[len];
        if (left < 0)
            return -1;
    }
    offs[1] = 0;
    for (int len = 1; len < 15; len++)
        offs[len + 1] = (uint16_t)(offs[len] + h->count[len]);
    for (int i = 0; i < n; i++)
        if (lens[i])
            h->sym[offs[lens[i]]++] = (uint16_t)i;
    return 0;
}

static int fixed(inf_t *z)
{
    uint8_t lens[288];
    for (int i = 0; i < 288; i++)
        lens[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
    build(&z->lit, lens, 288);
    for (int i = 0; i < 30; i++)
        lens[i] = 5;
    return build(&z->dist, lens, 30);
}

static int dynamic(inf_t *z)
{
    static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    int32_t nlen = get(z, 5), ndist = get(z, 5), ncode = get(z, 4);
    if (ncode < 0)
        return -1;
    nlen += 257;
    ndist += 1;
    ncode += 4;
    if (nlen > 286 || ndist > 30)
        return -1;
    uint8_t lens[288 + 32];
    for (int i = 0; i < 19; i++) {
        int32_t v = i < ncode ? get(z, 3) : 0;
        if (v < 0)
            return -1;
        lens[order[i]] = (uint8_t)v;
    }
    if (build(&z->lit, lens, 19))
        return -1;
    for (int i = 0; i < nlen + ndist;) {
        int sym = decode(z, &z->lit);
        if (sym < 0)
            return -1;
        if (sym < 16) {
            lens[i++] = (uint8_t)sym;
            continue;
        }
        int rep, val = 0;
        if (sym == 16) {
            if (!i)
                return -1;
            val = lens[i - 1];
            rep = 3 + get(z, 2);
        } else if (sym == 17) {
            rep = 3 + get(z, 3);
        } else {
            rep = 11 + get(z, 7);
        }
        if (rep < 3 || i + rep > nlen + ndist)
            return -1;
        while (rep--)
            lens[i++] = (uint8_t)val;
    }
    if (!lens[256])
        return -1;                      /* sin el final de bloque */
    if (build(&z->lit, lens, nlen) || build(&z->dist, lens + nlen, ndist))
        return -1;
    return 0;
}

/* Los símbolos de un bloque con códigos, al destino. */
static int codes(inf_t *z)
{
    for (;;) {
        int sym = decode(z, &z->lit);
        if (sym < 0)
            return -1;
        if (sym < 256) {
            if (z->done >= z->max) {
                z->space = 1;
                return -1;
            }
            z->out[z->done++] = (uint8_t)sym;
            continue;
        }
        if (sym == 256)
            return 0;
        sym -= 257;
        if (sym >= 29)
            return -1;
        int32_t e = get(z, len_extra[sym]);
        int d = decode(z, &z->dist);
        if (e < 0 || d < 0 || d >= 30)
            return -1;
        uint32_t len = len_base[sym] + (uint32_t)e;
        int32_t de = get(z, dist_extra[d]);
        if (de < 0)
            return -1;
        uint32_t dist = dist_base[d] + (uint32_t)de;
        if (dist > z->done)
            return -1;
        if (len > z->max - z->done) {
            z->space = 1;
            return -1;
        }
        uint8_t *o = z->out + z->done;
        const uint8_t *from = o - dist;             /* puede solaparse con o: byte a byte */
        for (uint32_t i = 0; i < len; i++)
            o[i] = from[i];
        z->done += len;
    }
}

/* Descomprime todo: 0 bien, -1 mal. */
static int inflate_all(inf_t *z)
{
    for (int last = 0; !last;) {
        int32_t h = get(z, 3);
        if (h < 0)
            return -1;
        last = h & 1;
        int type = h >> 1;
        if (type == 0) {
            z->bits = 0;                /* al byte siguiente */
            z->nbits = 0;
            int32_t a = get(z, 16), b = get(z, 16);
            if (a < 0 || b < 0 || (a ^ 0xFFFF) != b)
                return -1;
            if ((uint32_t)a > z->max - z->done) {
                z->space = 1;
                return -1;
            }
            for (int32_t i = 0; i < a; i++) {
                int c = next_byte(z);
                if (c < 0)
                    return -1;
                z->out[z->done++] = (uint8_t)c;
            }
        } else if (type == 1) {
            if (fixed(z) || codes(z))
                return -1;
        } else if (type == 2) {
            if (dynamic(z) || codes(z))
                return -1;
        } else {
            return -1;
        }
    }
    return 0;
}

static uint32_t get32le(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int run(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t out_max, uint32_t *used)
{
    inf_t z;
    z.in = in;
    z.pos = 0;
    z.n = in_len;
    z.bits = 0;
    z.nbits = 0;
    z.out = out;
    z.done = 0;
    z.max = out_max;
    z.space = 0;
    int bad = inflate_all(&z);
    if (bad)
        return z.space ? ZXV_INFLATE_E_SPACE : ZXV_INFLATE_E_DATA;
    if (used)
        *used = z.pos;                  /* bytes enteros: lo que sobra del último, no cuenta */
    return (int)z.done;
}

int zxv_inflate(const void *in, uint32_t in_len, void *out, uint32_t out_max)
{
    if (!in || (!out && out_max) || out_max > 0x7FFFFFFFu)
        return ZXV_INFLATE_E_DATA;
    return run((const uint8_t *)in, in_len, (uint8_t *)out, out_max, 0);
}

/* La cabecera de un .gz: dónde empieza el deflate, o -1. */
static int gzip_start(const uint8_t *p, uint32_t n)
{
    if (n < 18 || p[0] != 0x1F || p[1] != 0x8B || p[2] != 8 || (p[3] & 0xE0))
        return -1;
    uint32_t at = 10, flg = p[3];
    if (flg & 4) {                                  /* FEXTRA */
        if (at + 2 > n)
            return -1;
        at += 2 + (p[at] | (uint32_t)p[at + 1] << 8);
    }
    for (int k = 8; k <= 16; k += 8)                /* FNAME, FCOMMENT: hasta el 0 */
        if (flg & k) {
            while (at < n && p[at])
                at++;
            at++;
        }
    if (flg & 2)                                    /* FHCRC */
        at += 2;
    return at + 8 <= n ? (int)at : -1;
}

int zxv_gunzip_size(const void *in, uint32_t in_len)
{
    const uint8_t *p = (const uint8_t *)in;
    if (!p || gzip_start(p, in_len) < 0)
        return ZXV_INFLATE_E_DATA;
    uint32_t size = get32le(p + in_len - 4);
    return size > 0x7FFFFFFFu ? ZXV_INFLATE_E_DATA : (int)size;
}

/* CRC-32 (el de gzip), de 4 en 4 bits. */
static uint32_t crc32_4(const uint8_t *p, uint32_t n)
{
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        c = c >> 4 ^ t[c & 15];
        c = c >> 4 ^ t[c & 15];
    }
    return ~c;
}

int zxv_gunzip(const void *in, uint32_t in_len, void *out, uint32_t out_max)
{
    const uint8_t *p = (const uint8_t *)in;
    if (!p || (!out && out_max) || out_max > 0x7FFFFFFFu)
        return ZXV_INFLATE_E_DATA;
    int at = gzip_start(p, in_len);
    if (at < 0)
        return ZXV_INFLATE_E_DATA;
    uint32_t used = 0;
    int r = run(p + at, in_len - (uint32_t)at - 8, (uint8_t *)out, out_max, &used);
    if (r < 0)
        return r;
    const uint8_t *trail = p + in_len - 8;
    if (get32le(trail + 4) != (uint32_t)r || crc32_4((const uint8_t *)out, (uint32_t)r) != get32le(trail))
        return ZXV_INFLATE_E_DATA;
    return r;
}
