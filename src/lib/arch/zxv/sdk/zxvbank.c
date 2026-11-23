/*
 * zxvbank.c - Los bancos de un programa .ZXV (D109, zxv_bank.h): se leen
 * del propio fichero del programa, que la ROM deja abierto mientras va.
 *
 * Los comprimidos (deflate, RFC 1951) se descomprimen directamente en el
 * destino, leyendo el fichero de 512 en 512 bytes: no hace falta más
 * memoria que unos 2 KB de pila. Los códigos de Huffman se descodifican
 * bit a bit, como en music/inflate.c (de donde sale esto): unos 30 ms por
 * cada 100 KB a 144 MHz, para cargar entre fases. MIT, como el SDK.
 */
#include <stdint.h>

#include "zxv.h"
#include "zxv_api.h"
#include "zxv_bank.h"

#define REG32(a)        (*(volatile uint32_t *)(uintptr_t)(a))
#define CHUNK           512u
#define INDEX_MAX       4096u           /* bancos por programa, como mucho */

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* CRC-32 (el de zlib), de 4 en 4 bits. */
static uint32_t crc32_add(uint32_t c, const uint8_t *p, uint32_t n)
{
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    c = ~c;
    while (n--) {
        c ^= *p++;
        c = c >> 4 ^ t[c & 15];
        c = c >> 4 ^ t[c & 15];
    }
    return ~c;
}

/* El fichero del programa (el número), o un error. */
static int prog_file(void)
{
    if (zxv_api_version() < 2)
        return ZXV_E_ACCESS;
    uint32_t f = REG32(ZXV_SV_PROG_FILE);
    return f ? (int)f - 1 : ZXV_E_NOENT;
}

static int read_at(int f, uint32_t at, void *buf, uint32_t n)
{
    int r = zxv_file_seek(f, (int32_t)at, ZXV_SEEK_SET);
    if (r < 0)
        return r;
    r = zxv_file_read(f, buf, n);
    return r < 0 ? r : (uint32_t)r == n ? 0 : ZXV_E_FORMAT;
}

/* La entrada del banco name (ZXV_BANK_ENTRY bytes en e), comprobando el
 * índice y que es del programa en marcha. */
static int find(int f, const char *name, uint8_t *e)
{
    int size = zxv_file_seek(f, 0, ZXV_SEEK_END);
    if (size < 0)
        return size;
    uint8_t t[ZXV_BANK_TAIL], h[ZXV_BANK_HEAD];
    int r = (uint32_t)size < ZXV_BANK_TAIL ? ZXV_E_CHANGED : read_at(f, (uint32_t)size - ZXV_BANK_TAIL, t, sizeof t);
    if (r < 0)
        return r;
    for (int i = 0; i < 8; i++)
        if (t[i] != (uint8_t)ZXV_BANK_MAGIC[i])
            return ZXV_E_CHANGED;       /* lo cambiaron por uno sin bancos */
    uint32_t at = get32(t + ZXV_BANK_T_INDEX), len = get32(t + ZXV_BANK_T_INDEX_LEN);
    if (len < ZXV_BANK_HEAD || at > (uint32_t)size - ZXV_BANK_TAIL || len > (uint32_t)size - ZXV_BANK_TAIL - at)
        return ZXV_E_CHANGED;           /* no es el fichero que se cargó */
    if ((r = read_at(f, at, h, sizeof h)) < 0)
        return r;
    uint32_t count = get32(h + ZXV_BANK_I_COUNT);
    if (get32(h + ZXV_BANK_I_PROG_CRC) != REG32(ZXV_SV_PROG_CRC) || count > INDEX_MAX ||
        len != ZXV_BANK_HEAD + count * ZXV_BANK_ENTRY)
        return ZXV_E_CHANGED;
    /* Las entradas, de 8 en 8: se comprueba su CRC y se busca el nombre. */
    uint8_t buf[8 * ZXV_BANK_ENTRY];
    uint32_t crc = 0;
    int found = 0;
    for (uint32_t i = 0; i < count; i += 8) {
        uint32_t k = count - i < 8 ? count - i : 8;
        if ((r = read_at(f, at + ZXV_BANK_HEAD + i * ZXV_BANK_ENTRY, buf, k * ZXV_BANK_ENTRY)) < 0)
            return r;
        crc = crc32_add(crc, buf, k * ZXV_BANK_ENTRY);
        for (uint32_t j = 0; j < k && !found; j++) {
            const uint8_t *p = buf + j * ZXV_BANK_ENTRY;
            int n = 0;
            while (n <= ZXV_BANK_NAME_MAX && name[n] && p[ZXV_BANK_E_NAME + n] == (uint8_t)name[n])
                n++;
            if (n <= ZXV_BANK_NAME_MAX && !name[n] && !p[ZXV_BANK_E_NAME + n]) {
                for (int b = 0; b < ZXV_BANK_ENTRY; b++)
                    e[b] = p[b];
                found = 1;
            }
        }
    }
    if (crc != get32(h + ZXV_BANK_I_CRC))
        return ZXV_E_CHANGED;
    if (!found)
        return ZXV_E_NOENT;
    uint32_t off = get32(e + ZXV_BANK_E_OFFSET), stored = get32(e + ZXV_BANK_E_STORED);
    if (off > at || stored > at - off || e[ZXV_BANK_E_METHOD] > ZXV_BANK_DEFLATE)
        return ZXV_E_FORMAT;
    return 0;
}

/* La entrada del banco name, para zxvbankcode.c (los de código): el número
 * del fichero del programa, o un error. */
int __zxvbank_find(const char *name, uint8_t e[ZXV_BANK_ENTRY])
{
    int f = prog_file();
    if (f < 0)
        return f;
    int r = find(f, name, e);
    return r < 0 ? r : f;
}

int zxv_bank_size(const char *name)
{
    uint8_t e[ZXV_BANK_ENTRY];
    int r = __zxvbank_find(name, e);
    return r < 0 ? r : (int)get32(e + ZXV_BANK_E_SIZE);
}

/* --- Deflate, del fichero al destino --------------------------------------- */

typedef struct {
    uint16_t count[16];
    uint16_t sym[288];
} huff_t;

typedef struct {
    int f;
    uint32_t left;                      /* lo que queda por leer del fichero */
    uint8_t in[CHUNK];
    uint32_t pos, n;
    uint32_t bits;
    int nbits;
    uint8_t *out;
    uint32_t done, max;
    int err;                            /* el de leer el fichero, si lo hubo */
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

/* El byte siguiente del fichero, o -1. */
static int next_byte(inf_t *z)
{
    if (z->pos == z->n) {
        if (!z->left || z->err)
            return -1;
        uint32_t k = z->left < CHUNK ? z->left : CHUNK;
        int r = zxv_file_read(z->f, z->in, k);
        if (r <= 0) {
            z->err = r < 0 ? r : ZXV_E_FORMAT;
            return -1;
        }
        z->left -= (uint32_t)r;
        z->pos = 0;
        z->n = (uint32_t)r;
    }
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
            if (z->done >= z->max)
                return -1;
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
        if (dist > z->done || len > z->max - z->done)
            return -1;
        uint8_t *o = z->out + z->done;
        for (uint32_t i = 0; i < len; i++)
            o[i] = o[i - dist];
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
            if (a < 0 || b < 0 || (a ^ 0xFFFF) != b || (uint32_t)a > z->max - z->done)
                return -1;
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

/* Lo del banco de la entrada e, del fichero f a dst (que tiene sitio para
 * lo que ocupa): descomprimido y comprobado. Lo que ocupa, o un error. */
int __zxvbank_read(int f, const uint8_t e[ZXV_BANK_ENTRY], void *dst)
{
    uint32_t off = get32(e + ZXV_BANK_E_OFFSET), stored = get32(e + ZXV_BANK_E_STORED);
    uint32_t size = get32(e + ZXV_BANK_E_SIZE);
    int r;
    if ((r = zxv_file_seek(f, (int32_t)off, ZXV_SEEK_SET)) < 0)
        return r;
    if (e[ZXV_BANK_E_METHOD] == ZXV_BANK_STORED) {
        if (stored != size)
            return ZXV_E_FORMAT;
        r = zxv_file_read(f, dst, size);
        if (r < 0)
            return r;
        if ((uint32_t)r != size)
            return ZXV_E_FORMAT;
    } else {
        inf_t z;
        z.f = f;
        z.left = stored;
        z.pos = z.n = 0;
        z.bits = 0;
        z.nbits = 0;
        z.out = (uint8_t *)dst;
        z.done = 0;
        z.max = size;
        z.err = 0;
        int bad = inflate_all(&z);
        if (z.err)
            return z.err;
        if (bad || z.done != size)
            return ZXV_E_FORMAT;
    }
    if (crc32_add(0, (const uint8_t *)dst, size) != get32(e + ZXV_BANK_E_CRC))
        return ZXV_E_FORMAT;
    return (int)size;
}

int zxv_bank_load(const char *name, void *dst, uint32_t max)
{
    uint8_t e[ZXV_BANK_ENTRY];
    int f = __zxvbank_find(name, e);
    if (f < 0)
        return f;
    if (e[ZXV_BANK_E_KIND] != ZXV_BANK_DATA)
        return ZXV_E_NOENT;             /* uno de código: lo cargan sus puentes */
    if (get32(e + ZXV_BANK_E_SIZE) > max)
        return ZXV_E_RANGE;
    return __zxvbank_read(f, e, dst);
}
