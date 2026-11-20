/*
 * inflate.c - Descomprime deflate (RFC 1951) poco a poco, para tocar un
 * .vgz sin tenerlo entero en la memoria: lo que va saliendo queda en una
 * ventana de 32 KB (lo más atrás que puede mirar el deflate), y el
 * reproductor pide más cuando lo ha leído todo. Los .gz (RFC 1952): su
 * cabecera y lo que ocupan sin comprimir.
 *
 * Los códigos de Huffman, canónicos, se descodifican bit a bit con lo que
 * hay de cada largo: lento a lo grande, pero un VGM pide unos pocos KB por
 * segundo. Licencia MIT, como el resto del SDK.
 */
#include "music_int.h"

enum { ST_HEADER, ST_STORED, ST_CODES, ST_END, ST_ERROR };

static const uint16_t len_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                       35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t len_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                       3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t dist_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                        8193, 12289, 16385, 24577 };
static const uint8_t dist_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

/* n bits (hasta 24), del menos significativo; -1 si se acaban los datos. */
static int32_t get(mus_inflate_t *z, int n)
{
    while (z->nbits < n) {
        if (z->pos >= z->n)
            return -1;
        z->bits |= (uint32_t)z->in[z->pos++] << z->nbits;
        z->nbits += 8;
    }
    int32_t v = (int32_t)(z->bits & ((1u << n) - 1u));
    z->bits >>= n;
    z->nbits -= n;
    return v;
}

/* Un símbolo: los códigos de cada largo van seguidos (canónicos), así que
 * basta con contar. -1 sin datos o con un código que no existe. */
static int decode(mus_inflate_t *z, const mus_huff_t *h)
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

/* El código de n símbolos con estos largos (0: no está). */
static int build(mus_huff_t *h, const uint8_t *lens, int n)
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
            return -1;                  /* más códigos de los que caben */
    }
    offs[1] = 0;
    for (int len = 1; len < 15; len++)
        offs[len + 1] = (uint16_t)(offs[len] + h->count[len]);
    for (int i = 0; i < n; i++)
        if (lens[i])
            h->sym[offs[lens[i]]++] = (uint16_t)i;
    return 0;
}

static int fixed(mus_inflate_t *z)
{
    uint8_t lens[288];
    for (int i = 0; i < 288; i++)
        lens[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
    build(&z->lit, lens, 288);
    for (int i = 0; i < 30; i++)
        lens[i] = 5;
    return build(&z->dist, lens, 30);
}

static int dynamic(mus_inflate_t *z)
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
        int32_t e;
        if (sym == 16) {
            if (!i)
                return -1;
            val = lens[i - 1];
            rep = 3 + (e = get(z, 2));
        } else if (sym == 17) {
            rep = 3 + (e = get(z, 3));
        } else {
            rep = 11 + (e = get(z, 7));
        }
        if (e < 0 || i + rep > nlen + ndist)
            return -1;
        while (rep--)
            lens[i++] = (uint8_t)val;
    }
    if (!lens[256] || build(&z->lit, lens, nlen) || build(&z->dist, lens + nlen, ndist))
        return -1;
    return 0;
}

void mus_inflate_init(mus_inflate_t *z, const uint8_t *in, uint32_t n, uint8_t *win)
{
    z->in = in;
    z->n = n;
    z->pos = 0;
    z->bits = 0;
    z->nbits = 0;
    z->win = win;
    z->out = 0;
    z->state = ST_HEADER;
    z->last = 0;
    z->left = 0;
    z->mlen = z->mdist = 0;
}

static void put(mus_inflate_t *z, uint8_t b)
{
    z->win[z->out++ & MUS_WIN_MASK] = b;
}

uint32_t mus_inflate_run(mus_inflate_t *z, uint32_t want)
{
    uint32_t start = z->out;
    while (z->out - start < want) {
        if (z->mlen) {                  /* una copia de lo de antes */
            put(z, z->win[(z->out - z->mdist) & MUS_WIN_MASK]);
            z->mlen--;
            continue;
        }
        switch (z->state) {
        case ST_HEADER: {
            if (z->last) {
                z->state = ST_END;
                break;
            }
            int32_t last = get(z, 1), type = get(z, 2);
            if (type < 0) {
                z->state = ST_ERROR;
                break;
            }
            z->last = (uint8_t)last;
            if (type == 0) {            /* guardado: desde el byte siguiente */
                z->bits = 0;
                z->nbits = 0;
                if (z->pos + 4 > z->n || (z->in[z->pos] ^ z->in[z->pos + 2]) != 0xFF ||
                    (z->in[z->pos + 1] ^ z->in[z->pos + 3]) != 0xFF) {
                    z->state = ST_ERROR;
                    break;
                }
                z->left = z->in[z->pos] | (uint32_t)z->in[z->pos + 1] << 8;
                z->pos += 4;
                z->state = ST_STORED;
            } else if (type == 1) {
                z->state = fixed(z) ? ST_ERROR : ST_CODES;
            } else if (type == 2) {
                z->state = dynamic(z) ? ST_ERROR : ST_CODES;
            } else {
                z->state = ST_ERROR;
            }
            break;
        }
        case ST_STORED:
            if (!z->left) {
                z->state = ST_HEADER;
            } else if (z->pos >= z->n) {
                z->state = ST_ERROR;
            } else {
                put(z, z->in[z->pos++]);
                z->left--;
            }
            break;
        case ST_CODES: {
            int sym = decode(z, &z->lit);
            if (sym < 0) {
                z->state = ST_ERROR;
            } else if (sym < 256) {
                put(z, (uint8_t)sym);
            } else if (sym == 256) {
                z->state = ST_HEADER;
            } else {
                sym -= 257;
                if (sym >= 29) {
                    z->state = ST_ERROR;
                    break;
                }
                int32_t le = get(z, len_extra[sym]);
                int d = decode(z, &z->dist);
                int32_t de = d >= 0 && d < 30 ? get(z, dist_extra[d]) : -1;
                if (le < 0 || de < 0) {
                    z->state = ST_ERROR;
                    break;
                }
                uint32_t dist = dist_base[d] + (uint32_t)de;
                if (dist > z->out || dist > MUS_WIN) {
                    z->state = ST_ERROR;
                    break;
                }
                z->mlen = (uint16_t)(len_base[sym] + le);
                z->mdist = (uint16_t)dist;
            }
            break;
        }
        default:
            return z->out - start;      /* el final, o un error */
        }
    }
    return z->out - start;
}

int mus_gzip_data(const uint8_t *m, uint32_t n, uint32_t *at, uint32_t *size)
{
    if (n < 18 || m[0] != 0x1F || m[1] != 0x8B || m[2] != 8)
        return -1;
    uint8_t flg = m[3];
    uint32_t p = 10;
    if (flg & 4) {                      /* FEXTRA */
        if (p + 2 > n)
            return -1;
        p += 2u + (m[p] | (uint32_t)m[p + 1] << 8);
    }
    for (int k = 3; k <= 4; k++)        /* FNAME, FCOMMENT: con su cero */
        if (flg & (1 << k)) {
            while (p < n && m[p])
                p++;
            p++;
        }
    if (flg & 2)                        /* FHCRC */
        p += 2;
    if (p + 8 > n)
        return -1;
    *at = p;
    *size = m[n - 4] | (uint32_t)m[n - 3] << 8 | (uint32_t)m[n - 2] << 16 | (uint32_t)m[n - 1] << 24;
    return 0;
}
