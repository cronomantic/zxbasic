/*
 * m7.c - Objetos sobre el suelo del modo 7 (include/zxv_m7.h).
 *
 * Opcional: añade $(SDK)/lib/m7.c y $(SDK)/lib/math.c a tu programa.
 *
 * Las cuentas de zxv_m7_camera (rom/src/gfx/gfx_m7.c, spec §7.8): la línea
 * i (dy = i - horizonte > 0) está a la distancia z = altura·focal / dy, y
 * cada píxel hacia la derecha avanza z / focal en la dirección perpendicular
 * a la mirada (-sen, cos), con el centro del destino en la cámara. Proyectar
 * es lo contrario: la distancia hacia delante z y la lateral de un punto dan
 * su línea (altura·focal / z) y su x (lateral·focal / z).
 */
#include "zxv_m7.h"

int zxv_m7_floor_at(const zxv_m7cam *cam, int width, int sx, int sy, zxv_fix *px, zxv_fix *py)
{
    if (!cam || cam->focal <= 0)
        return 0;
    int dy = sy - cam->horizon;
    if (dy <= 0)
        return 0;
    /* Exactamente como m7_camera, y luego el píxel sx de su línea. */
    int32_t ca = zxv_cos(cam->angle), sa = zxv_sin(cam->angle);
    int64_t z = (((int64_t)cam->height * cam->focal) >> 16) / dy;
    int64_t step = z * 65536 / cam->focal;
    int64_t fx = (z * ca) >> 16, fy = (z * sa) >> 16;
    int64_t rx = -((step * sa) >> 16), ry = (step * ca) >> 16;
    uint32_t u = (uint32_t)(int32_t)(cam->x + fx - (width / 2) * rx);
    uint32_t v = (uint32_t)(int32_t)(cam->y + fy - (width / 2) * ry);
    u += (uint32_t)sx * (uint32_t)(int32_t)rx;
    v += (uint32_t)sx * (uint32_t)(int32_t)ry;
    if (px)
        *px = (zxv_fix)u;
    if (py)
        *py = (zxv_fix)v;
    return 1;
}

int zxv_m7_project(const zxv_m7cam *cam, int width, zxv_fix px, zxv_fix py, zxv_fix h, zxv_m7pos *p)
{
    if (!cam || !p || cam->focal <= 0)
        return 0;
    int64_t ca = zxv_cos(cam->angle), sa = zxv_sin(cam->angle);
    int64_t dx = (int64_t)px - cam->x, dy = (int64_t)py - cam->y;
    int64_t z = (dx * ca + dy * sa) >> 16;              /* hacia delante, 16.16 */
    int64_t lat = (-dx * sa + dy * ca) >> 16;           /* hacia la derecha, 16.16 */
    if (z < 0x10000)                                    /* detrás o encima de la cámara */
        return 0;
    int64_t f = cam->focal;
    /* Al más cercano: la línea es horizonte + (altura - h)·focal / z. */
    int64_t ly = ((((int64_t)cam->height - h) * f) / z + 0x8000) >> 16;
    int64_t lx = ((lat * f) / z + 0x8000) >> 16;
    int64_t sc = f * 65536 / z;
    if (ly > 0x3FFFFFFF || ly < -0x3FFFFFFF || lx > 0x3FFFFFFF || lx < -0x3FFFFFFF || sc > 0x7FFFFFFF)
        return 0;
    p->x = width / 2 + (int32_t)lx;
    p->y = cam->horizon + (int32_t)ly;
    p->scale = (zxv_fix)sc;
    p->depth = z > 0x7FFFFFFF ? 0x7FFFFFFF : (zxv_fix)z;
    return 1;
}
