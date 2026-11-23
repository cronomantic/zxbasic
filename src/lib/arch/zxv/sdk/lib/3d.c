/*
 * 3d.c - 3D sencillo: modelos de caras planas con sombreado por tramas
 * (include/zxv_3d.h).
 *
 * Opcional: añade $(SDK)/lib/3d.c y $(SDK)/lib/math.c a tu programa.
 *
 * zxv_3d_add pasa los vértices del modelo a coordenadas de la cámara (una
 * sola matriz: la de la cámara traspuesta por la del objeto), quita las
 * caras que dan la espalda, calcula su tono con la luz, las recorta contra
 * la pirámide de la vista (el plano de cerca y los cuatro lados, con un
 * margen) y guarda sus puntos ya proyectados. zxv_3d_draw las ordena de
 * lejos a cerca por la distancia media de sus vértices y las rellena con
 * zxv_gfx_poly: el color claro donde la trama del tono tiene un 1 y el
 * oscuro en lo demás. Todo con enteros (64 bits en las cuentas largas).
 */
#include <stdint.h>

#include "zxv_3d.h"

#ifdef ZXV_LIB_HOST
/* Las pruebas en el PC (emulator/tests/sdklib/3d_host.c): el motor de la
 * ROM compilado para el PC. */
#include "gfx.h"
#define zxv_gfx_poly gfx_poly
#define zxv_gfx_line gfx_line
#define zxv_gfx_pattern gfx_pattern
#else
#include "zxv_gfx.h"
#endif

#define GUARD       16                  /* píxeles de margen fuera de la vista al recortar */
#define MAX_POINTS  (ZXV_3D_MAX_FACES * 6)    /* de media, 6 por cara (recortadas, alguna más) */

typedef struct {
    int32_t depth;                      /* para ordenar: la z media, 16.16 */
    uint16_t at;                        /* sus puntos en pts[] */
    uint8_t n;                          /* cuántos */
    uint8_t dark, light, level;         /* colores y tono (0 oscuro ... 4 claro) */
} face_t;

static face_t faces[ZXV_3D_MAX_FACES];
static uint16_t order[ZXV_3D_MAX_FACES];
static int16_t pts[MAX_POINTS * 2];
static int nfaces, npts;
static int32_t cv[ZXV_3D_MAX_VERTS][3];  /* los vértices del modelo, en la cámara */

static zxv_camera cam;
static zxv_xform view;                  /* la cámara: su rotación (para trasponer) y su sitio */
static zxv_vec3 light_cam;              /* la luz, unitaria, en la cámara */
static zxv_vec3 light_world = { -0x5555, 0x9D89, -0x5555 };   /* arriba, delante, a la izquierda */
static int ambient = 1;
static int wire = -1;

/* --- Matrices ------------------------------------------------------------------------ */

static zxv_fix fmul(zxv_fix a, zxv_fix b) { return (zxv_fix)(((int64_t)a * b) >> 16); }

void zxv_xform_set(zxv_xform *x, int32_t yaw, int32_t pitch, int32_t roll, zxv_fix px, zxv_fix py, zxv_fix pz)
{
    zxv_fix cy = zxv_cos(yaw), sy = zxv_sin(yaw), cp = zxv_cos(pitch), sp = zxv_sin(pitch);
    zxv_fix cr = zxv_cos(roll), sr = zxv_sin(roll);
    /* Ryaw · Rpitch · Rroll, con
     *   Ryaw   = [c 0 s; 0 1 0; -s 0 c]   (z hacia x: gira a la derecha)
     *   Rpitch = [1 0 0; 0 c s; 0 -s c]   (z hacia y: mira arriba)
     *   Rroll  = [c s 0; -s c 0; 0 0 1]   (y hacia x: se inclina a la derecha) */
    zxv_fix a[3][3] = { { cy, 0, sy }, { 0, ZXV_FIX_ONE, 0 }, { -sy, 0, cy } };
    zxv_fix b[3][3] = { { ZXV_FIX_ONE, 0, 0 }, { 0, cp, sp }, { 0, -sp, cp } };
    zxv_fix c[3][3] = { { cr, sr, 0 }, { -sr, cr, 0 }, { 0, 0, ZXV_FIX_ONE } };
    zxv_fix ab[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            ab[i][j] = fmul(a[i][0], b[0][j]) + fmul(a[i][1], b[1][j]) + fmul(a[i][2], b[2][j]);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            x->m[i][j] = fmul(ab[i][0], c[0][j]) + fmul(ab[i][1], c[1][j]) + fmul(ab[i][2], c[2][j]);
    x->t.x = px;
    x->t.y = py;
    x->t.z = pz;
}

zxv_vec3 zxv_xform_apply(const zxv_xform *x, zxv_vec3 p)
{
    zxv_vec3 r;
    r.x = (zxv_fix)((((int64_t)x->m[0][0] * p.x + (int64_t)x->m[0][1] * p.y + (int64_t)x->m[0][2] * p.z) >> 16) + x->t.x);
    r.y = (zxv_fix)((((int64_t)x->m[1][0] * p.x + (int64_t)x->m[1][1] * p.y + (int64_t)x->m[1][2] * p.z) >> 16) + x->t.y);
    r.z = (zxv_fix)((((int64_t)x->m[2][0] * p.x + (int64_t)x->m[2][1] * p.y + (int64_t)x->m[2][2] * p.z) >> 16) + x->t.z);
    return r;
}

void zxv_xform_mul(zxv_xform *out, const zxv_xform *a, const zxv_xform *b)
{
    zxv_xform r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r.m[i][j] = fmul(a->m[i][0], b->m[0][j]) + fmul(a->m[i][1], b->m[1][j]) + fmul(a->m[i][2], b->m[2][j]);
    r.t = zxv_xform_apply(a, b->t);
    *out = r;
}

/* p en la cámara: la rotación de la cámara traspuesta por (p - su sitio). */
static void to_cam(zxv_fix x, zxv_fix y, zxv_fix z, int64_t out[3])
{
    int64_t dx = (int64_t)x - view.t.x, dy = (int64_t)y - view.t.y, dz = (int64_t)z - view.t.z;
    for (int i = 0; i < 3; i++)
        out[i] = ((int64_t)view.m[0][i] * dx + (int64_t)view.m[1][i] * dy + (int64_t)view.m[2][i] * dz) >> 16;
}

/* Un vector de largo 1 (16.16), con la misma dirección. */
static zxv_vec3 unit(int64_t x, int64_t y, int64_t z)
{
    zxv_vec3 r = { 0, 0, 0 };
    while (x > 0x3FFFFFFF || x < -0x3FFFFFFF || y > 0x3FFFFFFF || y < -0x3FFFFFFF || z > 0x3FFFFFFF ||
           z < -0x3FFFFFFF) {
        x >>= 1;
        y >>= 1;
        z >>= 1;
    }
    uint32_t len = zxv_isqrt64((uint64_t)(x * x) + (uint64_t)(y * y) + (uint64_t)(z * z));
    if (!len)
        return r;
    r.x = (zxv_fix)((x << 16) / len);
    r.y = (zxv_fix)((y << 16) / len);
    r.z = (zxv_fix)((z << 16) / len);
    return r;
}

void zxv_3d_light(zxv_fix x, zxv_fix y, zxv_fix z, int amb)
{
    light_world = unit(x, y, z);
    ambient = amb < 0 ? 0 : amb > 4 ? 4 : amb;
}

void zxv_3d_wireframe(int colour) { wire = colour >= 0 && colour <= 15 ? colour : -1; }

/* --- El frame -------------------------------------------------------------------------- */

void zxv_3d_begin(const zxv_camera *c)
{
    cam = *c;
    if (cam.near <= 0)
        cam.near = 0x4000;
    if (cam.w <= 0)
        cam.w = 256;
    if (cam.h <= 0)
        cam.h = 192;
    if (cam.focal <= 0)
        cam.focal = 128;
    zxv_xform_set(&view, cam.yaw, cam.pitch, cam.roll, cam.pos.x, cam.pos.y, cam.pos.z);
    int64_t l[3];
    for (int i = 0; i < 3; i++)                         /* la luz, girada con la cámara */
        l[i] = ((int64_t)view.m[0][i] * light_world.x + (int64_t)view.m[1][i] * light_world.y +
                (int64_t)view.m[2][i] * light_world.z) >> 16;
    light_cam = unit(l[0], l[1], l[2]);
    nfaces = npts = 0;
}

/* Lo que cuenta un punto para el plano k: >= 0 dentro. */
static int64_t plane(int k, const int64_t *p)
{
    int64_t hw = cam.w / 2 + GUARD, hh = cam.h / 2 + GUARD, f = cam.focal;
    switch (k) {
    case 0: return p[2] - cam.near;
    case 1: return hw * p[2] + f * p[0];            /* izquierda */
    case 2: return hw * p[2] - f * p[0];            /* derecha */
    case 3: return hh * p[2] - f * p[1];            /* arriba */
    default: return hh * p[2] + f * p[1];           /* abajo */
    }
}

/* Recorta el polígono in (n puntos) contra el plano k, en out. */
static int clip(int64_t (*in)[3], int n, int64_t (*out)[3], int k)
{
    int m = 0;
    for (int i = 0; i < n; i++) {
        const int64_t *a = in[i], *b = in[(i + 1) % n];
        int64_t da = plane(k, a), db = plane(k, b);
        if (da >= 0) {
            out[m][0] = a[0];
            out[m][1] = a[1];
            out[m][2] = a[2];
            m++;
        }
        if ((da >= 0) != (db >= 0)) {
            int64_t den = da - db;
            for (int j = 0; j < 3; j++) {
                /* a + (b - a)·da / (da - db), con la fracción en 1/65536: los
                 * valores de los planos no pasan de unos 2^40, así que da·65536
                 * cabe en 64 bits. */
                int64_t t = (da * 65536) / den;
                out[m][j] = a[j] + (((b[j] - a[j]) * t) >> 16);
            }
            m++;
        }
    }
    return m;
}

int zxv_3d_add(const zxv_model *m, const zxv_xform *pos)
{
    if (!m || !m->verts || !m->faces || m->nverts > ZXV_3D_MAX_VERTS)
        return -1;
    zxv_xform id;
    if (!pos) {
        zxv_xform_set(&id, 0, 0, 0, 0, 0, 0);
        pos = &id;
    }
    /* Los vértices, a la cámara. */
    for (int i = 0; i < m->nverts; i++) {
        zxv_vec3 v = { (zxv_fix)(((int64_t)m->verts[i][0] * m->scale)), (zxv_fix)(((int64_t)m->verts[i][1] * m->scale)),
                       (zxv_fix)(((int64_t)m->verts[i][2] * m->scale)) };
        zxv_vec3 w = zxv_xform_apply(pos, v);
        int64_t c[3];
        to_cam(w.x, w.y, w.z, c);
        cv[i][0] = (int32_t)c[0];
        cv[i][1] = (int32_t)c[1];
        cv[i][2] = (int32_t)c[2];
    }
    int seen = 0;
    const uint16_t *f = m->faces;
    for (int k = 0; k < m->nfaces; k++) {
        int n = f[0];
        uint16_t mat = f[1];
        const uint16_t *idx = f + 2;
        f += 2 + n;
        if (n < 3 || n > ZXV_3D_FACE_SIDES)
            return -1;
        int bad = 0;
        for (int i = 0; i < n; i++)
            bad |= idx[i] >= m->nverts;
        if (bad)
            return -1;
        /* La normal, con las aristas reducidas a 15 bits (no se desborda). */
        const int32_t *p0 = cv[idx[0]], *p1 = cv[idx[1]], *p2 = cv[idx[2]];
        int64_t e1[3], e2[3], q[3];
        int64_t big = 0;
        for (int j = 0; j < 3; j++) {
            e1[j] = (int64_t)p1[j] - p0[j];
            e2[j] = (int64_t)p2[j] - p0[j];
            int64_t a = e1[j] < 0 ? -e1[j] : e1[j], b = e2[j] < 0 ? -e2[j] : e2[j];
            big = a > big ? a : big;
            big = b > big ? b : big;
        }
        int sh = 0;
        while ((big >> sh) >= 0x8000)
            sh++;
        for (int j = 0; j < 3; j++) {
            e1[j] >>= sh;
            e2[j] >>= sh;
        }
        int64_t nx = e1[1] * e2[2] - e1[2] * e2[1], ny = e1[2] * e2[0] - e1[0] * e2[2], nz = e1[0] * e2[1] - e1[1] * e2[0];
        /* Por delante si la normal mira a la cámara: n · p0 < 0. */
        int64_t big0 = 0;
        for (int j = 0; j < 3; j++) {
            int64_t a = p0[j] < 0 ? -(int64_t)p0[j] : p0[j];
            big0 = a > big0 ? a : big0;
        }
        int sh0 = 0;
        while ((big0 >> sh0) >= 0x8000)
            sh0++;
        for (int j = 0; j < 3; j++)
            q[j] = (int64_t)p0[j] >> sh0;
        int64_t facing = nx * q[0] + ny * q[1] + nz * q[2];
        if (facing >= 0) {
            if (!(mat & ZXV_3D_TWO_SIDED))
                continue;
            nx = -nx;                                   /* se ve por detrás: la luz, por ese lado */
            ny = -ny;
            nz = -nz;
        }
        /* El tono: el ambiente si no le da la luz, y hasta el claro de cara a ella. */
        int level = 0;
        if (!(mat & ZXV_3D_FLAT)) {
            zxv_vec3 u = unit(nx, ny, nz);
            int64_t d = ((int64_t)u.x * light_cam.x + (int64_t)u.y * light_cam.y + (int64_t)u.z * light_cam.z) >> 16;
            level = ambient;
            if (d > 0)
                level = ambient + (int)((d * (4 - ambient) + 0x8000) >> 16);
            level = level > 4 ? 4 : level;
        }
        /* Recorte contra la vista. */
        int64_t a[ZXV_3D_FACE_SIDES + 6][3], b[ZXV_3D_FACE_SIDES + 6][3];
        int64_t zsum = 0, zmin = INT64_MAX;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < 3; j++)
                a[i][j] = cv[idx[i]][j];
            zsum += cv[idx[i]][2];
            zmin = cv[idx[i]][2] < zmin ? cv[idx[i]][2] : zmin;
        }
        int cnt = n;
        for (int pl = 0; pl < 5 && cnt >= 3; pl++)     /* de a a b, de b a a...: el último, en b */
            cnt = clip(pl & 1 ? b : a, cnt, pl & 1 ? a : b, pl);
        if (cnt < 3)
            continue;
        int64_t (*res)[3] = b;
        if (nfaces >= ZXV_3D_MAX_FACES || npts + cnt > MAX_POINTS)
            return -1;
        face_t *fc = &faces[nfaces];
        fc->depth = (int32_t)(mat & ZXV_3D_DECAL ? zmin - 1 : zsum / n);
        fc->at = (uint16_t)npts;
        fc->n = (uint8_t)cnt;
        fc->dark = (uint8_t)(mat & 15);
        fc->light = (uint8_t)((mat >> 4) & 15);
        fc->level = (uint8_t)level;
        for (int i = 0; i < cnt; i++) {
            int64_t z = res[i][2] > cam.near ? res[i][2] : cam.near;
            pts[2 * npts] = (int16_t)(cam.cx + (res[i][0] * cam.focal) / z);
            pts[2 * npts + 1] = (int16_t)(cam.cy - (res[i][1] * cam.focal) / z);
            npts++;
        }
        nfaces++;
        seen++;
    }
    return seen;
}

/* Las tramas de los tonos 1, 2 y 3: un cuarto, la mitad y tres cuartos del claro. */
static const uint8_t tone[3][8] = {
    { 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22 },
    { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 },
    { 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD },
};

void zxv_3d_draw(void)
{
    /* De lejos a cerca: Shell sort por la distancia, de mayor a menor. */
    for (int i = 0; i < nfaces; i++)
        order[i] = (uint16_t)i;
    for (int gap = nfaces / 2; gap > 0; gap /= 2)
        for (int i = gap; i < nfaces; i++) {
            uint16_t t = order[i];
            int j = i;
            for (; j >= gap && faces[order[j - gap]].depth < faces[t].depth; j -= gap)
                order[j] = order[j - gap];
            order[j] = t;
        }
    for (int i = 0; i < nfaces; i++) {
        const face_t *f = &faces[order[i]];
        const int16_t *p = &pts[2 * f->at];
        if (wire >= 0) {
            for (int k = 0; k < f->n; k++) {
                int k2 = (k + 1) % f->n;
                zxv_gfx_line(p[2 * k], p[2 * k + 1], p[2 * k2], p[2 * k2 + 1], wire);
            }
            continue;
        }
        int lv = f->dark == f->light ? 0 : f->level;
        if (lv == 0 || lv == 4) {
            zxv_gfx_poly(p, f->n, lv == 4 ? f->light : f->dark);
        } else {
            zxv_gfx_pattern(tone[lv - 1], f->dark);
            zxv_gfx_poly(p, f->n, f->light);
            zxv_gfx_pattern(0, 0);
        }
    }
}

int zxv_3d_project(const zxv_camera *c, zxv_vec3 p, int *sx, int *sy, zxv_fix *scale)
{
    zxv_camera save = cam;
    zxv_xform vs = view;
    cam = *c;
    if (cam.near <= 0)
        cam.near = 0x4000;
    if (cam.focal <= 0)
        cam.focal = 128;
    zxv_xform_set(&view, cam.yaw, cam.pitch, cam.roll, cam.pos.x, cam.pos.y, cam.pos.z);
    int64_t q[3];
    to_cam(p.x, p.y, p.z, q);
    int ok = q[2] >= cam.near;
    if (ok) {
        int64_t x = cam.cx + (q[0] * cam.focal) / q[2], y = cam.cy - (q[1] * cam.focal) / q[2];
        ok = x > -0x40000000 && x < 0x40000000 && y > -0x40000000 && y < 0x40000000;
        if (ok) {
            if (sx)
                *sx = (int)x;
            if (sy)
                *sy = (int)y;
            if (scale)
                *scale = (zxv_fix)((((int64_t)cam.focal << 32) / q[2]) > 0x7FFFFFFF ? 0x7FFFFFFF
                                                                                   : (((int64_t)cam.focal << 32) / q[2]));
        }
    }
    cam = save;
    view = vs;
    return ok;
}
