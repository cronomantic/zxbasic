/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_math.h - What zxb_bmath.c and zxb_fp.c (copied from the ZX-V ROM) need.
 */
#ifndef ZXB_MATH_H
#define ZXB_MATH_H

#include <stddef.h>
#include <stdint.h>

void *memset(void *s, int c, size_t n);     /* zxvlib.c */

double b_fabs(double x);
double b_floor(double x);
double b_ldexp(double x, int n);
double b_frexp(double x, int *e);
double b_sqrt(double x);
double b_exp(double x);
double b_log(double x);
double b_sin(double x);
double b_cos(double x);
double b_tan(double x);
double b_atan(double x);
double b_asin(double x);
double b_acos(double x);

int fp_format(double x, char *buf);   /* as PRINT: up to 14 chars */
int fp_parse(const uint8_t **pp, double *out);

static inline int is_digit(int c) { return c >= '0' && c <= '9'; }

/* Errors and timings of the ROM BASIC */
void zxb_ERROR(int code);
#define R_TOO_BIG               5     /* 6 Number too big */
#define basic_error(r)          zxb_ERROR(r)
#define speed_ev(e)             ((void)0)
#define speed_ev_n(e, n)        ((void)(n))

#endif
