/* SPDX-License-Identifier: MIT. Copyright (c) 2026 Cronomantic. */
/*
 * zxb_misc.c - Start and end of the program, numbers, arrays, keyboard,
 * sound and errors.
 */
#include "zxb.h"

static uint32_t seed;

void zxb_init(void)
{
    zxb_mem_init();
    zxb_screen_init();
    zxb_CLS();
    seed = *(volatile uint32_t *)(uintptr_t)ZXV_SV_FRAMES & 0xFFFF;
}

/* --- zxv-cli --test: the printed text goes to its console ----------------------- */

#ifdef ZXB_TEST
static char echo_buf[128];
static int echo_len;

static void echo_flush(void)
{
    register uint32_t a0 __asm__("a0") = 1;
    register uint32_t a1 __asm__("a1") = (uint32_t)(uintptr_t)echo_buf;
    register uint32_t a2 __asm__("a2") = (uint32_t)echo_len;
    register uint32_t a7 __asm__("a7") = 64;
    if (echo_len)
        __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    echo_len = 0;
}

static void test_exit(int code)
{
    zxb_screen_stop();
    echo_flush();
    register uint32_t a0 __asm__("a0") = (uint32_t)code;
    register uint32_t a7 __asm__("a7") = 93;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    for (;;)
        ;
}

void zxb_echo(const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        echo_buf[echo_len++] = s[i];
        if (echo_len == (int)sizeof(echo_buf) || s[i] == '\n')
            echo_flush();
    }
}
#else
void zxb_echo(const char *s, int n)
{
    (void)s;
    (void)n;
}
#endif

/* The BASIC starts again when the program returns, and clears the screen:
 * as in the Spectrum, the report goes to the bottom line, and the screen
 * stays until a key is pressed. */
static void report_and_wait(const char *msg)
{
    zxb_report(msg);
    while (zxb_key())
        ;
    while (!zxb_key())
        zxv_wait_vblank();
}

int zxb_finish(int code)
{
#ifdef ZXB_TEST
    test_exit(code);
#endif
    report_and_wait("0 OK");
    zxb_screen_stop();
    return code;
}

/* --- Errors ------------------------------------------------------------------------ */

static const char *const reports[] = {
    "1 NEXT without FOR", "2 Variable not found", "3 Subscript wrong", "4 Out of memory",
    "5 Out of screen", "6 Number too big", "7 RETURN without GOSUB", "8 End of file",
    "9 STOP statement", "A Invalid argument", "B Integer out of range", "C Nonsense in BASIC",
    "D BREAK - CONT repeats", "E Out of DATA", "F Invalid file name", "G No room for line",
    "H STOP in INPUT", "I FOR without NEXT", "J Invalid I/O device", "K Invalid colour",
    "L BREAK into program", "M RAMTOP no good", "N Statement lost", "O Invalid stream",
    "P FN without DEF", "Q Parameter error", "R Tape loading error",
};

uint32_t zxb_line;                      /* The line running, with --enable-break (0: unknown) */

/* An error report: with the line, if it is known ("2 Variable not found, 20"),
 * on the bottom line until a key is pressed, and back to the BASIC */
static void fail(const char *msg, int test_code)
{
    char buf[64];
    if (zxb_line) {
        int n = 0;
        while (msg[n] && n < 48)
            buf[n] = msg[n], n++;
        buf[n++] = ',', buf[n++] = ' ';
        char d[10];
        int k = 0;
        for (uint32_t v = zxb_line; v; v /= 10)
            d[k++] = (char)('0' + v % 10);
        while (k)
            buf[n++] = d[--k];
        buf[n] = 0;
        msg = buf;
    }
#ifdef ZXB_TEST
    char m[] = "\nERROR ";
    zxb_echo(m, sizeof(m) - 1);
    int len = 0;
    while (msg[len])
        len++;
    zxb_echo(msg, len);
    zxb_echo("\n", 1);
    test_exit(test_code);
#else
    (void)test_code;
#endif
    report_and_wait(msg);
    zxb_screen_stop();
    zxv_report_error(0, msg);
    for (;;)
        ;
}

/* ERROR n: the report n + 1, as ZX Basic */
void zxb_ERROR(int code)
{
    code &= 0xFF;
    if (code < (int)(sizeof(reports) / sizeof(reports[0])))
        fail(reports[code], code + 1);
#ifdef ZXB_TEST
    char m[] = "\nERROR \n";
    zxb_echo(m, sizeof(m) - 1);
    test_exit(code + 1);
#endif
    report_and_wait("0 OK");            /* ERROR 255 */
    zxb_screen_stop();
    zxv_exit_to_basic(0);
}

/* An error with its own report (the ZX-V BASIC ones, such as T File not found) */
void zxb_fail(const char *msg) { fail(msg, 100); }

/* CHECK_BREAK line (--enable-break), after each line: Esc (BREAK) stops the
 * program. zxb_line has the line already: each statement sets it before it runs. */
void zxb_CHECK_BREAK(uint32_t line)
{
    (void)line;
    if ((*(volatile uint8_t *)(uintptr_t)(ZXV_I_KEYMAP + (0x29 >> 3)) >> (0x29 & 7)) & 1)
        zxb_ERROR(20);                  /* L BREAK into program */
}

void zxb_STOP(int code)
{
    (void)code;
    zxb_ERROR(8);
}

/* --- Numbers ------------------------------------------------------------------------ */

int zxb_cmpf(double a, double b)
{
    return a < b ? -1 : a > b ? 1 : a == b ? 0 : 2;
}

/* Float to integer: truncated, as two's complement */
uint32_t zxb_ftoi(double x)
{
    if (x >= 2147483648.0)
        return (uint32_t)x;
    return (uint32_t)(int32_t)x;
}

double zxb_f16tof(int32_t x) { return (double)x / 65536.0; }
int32_t zxb_ftof16(double x) { return (int32_t)(int64_t)(x * 65536.0); }
int32_t zxb_divf16(int32_t a, int32_t b) { return b ? (int32_t)(((int64_t)a << 16) / b) : 0; }
int32_t zxb_modf16(int32_t a, int32_t b) { return b ? a % b : 0; }

double zxb_modf(double a, double b)
{
    if (b == 0)
        return 0;
    return a - b * b_floor(a / b);
}

double zxb_pow(double x, double y)
{
    if (y == 0)
        return 1;
    if (x == 0)
        return 0;
    double r = (double)(int32_t)y;
    if (r == y && r > -1024 && r < 1024) {  /* Integer powers: exact */
        double p = 1, b = x;
        int32_t n = (int32_t)r;
        for (int32_t k = n < 0 ? -n : n; k; k >>= 1, b *= b)
            if (k & 1)
                p *= b;
        return n < 0 ? 1 / p : p;
    }
    if (x < 0)
        zxb_ERROR(ERR_INVALID_ARG);
    return b_exp(y * b_log(x));
}

double zxb_SIN(double x) { return b_sin(x); }
double zxb_COS(double x) { return b_cos(x); }
double zxb_TAN(double x) { return b_tan(x); }
double zxb_ASIN(double x) { return b_asin(x); }
double zxb_ACOS(double x) { return b_acos(x); }
double zxb_ATAN(double x) { return b_atan(x); }
double zxb_EXP(double x) { return b_exp(x); }

double zxb_LN(double x)
{
    if (x <= 0)
        zxb_ERROR(ERR_INVALID_ARG);
    return b_log(x);
}

double zxb_SQRT(double x)
{
    if (x < 0)
        zxb_ERROR(ERR_INVALID_ARG);
    return b_sqrt(x);
}

int zxb_SGNI8(int32_t x) { return x < 0 ? -1 : x > 0; }
int zxb_SGNI16(int32_t x) { return x < 0 ? -1 : x > 0; }
int zxb_SGNI32(int32_t x) { return x < 0 ? -1 : x > 0; }
int zxb_SGNU8(uint32_t x) { return x != 0; }
int zxb_SGNU16(uint32_t x) { return x != 0; }
int zxb_SGNU32(uint32_t x) { return x != 0; }
int zxb_SGNF16(int32_t x) { return x < 0 ? -1 : x > 0; }
int zxb_SGNF(double x) { return x < 0 ? -1 : x > 0; }

/* RND as in the Spectrum: seed = (75 * (seed + 1)) mod 65537 - 1 */
double zxb_RND(void)
{
    seed = (75 * (seed + 1)) % 65537 - 1;
    return seed / 65536.0;
}

void zxb_RANDOMIZE(uint32_t n)
{
    seed = (n ? n : *(volatile uint32_t *)(uintptr_t)ZXV_SV_FRAMES) & 0xFFFF;
}

/* --- Arrays --------------------------------------------------------------------------
 * The array variable: the dimensions table, the data, the LBOUND table (or 0)
 * and the UBOUND table. The dimensions table: number of dimensions - 1 (u16),
 * the size of each one but the first (u16) and the size of an element (u8).
 * The indexes are in the stack, the first one first. Returns the address of
 * the element and, in the high word, the bytes to pop.
 */
uint64_t zxb_array(const uint32_t *arr, const uint32_t *idx)
{
    const uint8_t *dims = (const uint8_t *)(uintptr_t)arr[0];
    const uint8_t *lb = (const uint8_t *)(uintptr_t)arr[2];
    uint32_t n = (uint32_t)(dims[0] | dims[1] << 8) + 1;
    uint32_t off = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t v = idx[i] & 0xFFFF;
        if (lb)
            v -= (uint32_t)(lb[2 * i] | lb[2 * i + 1] << 8);
        off += v;
        if (i + 1 < n)
            off *= (uint32_t)(dims[2 + 2 * i] | dims[3 + 2 * i] << 8);
    }
    uint32_t size = dims[2 + 2 * (n - 1)];
    uint32_t addr = arr[1] + off * size;
    return addr | (uint64_t)(n * 4) << 32;
}

/* A local array, when the function starts: the data are in the heap */
void zxb_local_array(uint32_t *arr, const uint8_t *dims, uint32_t size, const uint8_t *init,
                     const uint8_t *lbound, const uint8_t *ubound)
{
    uint8_t *data = zxb_alloc(size);
    __builtin_memset(data, 0, size);
    if (init)
        __builtin_memcpy(data, init, size);
    arr[0] = (uint32_t)(uintptr_t)dims;
    arr[1] = (uint32_t)(uintptr_t)data;
    arr[2] = (uint32_t)(uintptr_t)lbound;
    if (ubound)
        arr[3] = (uint32_t)(uintptr_t)ubound;
}

/* The end of a function frees its local arrays of strings */
void zxb_ARRAYSTR_FREE_MEM(zstr *data, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
        zxb_free(data[i]);
    zxb_free(data);
}

/* LBOUND (a, n) and UBOUND (a, n); with n = 0, the number of dimensions */
static uint32_t bound(const uint32_t *arr, uint32_t n, int upper)
{
    const uint8_t *dims = (const uint8_t *)(uintptr_t)arr[0];
    uint32_t count = (uint32_t)(dims[0] | dims[1] << 8) + 1;
    n &= 0xFFFF;
    if (n == 0)
        return count;
    if (n > count)
        return 0;
    const uint8_t *table = (const uint8_t *)(uintptr_t)arr[upper ? 3 : 2];
    if (!table)
        return 0;
    return (uint32_t)(table[2 * (n - 1)] | table[2 * (n - 1) + 1] << 8);
}

uint32_t zxb_LBOUND(const uint32_t *arr, uint32_t n) { return bound(arr, n, 0); }
uint32_t zxb_UBOUND(const uint32_t *arr, uint32_t n) { return bound(arr, n, 1); }

/* ON n GO TO / GO SUB: the address of entry n (from 0), or 0 */
uint32_t zxb_on_goto(uint32_t n, const uint32_t *table)
{
    return (n & 0xFF) < table[0] ? table[1 + (n & 0xFF)] : 0;
}

/* --- READ and DATA ---------------------------------------------------------------------
 * Each item: its type (as ZX Basic: 1 string, 2 i8, 3 u8, 4 i16, 5 u16, 6 i32,
 * 7 u32, 8 fixed, 9 float) and its value (a string is a pointer). A 0 ends
 * them, and READ starts again from the first one.
 */
extern const uint8_t __zxb_data0[] __attribute__((weak));
uint64_t __zxb_call_basic(uint32_t func);  /* zxb_core.S */
static const uint8_t *data_ptr;

void zxb_RESTORE(const uint8_t *p)
{
    data_ptr = p;
}

static uint32_t rd(const uint8_t *p, int n)
{
    uint32_t v = 0;
    for (int i = 0; i < n; i++)
        v |= (uint32_t)p[i] << (8 * i);
    return v;
}

uint64_t zxb_READ(int type)
{
    static const uint8_t sizes[] = { 0, 4, 1, 1, 2, 2, 4, 4, 4, 8 };
    if (!data_ptr || !*data_ptr)
        data_ptr = __zxb_data0;
    if (!data_ptr || !*data_ptr)
        zxb_ERROR(13);                  /* E Out of DATA */
    int t = *data_ptr++;
    uint8_t value[8];
    const uint8_t *v = data_ptr;
    if (t & 0x80) {                     /* An expression: a function computes it */
        t &= 0x7F;
        uint64_t r = __zxb_call_basic(rd(data_ptr, 4));
        data_ptr += 4;
        for (int i = 0; i < 8; i++)
            value[i] = (uint8_t)(r >> (8 * i));
        v = value;
        if (t == 1 && type == 1)        /* The function returns a new string */
            return (uint32_t)r;
    } else {
        data_ptr += sizes[t > 9 ? 0 : t];
        if (t == 1 && type == 1)
            return (uint32_t)(uintptr_t)zxb_loadstr((const uint8_t *)(uintptr_t)rd(v, 4));
    }
    if (t < 2 || t > 9 || type == 1)
        zxb_ERROR(ERR_INVALID_ARG);     /* A number for a string or the other way round */

    double x;
    switch (t) {
    case 2: x = (int8_t)v[0]; break;
    case 3: x = v[0]; break;
    case 4: x = (int16_t)rd(v, 2); break;
    case 5: x = rd(v, 2); break;
    case 6: x = (int32_t)rd(v, 4); break;
    case 7: x = rd(v, 4); break;
    case 8: x = (int32_t)rd(v, 4) / 65536.0; break;
    default: {
        union { uint64_t u; double d; } f = { rd(v, 4) | (uint64_t)rd(v + 4, 4) << 32 };
        x = f.d;
    }
    }
    if (type == 9) {
        union { double d; uint64_t u; } f = { x };
        return f.u;
    }
    if (type == 8)
        return (uint32_t)zxb_ftof16(x);
    return zxb_ftoi(x);
}

/* --- Keyboard, time and sound ------------------------------------------------------- */

int zxb_key(void)
{
    int k = zxv_key_get();
    return k < 0 ? 0 : k;
}

static int key_waiting(void)
{
    return *(volatile uint8_t *)(uintptr_t)ZXV_I_KEY_HEAD != *(volatile uint8_t *)(uintptr_t)ZXV_I_KEY_TAIL;
}

/* PAUSE n: n frames (0: for ever) or until a key is pressed */
void zxb_PAUSE(uint32_t n)
{
    n &= 0xFFFF;
    for (uint32_t i = 0; n == 0 || i < n; i++) {
        if (key_waiting())
            break;
        zxv_wait_vblank();
    }
}

/* BEEP duration, pitch: a square wave through the beeper queue */
void zxb_BEEP(double dur, double pitch)
{
    if (pitch < -60 || pitch >= 70 || dur < 0 || dur > 10)
        zxb_ERROR(ERR_INVALID_ARG);
    double f = 261.6255653 * b_exp(pitch / 12.0 * 0.69314718055994530942);
    uint32_t half = (uint32_t)(500000.0 / f + 0.5);
    uint32_t total = (uint32_t)(dur * 1000000.0);
    uint32_t start = zxv_time_us(), t = 0;
    int level = 0;
    while (t + half <= total) {
        level ^= 1;
        zxv_beeper_event(level);
        t += half;
        while (zxv_time_us() - start < t)
            ;
    }
    zxv_beeper_event(0);
}

/* USR address: native code (ilp32), returns a0 */
uint32_t zxb_USR(uint32_t addr)
{
    __asm__ volatile("fence.i" ::: "memory");
    return ((uint32_t(*)(void))(uintptr_t)addr)();
}
