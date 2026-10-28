/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic.
 * Copied from ZX-V (rom/src/basic/fp.c, MIT like its SDK), so compiled
 * programs give the same results as its BASIC. */
/*
 * fp.c - Números: formato de PRINT, lectura de literales y la forma de
 * 5 bytes del Spectrum.
 *
 * El formato reproduce la rutina PRINT-FP: 8 cifras significativas
 * redondeadas, sin ceros finales, y notación E si hay 9 o más cifras antes
 * del punto o más de 4 ceros tras él (0.5 -> "0.5", 0.05 -> ".05").
 */
#include "zxb_math.h"

/* Mayor número representable en 5 bytes: (1 - 2^-32) * 2^127. */
#define FP5_MAX     1.7014118342085e38
/* Menor normalizado: 0.5 * 2^-127. */
#define FP5_MIN     2.9387358770557e-39

static const double p10_bin[] = { 1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256 };

/* 10^k para |k| < 512; exacto hasta 10^22. */
static double p10(int k)
{
    int neg = k < 0;
    if (neg)
        k = -k;
    double r = 1.0;
    for (int i = 0; k && i < 9; i++, k >>= 1)
        if (k & 1)
            r *= p10_bin[i];
    return neg ? 1.0 / r : r;
}

/* x / 10^k o x * 10^-k, la operación más exacta. */
static double scale10(double x, int k)
{
    if (k >= 0)
        return x / p10(k);
    if (k >= -308)
        return x * p10(-k);
    return x * p10(-k - 100) * 1e100;
}


double fp_check(double x)
{
    double a = x < 0 ? -x : x;
    if (!(a <= FP5_MAX))            /* también NaN */
        basic_error(R_TOO_BIG);
    if (a < FP5_MIN)
        return 0.0;
    return x;
}

static int put_uint(char *buf, uint32_t v)
{
    char t[10];
    int n = 0, o = 0;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        buf[o++] = t[--n];
    return o;
}

static int format(double x, char *buf)
{
    int o = 0;
    if (x == 0) {
        buf[0] = '0';
        return 1;
    }
    if (x < 0) {
        buf[o++] = '-';
        x = -x;
    }

    /* Exponente decimal e: 10^e <= x < 10^(e+1). */
    int b2;
    b_frexp(x, &b2);
    int e = (int)b_floor((b2 - 1) * 0.30102999566398);
    while (e > -330 && scale10(x, e) < 1.0)
        e--;
    while (scale10(x, e + 1) >= 1.0)
        e++;

    /* 8 cifras redondeadas. */
    uint32_t d = (uint32_t)(scale10(x, e - 7) + 0.5);
    if (d >= 100000000u) {
        d = (d + 5) / 10;
        e++;
    }
    if (d < 10000000u)              /* por errores de redondeo en la escala */
        d = 10000000u;
    char dig[8];
    for (int i = 7; i >= 0; i--) {
        dig[i] = (char)('0' + d % 10);
        d /= 10;
    }
    int nd = 8;
    while (nd > 1 && dig[nd - 1] == '0')
        nd--;

    int before = e + 1;             /* cifras antes del punto */
    if (before >= 9 || before <= -5) {
        buf[o++] = dig[0];
        if (nd > 1) {
            buf[o++] = '.';
            for (int i = 1; i < nd; i++)
                buf[o++] = dig[i];
        }
        buf[o++] = 'E';
        buf[o++] = e < 0 ? '-' : '+';
        o += put_uint(buf + o, (uint32_t)(e < 0 ? -e : e));
        return o;
    }
    if (before <= 0) {
        if (before == 0)
            buf[o++] = '0';
        buf[o++] = '.';
        for (int i = 0; i < -before; i++)
            buf[o++] = '0';
        for (int i = 0; i < nd; i++)
            buf[o++] = dig[i];
        return o;
    }
    for (int i = 0; i < before; i++)
        buf[o++] = i < nd ? dig[i] : '0';
    if (nd > before) {
        buf[o++] = '.';
        for (int i = before; i < nd; i++)
            buf[o++] = dig[i];
    }
    return o;
}

int fp_parse(const uint8_t **pp, double *out)
{
    const uint8_t *p = *pp;
    uint64_t mant = 0;
    int exp10 = 0, ndig = 0, any = 0;

    while (is_digit(*p)) {
        any = 1;
        if (ndig < 19) {
            mant = mant * 10 + (uint64_t)(*p - '0');
            if (mant)
                ndig++;
        } else {
            exp10++;
        }
        p++;
    }
    if (*p == '.' && (any || is_digit(p[1]))) {
        p++;
        while (is_digit(*p)) {
            any = 1;
            if (ndig < 19) {
                mant = mant * 10 + (uint64_t)(*p - '0');
                if (mant)
                    ndig++;
                exp10--;
            }
            p++;
        }
    }
    if (!any)
        return 0;
    if (*p == 'e' || *p == 'E') {
        const uint8_t *q = p + 1;
        int neg = 0, ev = 0;
        if (*q == '+' || *q == '-')
            neg = *q++ == '-';
        if (is_digit(*q)) {
            while (is_digit(*q)) {
                if (ev < 10000)
                    ev = ev * 10 + (*q - '0');
                q++;
            }
            exp10 += neg ? -ev : ev;
            p = q;
        }
    }
    *pp = p;
    double v = (double)mant;
    if (mant == 0)
        v = 0;
    else if (exp10 > 0)
        v = exp10 > 400 ? 1e300 * 1e300 : v * p10(exp10);
    else if (exp10 < 0)
        v = exp10 < -400 ? 0.0 : scale10(v, -exp10);
    *out = v;
    return 1;
}

int fp_to5(double x, uint8_t b[5])
{
    if (x == b_floor(x) && x >= -65535.0 && x <= 65535.0) {
        int32_t i = (int32_t)x;
        uint16_t u = (uint16_t)(i < 0 ? 65536 + i : i);
        b[0] = 0;
        b[1] = i < 0 ? 0xFF : 0x00;
        b[2] = (uint8_t)u;
        b[3] = (uint8_t)(u >> 8);
        b[4] = 0;
        return 1;
    }
    int neg = x < 0;
    if (neg)
        x = -x;
    int e;
    double m = b_frexp(x, &e);                    /* 0.5 <= m < 1 */
    double mm = b_ldexp(m, 32) + 0.5;
    uint32_t mant;
    if (mm >= 4294967296.0) {
        mant = 0x80000000u;
        e++;
    } else {
        mant = (uint32_t)mm;
    }
    if (e + 128 > 255)
        return 0;
    if (e + 128 < 1) {
        memset(b, 0, 5);
        return 1;
    }
    b[0] = (uint8_t)(e + 128);
    b[1] = (uint8_t)(((mant >> 24) & 0x7F) | (neg ? 0x80 : 0));
    b[2] = (uint8_t)(mant >> 16);
    b[3] = (uint8_t)(mant >> 8);
    b[4] = (uint8_t)mant;
    return 1;
}

/* x con la precisión de la forma de 5 bytes (mantisa de 32 bits). Las
 * comparaciones la usan para que 0.1*3=0.3 sea cierto, como en el
 * Spectrum. */
double fp_round5(double x)
{
    if (x == 0 || (x == b_floor(x) && x >= -65535.0 && x <= 65535.0))
        return x;
    int e;
    double m = b_frexp(x, &e);
    return b_ldexp(b_floor(b_ldexp(m, 32) + 0.5), e - 32);
}

double fp_from5(const uint8_t b[5])
{
    if (b[0] == 0) {
        int v = b[2] | (b[3] << 8);
        if (b[1] == 0xFF)
            v -= 65536;
        return (double)v;
    }
    uint32_t mant = ((uint32_t)(b[1] | 0x80) << 24) | ((uint32_t)b[2] << 16) |
                    ((uint32_t)b[3] << 8) | b[4];
    double v = b_ldexp((double)mant, b[0] - 128 - 32);
    return (b[1] & 0x80) ? -v : v;
}

int fp_format(double x, char *buf)
{
    int n = format(x, buf);
    speed_ev(SE_PRINT_NUM);
    /* El 48K multiplica o divide por 10 hasta dejarlo en su sitio, y saca
     * las cifras decimales una a una. */
    double a = x < 0 ? -x : x;
    uint32_t big = 0, small = 0, frac = 0;
    if (a != 0) {
        for (; a >= 10 && big < 40; a /= 10)
            big++;
        for (; a < 1 && small < 40; a *= 10)
            small++;
    }
    for (int i = 0, in_frac = 0; i < n && buf[i] != 'E'; i++) {
        if (in_frac)
            frac++;
        if (buf[i] == '.')
            in_frac = 1;
    }
    speed_ev_n(SE_PRINT_EXP10, big);
    speed_ev_n(SE_PRINT_SMALL, small);
    speed_ev_n(SE_PRINT_FRAC, frac);
    return n;
}
