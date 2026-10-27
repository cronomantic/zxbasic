/* Copied from ZX-V (rom/src/basic/bmath.c), so compiled programs give the same results as its BASIC. */
/*
 * bmath.c - Funciones matemáticas en double para el BASIC.
 *
 * Son propias para que el intérprete dé exactamente los mismos resultados
 * en la ROM y en las pruebas del PC, sin depender de la libm de cada
 * plataforma. Los métodos (reducción del argumento y polinomios minimax)
 * siguen los de fdlibm:
 *
 *   Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *   Developed at SunSoft, a Sun Microsystems, Inc. business.
 *   Permission to use, copy, modify, and distribute this software is freely
 *   granted, provided that this notice is preserved.
 *
 * La precisión es de unos pocos ulp en el rango que usa el BASIC, mucho más
 * de lo que muestran las 8 cifras de PRINT.
 */
#include "zxb_math.h"

typedef union {
    double d;
    uint64_t u;
} dbits_t;

static uint64_t bits_of(double x)
{
    dbits_t b;
    b.d = x;
    return b.u;
}

static double from_bits(uint64_t u)
{
    dbits_t b;
    b.u = u;
    return b.d;
}

static int exp_of(double x) { return (int)((bits_of(x) >> 52) & 0x7FF); }

double b_fabs(double x)
{
    return from_bits(bits_of(x) & 0x7FFFFFFFFFFFFFFFull);
}

double b_floor(double x)
{
    int e = exp_of(x) - 1023;
    if (e >= 52)
        return x;                               /* ya es entero (o inf/NaN) */
    if (e < 0) {
        if (x == 0)
            return x;
        return x < 0 ? -1.0 : 0.0;
    }
    uint64_t u = bits_of(x);
    uint64_t frac = 0x000FFFFFFFFFFFFFull >> e;
    if (!(u & frac))
        return x;
    if (u >> 63)
        u += frac;                              /* hacia -inf */
    return from_bits(u & ~frac);
}

double b_ldexp(double x, int n)
{
    /* Por pasos para no desbordar los factores intermedios. */
    while (n > 1000) {
        x *= from_bits((uint64_t)(1023 + 1000) << 52);
        n -= 1000;
    }
    while (n < -1000) {
        x *= from_bits((uint64_t)(1023 - 1000) << 52);
        n += 1000;
    }
    return x * from_bits((uint64_t)(1023 + n) << 52);
}

double b_frexp(double x, int *e)
{
    int ex = exp_of(x);
    if (x == 0 || ex == 0x7FF) {
        *e = 0;
        return x;
    }
    if (ex == 0) {                              /* subnormal */
        x *= 18014398509481984.0;               /* 2^54 */
        ex = exp_of(x) - 54;
    }
    *e = ex - 1022;
    return from_bits((bits_of(x) & 0x800FFFFFFFFFFFFFull) | (0x3FEull << 52));
}

double b_sqrt(double x)
{
    if (x <= 0)
        return x == 0 ? x : 0.0 / 0.0;
    int e;
    double m = b_frexp(x, &e);                  /* 0.5 <= m < 1 */
    if (e & 1) {
        m *= 2;
        e--;
    }
    /* Aproximación lineal y Newton (converge cuadráticamente). */
    double y = 0.41731 + 0.59016 * m;
    for (int i = 0; i < 5; i++)
        y = 0.5 * (y + m / y);
    return b_ldexp(y, e / 2);
}

/* --- exp y log ----------------------------------------------------------------- */

static const double ln2_hi = 6.93147180369123816490e-01;
static const double ln2_lo = 1.90821492927058770002e-10;
static const double inv_ln2 = 1.44269504088896338700e+00;

double b_exp(double x)
{
    if (x > 709.78)
        return 1e300 * 1e300;
    if (x < -745.13)
        return 0.0;
    static const double P1 = 1.66666666666666019037e-01, P2 = -2.77777777770155933842e-03,
                        P3 = 6.61375632143793436117e-05, P4 = -1.65339022054652515390e-06,
                        P5 = 4.13813679705723846039e-08;
    int k = (int)b_floor(x * inv_ln2 + 0.5);
    double hi = x - k * ln2_hi, lo = k * ln2_lo;
    double r = hi - lo;
    double t = r * r;
    double c = r - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    double y = 1.0 - ((lo - (r * c) / (2.0 - c)) - hi);
    return b_ldexp(y, k);
}

double b_log(double x)
{
    if (x <= 0)
        return x == 0 ? -1e300 * 1e300 : 0.0 / 0.0;
    static const double Lg1 = 6.666666666666735130e-01, Lg2 = 3.999999999940941908e-01,
                        Lg3 = 2.857142874366239149e-01, Lg4 = 2.222219843214978396e-01,
                        Lg5 = 1.818357216161805012e-01, Lg6 = 1.531383769920937332e-01,
                        Lg7 = 1.479819860511658591e-01;
    int k;
    double m = b_frexp(x, &k);                  /* 0.5 <= m < 1 */
    if (m < 0.70710678118654752440) {
        m *= 2;
        k--;
    }
    double f = m - 1.0;
    double hfsq = 0.5 * f * f;
    double s = f / (2.0 + f);
    double z = s * s, w = z * z;
    double t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    double t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    double R = t2 + t1;
    return k * ln2_hi - ((hfsq - (s * (hfsq + R) + k * ln2_lo)) - f);
}

/* --- Trigonometría ---------------------------------------------------------------- */

static double k_sin(double x, double y)
{
    static const double S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03,
                        S3 = -1.98412698298579493134e-04, S4 = 2.75573137070700676789e-06,
                        S5 = -2.50507602534068634195e-08, S6 = 1.58969099521155010221e-10;
    double z = x * x, v = z * x;
    double r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

static double k_cos(double x, double y)
{
    static const double C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03,
                        C3 = 2.48015872894767294178e-05, C4 = -2.75573143513906633035e-07,
                        C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11;
    double z = x * x;
    double r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
    double hz = 0.5 * z, w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}

/* x = n*pi/2 + (y0 + y1), |y0 + y1| <= pi/4. Devuelve n mod 4. */
static int rem_pio2(double x, double *y0, double *y1)
{
    static const double pio2_1 = 1.57079632673412561417e+00, pio2_1t = 6.07710050650619224932e-11,
                        pio2_2 = 6.07710050630396597660e-11, pio2_2t = 2.02226624879595063154e-21,
                        pio2_3 = 2.02226624871116645580e-21, pio2_3t = 8.47842766036889956997e-32,
                        invpio2 = 6.36619772367581382433e-01;
    if (b_fabs(x) <= 0.78539816339744830962) {
        *y0 = x;
        *y1 = 0;
        return 0;
    }
    double fn = b_floor(x * invpio2 + 0.5);
    double r = x - fn * pio2_1;
    double w = fn * pio2_1t;
    double y = r - w;
    /* Segunda y tercera vuelta si hubo mucha cancelación. */
    if (b_fabs(y) < 1e-3 * b_fabs(fn)) {
        double t = r;
        w = fn * pio2_2;
        r = t - w;
        w = fn * pio2_2t - ((t - r) - w);
        y = r - w;
        if (b_fabs(y) < 1e-12 * b_fabs(fn)) {
            t = r;
            w = fn * pio2_3;
            r = t - w;
            w = fn * pio2_3t - ((t - r) - w);
            y = r - w;
        }
    }
    *y0 = y;
    *y1 = (r - y) - w;
    double q = fn - 4.0 * b_floor(fn / 4.0);
    return (int)q;
}

double b_sin(double x)
{
    double a, b;
    switch (rem_pio2(x, &a, &b)) {
    case 0: return k_sin(a, b);
    case 1: return k_cos(a, b);
    case 2: return -k_sin(a, b);
    default: return -k_cos(a, b);
    }
}

double b_cos(double x)
{
    double a, b;
    switch (rem_pio2(x, &a, &b)) {
    case 0: return k_cos(a, b);
    case 1: return -k_sin(a, b);
    case 2: return -k_cos(a, b);
    default: return k_sin(a, b);
    }
}

double b_tan(double x)
{
    double a, b;
    int n = rem_pio2(x, &a, &b);
    double s = k_sin(a, b), c = k_cos(a, b);
    return (n & 1) ? -c / s : s / c;
}

double b_atan(double x)
{
    static const double atanhi[] = { 4.63647609000806093515e-01, 7.85398163397448278999e-01,
                                     9.82793723247329054082e-01, 1.57079632679489655800e+00 };
    static const double atanlo[] = { 2.26987774529616870924e-17, 3.06161699786838301793e-17,
                                     1.39033110312309984516e-17, 6.12323399573676603587e-17 };
    static const double aT[] = {
        3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01,
        -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02,
        6.66107313738753120669e-02, -5.83357013379057348645e-02, 4.97687799461593236017e-02,
        -3.65315727442169155270e-02, 1.62858201153657823623e-02,
    };
    int neg = x < 0;
    double ax = b_fabs(x);
    int id;
    if (ax > 1e17)
        return neg ? -atanhi[3] - atanlo[3] : atanhi[3] + atanlo[3];
    if (ax < 0.4375) {
        id = -1;
    } else if (ax < 1.1875) {
        if (ax < 0.6875) {
            id = 0;
            ax = (2.0 * ax - 1.0) / (2.0 + ax);
        } else {
            id = 1;
            ax = (ax - 1.0) / (ax + 1.0);
        }
    } else if (ax < 2.4375) {
        id = 2;
        ax = (ax - 1.5) / (1.0 + 1.5 * ax);
    } else {
        id = 3;
        ax = -1.0 / ax;
    }
    double z = ax * ax, w = z * z;
    double s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    double s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    double r;
    if (id < 0)
        r = ax - ax * (s1 + s2);
    else
        r = atanhi[id] - ((ax * (s1 + s2) - atanlo[id]) - ax);
    return neg ? -r : r;
}

double b_asin(double x)
{
    if (x >= 1.0)
        return 1.57079632679489661923;
    if (x <= -1.0)
        return -1.57079632679489661923;
    return b_atan(x / b_sqrt((1.0 - x) * (1.0 + x)));
}

double b_acos(double x)
{
    if (x >= 1.0)
        return 0.0;
    if (x <= -1.0)
        return 3.14159265358979323846;
    return 2.0 * b_atan(b_sqrt((1.0 - x) / (1.0 + x)));
}
