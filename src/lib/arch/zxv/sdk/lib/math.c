/*
 * math.c - Cuentas para juegos sin coma flotante (include/zxv_math.h).
 *
 * Opcional: añádelo a los ficheros de tu programa ($(SDK)/lib/math.c) si
 * usas zxv_math.h. La tabla del seno es la del motor gráfico de la ROM
 * (rom/tools/mksin.py: sin(i·90°/256) en 16.16), así que zxv_sin da lo
 * mismo que el seno con el que m7_camera y m7_affine calculan sus líneas.
 */
#include "zxv_math.h"

static const int32_t sin_q[257] = {
    0, 402, 804, 1206, 1608, 2010, 2412, 2814,
    3216, 3617, 4019, 4420, 4821, 5222, 5623, 6023,
    6424, 6824, 7224, 7623, 8022, 8421, 8820, 9218,
    9616, 10014, 10411, 10808, 11204, 11600, 11996, 12391,
    12785, 13180, 13573, 13966, 14359, 14751, 15143, 15534,
    15924, 16314, 16703, 17091, 17479, 17867, 18253, 18639,
    19024, 19409, 19792, 20175, 20557, 20939, 21320, 21699,
    22078, 22457, 22834, 23210, 23586, 23961, 24335, 24708,
    25080, 25451, 25821, 26190, 26558, 26925, 27291, 27656,
    28020, 28383, 28745, 29106, 29466, 29824, 30182, 30538,
    30893, 31248, 31600, 31952, 32303, 32652, 33000, 33347,
    33692, 34037, 34380, 34721, 35062, 35401, 35738, 36075,
    36410, 36744, 37076, 37407, 37736, 38064, 38391, 38716,
    39040, 39362, 39683, 40002, 40320, 40636, 40951, 41264,
    41576, 41886, 42194, 42501, 42806, 43110, 43412, 43713,
    44011, 44308, 44604, 44898, 45190, 45480, 45769, 46056,
    46341, 46624, 46906, 47186, 47464, 47741, 48015, 48288,
    48559, 48828, 49095, 49361, 49624, 49886, 50146, 50404,
    50660, 50914, 51166, 51417, 51665, 51911, 52156, 52398,
    52639, 52878, 53114, 53349, 53581, 53812, 54040, 54267,
    54491, 54714, 54934, 55152, 55368, 55582, 55794, 56004,
    56212, 56418, 56621, 56823, 57022, 57219, 57414, 57607,
    57798, 57986, 58172, 58356, 58538, 58718, 58896, 59071,
    59244, 59415, 59583, 59750, 59914, 60075, 60235, 60392,
    60547, 60700, 60851, 60999, 61145, 61288, 61429, 61568,
    61705, 61839, 61971, 62101, 62228, 62353, 62476, 62596,
    62714, 62830, 62943, 63054, 63162, 63268, 63372, 63473,
    63572, 63668, 63763, 63854, 63944, 64031, 64115, 64197,
    64277, 64354, 64429, 64501, 64571, 64639, 64704, 64766,
    64827, 64884, 64940, 64993, 65043, 65091, 65137, 65180,
    65220, 65259, 65294, 65328, 65358, 65387, 65413, 65436,
    65457, 65476, 65492, 65505, 65516, 65525, 65531, 65535,
    65536,
};

/* Un cuarto de vuelta, con p en [0, 0x4000] (1/64 de paso de la tabla). */
static int32_t sin_quarter(uint32_t p)
{
    uint32_t idx = p >> 6, frac = p & 63;
    if (idx >= 256)
        return sin_q[256];
    int32_t s0 = sin_q[idx], s1 = sin_q[idx + 1];
    return s0 + (int32_t)(((uint32_t)(s1 - s0) * frac) >> 6);
}

zxv_fix zxv_sin(int32_t a)
{
    uint32_t u = (uint32_t)a & 0xFFFFu, o = u & 0x3FFFu, quad = u >> 14;
    int32_t v = sin_quarter(quad & 1 ? 0x4000u - o : o);   /* 90°-180°: espejo */
    return quad & 2 ? -v : v;
}

zxv_fix zxv_cos(int32_t a) { return zxv_sin(a + 0x4000); }

/* atan2 con CORDIC: se gira el vector hacia el eje x con pasos de atan(2^-i)
 * sumando lo girado (en 1/2^24 de vuelta). Antes se lleva al semiplano de
 * x >= 0 (sumando media vuelta) y a unos 2^28 de tamaño: ni se desborda ni
 * pierde precisión con vectores pequeños. */
static const int32_t atan_steps[22] = {
    2097152, 1238021, 654136, 332050, 166669, 83416, 41718, 20860, 10430, 5215, 2608,
    1304, 652, 326, 163, 81, 41, 20, 10, 5, 3, 1,
};

int32_t zxv_atan2(int32_t y, int32_t x)
{
    int64_t X = x, Y = y;
    if (X == 0 && Y == 0)
        return 0;
    int32_t acc = 0;
    if (X < 0) {
        X = -X;
        Y = -Y;
        acc = 1 << 23;                          /* media vuelta */
    }
    int64_t m = (X > (Y < 0 ? -Y : Y)) ? X : (Y < 0 ? -Y : Y);
    while (m >= (1LL << 29)) {
        X >>= 1;
        Y >>= 1;
        m >>= 1;
    }
    while (m < (1LL << 27)) {
        X <<= 1;
        Y <<= 1;
        m <<= 1;
    }
    int32_t cx = (int32_t)X, cy = (int32_t)Y;
    for (int i = 0; i < 22; i++) {
        int32_t dx = cx >> i, dy = cy >> i;
        if (cy > 0) {
            cx += dy;
            cy -= dx;
            acc += atan_steps[i];
        } else {
            cx -= dy;
            cy += dx;
            acc -= atan_steps[i];
        }
    }
    return ((acc + 128) >> 8) & 0xFFFF;
}

uint32_t zxv_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}

uint32_t zxv_isqrt64(uint64_t v)
{
    if (v <= 0xFFFFFFFFu)
        return zxv_isqrt((uint32_t)v);
    uint64_t r = 0, b = 1ull << 62;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return (uint32_t)r;
}

zxv_fix zxv_fsqrt(zxv_fix v) { return v <= 0 ? 0 : (zxv_fix)zxv_isqrt64((uint64_t)v << 16); }

uint32_t zxv_hypot(int32_t x, int32_t y)
{
    return zxv_isqrt64((uint64_t)((int64_t)x * x) + (uint64_t)((int64_t)y * y));
}

static uint32_t rng_state = 2463534242u;

uint32_t zxv_rand_next(uint32_t *state)
{
    uint32_t s = *state ? *state : 1;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *state = s;
    return s;
}

void zxv_srand(uint32_t seed) { rng_state = seed ? seed : 1; }

uint32_t zxv_rand(void) { return zxv_rand_next(&rng_state); }

int zxv_rand_range(int lo, int hi)
{
    if (hi < lo) {
        int t = lo;
        lo = hi;
        hi = t;
    }
    uint32_t span = (uint32_t)hi - (uint32_t)lo + 1u;
    uint32_t r = zxv_rand();
    if (span == 0)                              /* todos los int */
        return (int)r;
    return (int)((uint32_t)lo + (uint32_t)(((uint64_t)r * span) >> 32));
}
