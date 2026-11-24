/*
 * save.c - Guardar los datos de un juego (include/zxv_save.h).
 *
 * Opcional: añade $(SDK)/lib/save.c a tu programa.
 *
 * El fichero: "ZXVS", la versión de los datos (u16), 0 (u16), la longitud
 * (u32), el CRC-32 de los datos (u32) y los datos; todo con el byte bajo
 * primero. Se escribe en name~, se borra name y name~ pasa a llamarse
 * name. Si la máquina se apaga entre medias, al leer queda name~: zxv_load
 * lo prueba cuando name no está o no vale.
 *
 * zxv_load comprueba el CRC antes de tocar data. Hasta SMALL bytes, lee el
 * fichero en la pila y copia; con más no hay sitio para una copia (no hay
 * montón): lo lee una vez para comprobarlo y otra en data, comprobándolo
 * otra vez (y si esa falla, lo intenta de nuevo).
 */
#include "zxv_save.h"

#include "zxv_api.h"

#define HEAD 16
#define PATH_MAX_SAVE 254
#define SMALL 256                       /* hasta aquí, zxv_load lee en la pila: data solo cambia si vale */

static uint32_t crc_more(uint32_t c, const uint8_t *p, uint32_t n)
{
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return c;
}

uint32_t zxv_crc32(const void *data, uint32_t n) { return ~crc_more(0xFFFFFFFFu, data, n); }

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* name y name~ (en tmp): 0 si no caben (el nombre del fichero, hasta 62
 * caracteres para que quepa el ~; la ruta, hasta 254). */
static int tmp_name(const char *name, char *tmp)
{
    int n = 0, last = 0;
    while (name && name[n]) {
        if (name[n] == '/' || name[n] == ':')
            last = n + 1;
        n++;
    }
    if (n == last || n > PATH_MAX_SAVE || n - last > 62)
        return 0;
    for (int i = 0; i < n; i++)
        tmp[i] = name[i];
    tmp[n] = '~';
    tmp[n + 1] = 0;
    return 1;
}

static int write_all(int f, const void *data, uint32_t n)
{
    int r = zxv_file_write(f, data, n);
    if (r < 0)
        return r;
    return (uint32_t)r == n ? ZXV_OK : ZXV_E_NOSPC;
}

int zxv_save(const char *name, uint16_t version, const void *data, uint32_t len)
{
    char tmp[PATH_MAX_SAVE + 2];
    if (!tmp_name(name, tmp) || (!data && len))
        return ZXV_E_NAME;
    uint8_t h[HEAD] = { 'Z', 'X', 'V', 'S', (uint8_t)version, (uint8_t)(version >> 8), 0, 0 };
    put32(h + 8, len);
    put32(h + 12, zxv_crc32(data, len));
    /* Si una vez se apagó entre el borrar y el renombrar, solo queda name~:
     * se recupera antes de escribir otro encima (y de perderlo si ahora
     * también se apaga). */
    zxv_dirent_t e;
    if (zxv_file_stat(name, &e) == ZXV_E_NOENT && zxv_file_stat(tmp, &e) == ZXV_OK)
        zxv_file_rename(tmp, name);
    int f = zxv_file_open(tmp, ZXV_O_WRITE | ZXV_O_CREATE | ZXV_O_TRUNC);
    if (f < 0)
        return f;
    int r = write_all(f, h, HEAD);
    if (r == ZXV_OK && len)
        r = write_all(f, data, len);
    int c = zxv_file_close(f);
    if (r == ZXV_OK)
        r = c < 0 ? c : ZXV_OK;
    if (r != ZXV_OK) {
        zxv_file_remove(tmp);                   /* el de antes sigue como estaba */
        return r;
    }
    r = zxv_file_remove(name);
    if (r < 0 && r != ZXV_E_NOENT)
        return r;
    return zxv_file_rename(tmp, name);
}

/* Lee los len bytes de datos de f (en dst, o solo para comprobarlos si dst
 * es NULL) y comprueba que su CRC es want y que no hay nada más. */
static int read_check(int f, uint8_t *dst, uint32_t len, uint32_t want)
{
    uint32_t crc = 0xFFFFFFFFu, left = len;
    uint8_t buf[64];
    while (left) {
        uint32_t n = left < sizeof buf ? left : (uint32_t)sizeof buf;
        uint8_t *to = dst ? dst : buf;
        int r = zxv_file_read(f, to, n);
        if (r != (int)n)
            return r < 0 ? r : ZXV_SAVE_E_DAMAGED;
        crc = crc_more(crc, to, n);
        if (dst)
            dst += n;
        left -= n;
    }
    return zxv_file_read(f, buf, 1) != 0 || ~crc != want ? ZXV_SAVE_E_DAMAGED : ZXV_OK;
}

/* Lee y comprueba un fichero. data solo cambia si vale, salvo con más de
 * SMALL bytes si la lectura en data falla después de comprobarlo (dos
 * veces): entonces queda a medias. */
static int load_one(const char *name, uint16_t version, void *data, uint32_t len)
{
    int f = zxv_file_open(name, ZXV_O_READ);
    if (f < 0)
        return f;
    uint8_t h[HEAD];
    int r = zxv_file_read(f, h, HEAD);
    if (r != HEAD || h[0] != 'Z' || h[1] != 'X' || h[2] != 'V' || h[3] != 'S') {
        zxv_file_close(f);
        return r < 0 ? r : ZXV_SAVE_E_DAMAGED;
    }
    if ((h[4] | h[5] << 8) != version || get32(h + 8) != len) {
        zxv_file_close(f);
        return ZXV_SAVE_E_VERSION;
    }
    uint32_t want = get32(h + 12);
    if (len <= SMALL) {
        /* En la pila, comprobado; luego a data. */
        uint8_t copy[SMALL];
        r = read_check(f, copy, len, want);
        if (r == ZXV_OK)
            for (uint32_t i = 0; i < len; i++)
                ((uint8_t *)data)[i] = copy[i];
    } else {
        /* Comprobado sin tocar data; luego en data, comprobándolo otra vez
         * (si la tarjeta falla justo ahora o lee otra cosa), y si falla, una
         * vez más. */
        r = read_check(f, NULL, len, want);
        if (r == ZXV_OK)
            for (int tries = 0; tries < 2; tries++) {
                r = zxv_file_seek(f, HEAD, ZXV_SEEK_SET);
                if (r >= 0)
                    r = read_check(f, data, len, want);
                if (r == ZXV_OK)
                    break;
            }
    }
    zxv_file_close(f);
    return r;
}

int zxv_load(const char *name, uint16_t version, void *data, uint32_t len)
{
    char tmp[PATH_MAX_SAVE + 2];
    if (!tmp_name(name, tmp) || (!data && len))
        return ZXV_E_NAME;
    int r = load_one(name, version, data, len);
    if (r == ZXV_OK || r == ZXV_E_NOCARD || r == ZXV_E_OFFLINE)
        return r;
    /* El de verdad no está o no vale: quizá se apagó a medio cambiarlo. Si
     * name~ vale, data queda bien aunque la lectura de name la estropease. */
    int t = load_one(tmp, version, data, len);
    return t == ZXV_OK ? ZXV_OK : r;
}

int zxv_score_add(zxv_score *table, int n, uint32_t score, const char *name)
{
    if (!table || n <= 0)
        return -1;
    int at = 0;
    while (at < n && table[at].score >= score)
        at++;
    if (at == n)
        return -1;
    for (int i = n - 1; i > at; i--)
        table[i] = table[i - 1];
    table[at].score = score;
    int k = 0;
    for (; name && name[k] && k < (int)sizeof(table[at].name) - 1; k++)
        table[at].name[k] = name[k];
    for (; k < (int)sizeof(table[at].name); k++)
        table[at].name[k] = 0;
    return at;
}
