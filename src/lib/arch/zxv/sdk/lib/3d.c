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
 * margen) y guarda sus puntos ya proyectados; las no convexas, partidas en
 * triángulos (recortada, una cara así podría tener muchos más puntos de
 * los que caben). zxv_3d_draw las ordena de
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
#define CLIP_MAX    (ZXV_3D_FACE_SIDES + 16)  /* puntos al recortar: una cara convexa no pasa de
                                               * FACE_SIDES + 5; zxv_gfx_poly dibuja 64 */
#define FOCAL_MAX   0x7FFF              /* más, las cuentas de los planos se desbordarían */

typedef struct {
    int64_t depth;                      /* para ordenar: la z media, 16.16 */
    uint16_t at;                        /* sus puntos en pts[] */
    uint8_t n;                          /* cuántos */
    uint8_t dark, light, level;         /* colores y tono (0 oscuro ... 4 claro) */
} face_t;

static face_t faces[ZXV_3D_MAX_FACES];
static uint16_t order[ZXV_3D_MAX_FACES];
static int16_t pts[MAX_POINTS * 2];
static uint8_t edges[(MAX_POINTS + 7) / 8];     /* bit a 1: el lado que sale de ese punto es un borde
                                                 * de la cara (con líneas, solo esos) */
static int nfaces, npts;
static int64_t cv[ZXV_3D_MAX_VERTS][3];  /* los vértices del modelo, en la cámara */

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
    r.x = (zxv_fix)(x * 65536 / len);                   /* x * 65536, no x << 16: x puede ser negativo */
    r.y = (zxv_fix)(y * 65536 / len);
    r.z = (zxv_fix)(z * 65536 / len);
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
    if (cam.focal > FOCAL_MAX)
        cam.focal = FOCAL_MAX;
    zxv_xform_set(&view, cam.yaw, cam.pitch, cam.roll, cam.pos.x, cam.pos.y, cam.pos.z);
    int64_t l[3];
    for (int i = 0; i < 3; i++)                         /* la luz, girada con la cámara */
        l[i] = ((int64_t)view.m[0][i] * light_world.x + (int64_t)view.m[1][i] * light_world.y +
                (int64_t)view.m[2][i] * light_world.z) >> 16;
    light_cam = unit(l[0], l[1], l[2]);
    nfaces = npts = 0;
}

/* Lo que cuenta un punto para el plano k: >= 0 dentro. Con las coordenadas
 * de la cámara (hasta unos 2^37), la focal hasta FOCAL_MAX y la vista de 16
 * bits, no pasa de unos 2^53. */
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

/* a + (b - a) · num / den, con 0 <= num < den: donde cruza un plano el lado
 * de a (dentro) a b (fuera). La fracción, en 1/2^30; b - a (hasta unos
 * 2^38), en dos trozos para que el producto no se desborde. */
static int64_t towards(int64_t a, int64_t b, int64_t num, int64_t den)
{
    while (den >= ((int64_t)1 << 31)) {
        den >>= 1;
        num >>= 1;
    }
    int64_t t = den > 0 ? num * ((int64_t)1 << 30) / den : 0;
    int64_t d = b - a, hi = d >> 16, lo = d & 0xFFFF;
    return a + ((hi * t) >> 14) + ((lo * t) >> 30);
}

/* Recorta el polígono in (n puntos; ei[i]: el lado de i a i + 1 es un borde
 * de la cara) contra el plano k, en out y eo. Devuelve cuántos puntos
 * quedan, o -1 si pasarían de CLIP_MAX. Cada cruce se calcula desde el punto
 * de dentro: el lado que comparten dos triángulos de una cara queda igual
 * en los dos, sin rendijas entre ellos. */
static int clip(int64_t (*in)[3], const uint8_t *ei, int n, int64_t (*out)[3], uint8_t *eo, int k)
{
    int m = 0;
    for (int i = 0; i < n; i++) {
        const int64_t *a = in[i], *b = in[i + 1 < n ? i + 1 : 0];
        int64_t da = plane(k, a), db = plane(k, b);
        if (da >= 0) {
            if (m >= CLIP_MAX)
                return -1;
            out[m][0] = a[0];
            out[m][1] = a[1];
            out[m][2] = a[2];
            eo[m++] = ei[i];
        }
        if ((da >= 0) != (db >= 0)) {
            if (m >= CLIP_MAX)
                return -1;
            const int64_t *p = da >= 0 ? a : b, *q = da >= 0 ? b : a;
            int64_t dp = da >= 0 ? da : db, dq = da >= 0 ? db : da;      /* dp >= 0 > dq */
            for (int j = 0; j < 3; j++)
                out[m][j] = towards(p[j], q[j], dp, dp - dq);
            /* Al salir, lo que sigue va por el plano: no es un borde de la
             * cara (con líneas no se dibuja). Al entrar, sigue el lado. */
            eo[m++] = da >= 0 ? 0 : ei[i];
        }
    }
    return m;
}

/* Recorta un polígono de una cara (a: n puntos de la cámara, en un búfer de
 * CLIP_MAX; ea, sus bordes) y lo guarda proyectado, con lo demás de proto.
 * 1 si queda algo en la vista, 0 si no, -1 si no cabe en las tablas del
 * frame y -2 si al recortarlo pasaría de CLIP_MAX puntos (con una cara
 * convexa no pasa de n + 5; un triángulo, de 19). */
static int put(int64_t (*a)[3], uint8_t *ea, int n, const face_t *proto)
{
    int64_t b[CLIP_MAX][3];
    uint8_t eb[CLIP_MAX];
    int cnt = n;
    for (int pl = 0; pl < 5 && cnt >= 3; pl++) {     /* de a a b, de b a a...: el último, en b */
        cnt = pl & 1 ? clip(b, eb, cnt, a, ea, pl) : clip(a, ea, cnt, b, eb, pl);
        if (cnt < 0)
            return -2;
    }
    if (cnt < 3)
        return 0;
    if (nfaces >= ZXV_3D_MAX_FACES || npts + cnt > MAX_POINTS)
        return -1;
    face_t *fc = &faces[nfaces++];
    *fc = *proto;
    fc->at = (uint16_t)npts;
    fc->n = (uint8_t)cnt;
    for (int i = 0; i < cnt; i++) {
        int64_t z = b[i][2] > cam.near ? b[i][2] : cam.near;
        pts[2 * npts] = (int16_t)(cam.cx + (b[i][0] * cam.focal) / z);
        pts[2 * npts + 1] = (int16_t)(cam.cy - (b[i][1] * cam.focal) / z);
        if (eb[i])
            edges[npts >> 3] |= (uint8_t)(1u << (npts & 7));
        else
            edges[npts >> 3] &= (uint8_t)~(1u << (npts & 7));
        npts++;
    }
    return 1;
}

/* --- Caras no convexas --------------------------------------------------------------- */

/* Los vértices de una cara (respecto al primero, reducidos a 15 bits) vistos
 * en el plano en que más se ven: sin el eje de la componente más larga de
 * la normal. u y v son los otros dos en orden, así el producto vectorial en
 * 2D es esa componente; s, su signo (0 si la cara no tiene área). */
typedef struct {
    int32_t (*d)[3];
    int u, v, s;
} flat_t;

static int64_t cross2(const flat_t *f, int ia, int ib, int ic)
{
    const int32_t *a = f->d[ia], *b = f->d[ib], *c = f->d[ic];
    return (((int64_t)b[f->u] - a[f->u]) * ((int64_t)c[f->v] - a[f->v]) -
            ((int64_t)b[f->v] - a[f->v]) * ((int64_t)c[f->u] - a[f->u])) * f->s;
}

/* ¿Convexa? Todas las vueltas hacia el mismo lado y una sola vuelta entera:
 * la u va y vuelve una vez (una estrella de cinco puntas gira siempre hacia
 * el mismo lado, pero da dos vueltas). Sin área (s 0), como convexa: no se
 * ve, y si al recortarla saliera de más, se parte igual. */
static int is_convex(const flat_t *f, int n)
{
    int first = 0, last = 0, changes = 0;
    if (!f->s)
        return 1;
    for (int i = 0; i < n; i++) {
        int ia = i ? i - 1 : n - 1, ic = i + 1 < n ? i + 1 : 0;
        if (cross2(f, ia, i, ic) < 0)
            return 0;
        int64_t du = (int64_t)f->d[ic][f->u] - f->d[i][f->u];
        int sg = du > 0 ? 1 : du < 0 ? -1 : 0;
        if (sg) {
            if (!first)
                first = sg;
            else if (sg != last)
                changes++;
            last = sg;
        }
    }
    if (first && first != last)
        changes++;
    return changes <= 2;
}

/* ¿p dentro del triángulo abc o en su borde? (Un vértice repetido en el
 * mismo sitio que uno de los tres, no.) */
static int in_tri(const flat_t *f, int ip, int ia, int ib, int ic)
{
    const int32_t *p = f->d[ip];
    for (int k = 0; k < 3; k++) {
        const int32_t *q = f->d[k == 0 ? ia : k == 1 ? ib : ic];
        if (p[0] == q[0] && p[1] == q[1] && p[2] == q[2])
            return 0;
    }
    return cross2(f, ia, ib, ip) >= 0 && cross2(f, ib, ic, ip) >= 0 && cross2(f, ic, ia, ip) >= 0;
}

/* Parte en triángulos una cara de n vértices, cortando orejas (un vértice
 * convexo sin otro dentro de su triángulo). tri, los vértices de cada uno,
 * en el orden de la cara; te, si cada lado es un borde de la cara (y no una
 * diagonal). Devuelve cuántos. Con una cara que se cruza consigo misma,
 * cuando no hay oreja se corta por un vértice cualquiera: termina siempre. */
static int triangulate(const flat_t *f, int n, uint8_t (*tri)[3], uint8_t (*te)[3])
{
    uint8_t w[ZXV_3D_FACE_SIDES], e[ZXV_3D_FACE_SIDES];   /* los que quedan; e[i]: el lado de w[i] a w[i + 1] */
    int m = n, nt = 0;
    for (int i = 0; i < n; i++) {
        w[i] = (uint8_t)i;
        e[i] = 1;
    }
    while (m >= 3) {
        int pick = -1, any = -1;
        for (int i = 0; i < m && pick < 0; i++) {
            int ia = w[i ? i - 1 : m - 1], ib = w[i], ic = w[i + 1 < m ? i + 1 : 0];
            int64_t cr = cross2(f, ia, ib, ic);
            if (cr < 0)
                continue;
            any = any < 0 ? i : any;
            if (cr == 0 || m == 3) {                    /* en línea: se quita sin triángulo */
                pick = i;
                break;
            }
            int ok = 1;
            for (int k = 0; k < m && ok; k++)
                if (w[k] != ia && w[k] != ib && w[k] != ic)
                    ok = !in_tri(f, w[k], ia, ib, ic);
            if (ok)
                pick = i;
        }
        if (pick < 0)
            pick = any >= 0 ? any : 0;
        int pi = pick ? pick - 1 : m - 1, ni = pick + 1 < m ? pick + 1 : 0;
        int64_t cr = cross2(f, w[pi], w[pick], w[ni]);
        if (cr) {
            tri[nt][0] = w[pi];
            tri[nt][1] = w[pick];
            tri[nt][2] = w[ni];
            te[nt][0] = e[pi];
            te[nt][1] = e[pick];
            te[nt][2] = m == 3 ? e[ni] : 0;
            nt++;
        }
        if (m == 3)
            break;
        /* El lado nuevo, de w[pi] a w[ni]: una diagonal, o el mismo borde si
         * el vértice estaba en línea. */
        e[pi] = cr ? 0 : (uint8_t)(e[pi] & e[pick]);
        for (int k = pick; k + 1 < m; k++) {
            w[k] = w[k + 1];
            e[k] = e[k + 1];
        }
        m--;
    }
    return nt;
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
    /* Los vértices, a la cámara: en 64 bits, como zxv_3d_project (el mundo
     * mide ±32768 unidades, y de un extremo a otro hay más). */
    for (int i = 0; i < m->nverts; i++) {
        zxv_vec3 v = { (zxv_fix)(((int64_t)m->verts[i][0] * m->scale)), (zxv_fix)(((int64_t)m->verts[i][1] * m->scale)),
                       (zxv_fix)(((int64_t)m->verts[i][2] * m->scale)) };
        zxv_vec3 w = zxv_xform_apply(pos, v);
        to_cam(w.x, w.y, w.z, cv[i]);
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
        /* Los vértices respecto al primero, reducidos a 15 bits (así no se
         * desborda nada): para la normal y para ver si es convexa. */
        const int64_t *p0 = cv[idx[0]];
        int32_t d[ZXV_3D_FACE_SIDES][3];
        int64_t big = 0;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < 3; j++) {
                int64_t a = cv[idx[i]][j] - p0[j];
                a = a < 0 ? -a : a;
                big = a > big ? a : big;
            }
        int sh = 0;
        while ((big >> sh) >= 0x8000)
            sh++;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < 3; j++)
                d[i][j] = (int32_t)((cv[idx[i]][j] - p0[j]) >> sh);
        /* La normal de toda la cara (Newell: la suma de d[i] × d[i + 1]):
         * vale con vértices en línea y con caras no convexas, en las que los
         * tres primeros pueden dar la vuelta. */
        int64_t nv[3] = { 0, 0, 0 };
        for (int i = 0; i < n; i++) {
            const int32_t *a = d[i], *b = d[i + 1 < n ? i + 1 : 0];
            nv[0] += (int64_t)a[1] * b[2] - (int64_t)a[2] * b[1];
            nv[1] += (int64_t)a[2] * b[0] - (int64_t)a[0] * b[2];
            nv[2] += (int64_t)a[0] * b[1] - (int64_t)a[1] * b[0];
        }
        /* Por delante si la normal mira a la cámara: n · p0 < 0. */
        int64_t big0 = 0, q[3];
        for (int j = 0; j < 3; j++) {
            int64_t a = p0[j] < 0 ? -p0[j] : p0[j];
            big0 = a > big0 ? a : big0;
        }
        int sh0 = 0;
        while ((big0 >> sh0) >= 0x8000)
            sh0++;
        for (int j = 0; j < 3; j++)
            q[j] = p0[j] >> sh0;
        int64_t facing = nv[0] * q[0] + nv[1] * q[1] + nv[2] * q[2];
        if (facing >= 0 && !(mat & ZXV_3D_TWO_SIDED))
            continue;
        /* La cara en el plano en que más se ve: para ver si es convexa y
         * partirla. */
        int64_t ax = nv[0] < 0 ? -nv[0] : nv[0], ay = nv[1] < 0 ? -nv[1] : nv[1], az = nv[2] < 0 ? -nv[2] : nv[2];
        int drop = ax >= ay && ax >= az ? 0 : ay >= az ? 1 : 2;
        flat_t fl = { d, (drop + 1) % 3, (drop + 2) % 3, nv[drop] > 0 ? 1 : nv[drop] < 0 ? -1 : 0 };
        if (facing >= 0) {                              /* se ve por detrás: la luz, por ese lado */
            nv[0] = -nv[0];
            nv[1] = -nv[1];
            nv[2] = -nv[2];
        }
        /* El tono: el ambiente si no le da la luz, y hasta el claro de cara a ella. */
        int level = 0;
        if (!(mat & ZXV_3D_FLAT)) {
            zxv_vec3 u = unit(nv[0], nv[1], nv[2]);
            int64_t dl = ((int64_t)u.x * light_cam.x + (int64_t)u.y * light_cam.y + (int64_t)u.z * light_cam.z) >> 16;
            level = ambient;
            if (dl > 0)
                level = ambient + (int)((dl * (4 - ambient) + 0x8000) >> 16);
            level = level > 4 ? 4 : level;
        }
        int64_t zsum = 0, zmin = INT64_MAX;
        for (int i = 0; i < n; i++) {
            zsum += cv[idx[i]][2];
            zmin = cv[idx[i]][2] < zmin ? cv[idx[i]][2] : zmin;
        }
        face_t proto = { mat & ZXV_3D_DECAL ? zmin - 1 : zsum / n, 0, 0, (uint8_t)(mat & 15),
                         (uint8_t)((mat >> 4) & 15), (uint8_t)level };
        /* Recorte contra la vista: entera si es convexa; si no (o si por el
         * redondeo saliera de más), en triángulos, con el tono y la
         * distancia de la cara, y que con líneas solo dibujan sus bordes. */
        int64_t a[CLIP_MAX][3];
        uint8_t ea[CLIP_MAX];
        int r = -2;
        if (is_convex(&fl, n)) {
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < 3; j++)
                    a[i][j] = cv[idx[i]][j];
                ea[i] = 1;
            }
            r = put(a, ea, n, &proto);
        }
        if (r == -2) {
            uint8_t tri[ZXV_3D_FACE_SIDES - 2][3], te[ZXV_3D_FACE_SIDES - 2][3];
            int nt = triangulate(&fl, n, tri, te);
            r = 0;
            for (int t = 0; t < nt; t++) {
                for (int c = 0; c < 3; c++) {
                    for (int j = 0; j < 3; j++)
                        a[c][j] = cv[idx[tri[t][c]]][j];
                    ea[c] = te[t][c];
                }
                int s = put(a, ea, 3, &proto);
                if (s == -1)
                    return -1;
                r |= s > 0;
            }
        }
        if (r < 0)
            return -1;
        seen += r;
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
                int k2 = (k + 1) % f->n, at = f->at + k;
                if (edges[at >> 3] >> (at & 7) & 1)     /* no las diagonales de las caras partidas */
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
    if (cam.focal > FOCAL_MAX)
        cam.focal = FOCAL_MAX;
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
