/*
 * zxv_3d.h - 3D sencillo: modelos de caras planas con sombreado por tramas
 * (sdk/lib/3d.c, con sdk/lib/math.c).
 *
 * Un modelo son vértices (enteros de 16 bits) y caras de hasta 32 lados, cada
 * una con dos colores: con la luz, la cara se ve del oscuro, del claro o de
 * una mezcla con las tramas de zxv_gfx_pattern (cinco tonos), como en los
 * juegos 3D de los 8 bits. sdk/tools/obj2c.py convierte modelos .obj (los
 * de Blender y casi todos los programas) en C.
 *
 * Coordenadas: x a la derecha, y hacia arriba, z hacia delante (adonde mira
 * la cámara sin girar), en coma fija 16.16 (zxv_math.h). Ángulos de 0x10000
 * por vuelta: yaw gira a la derecha, pitch mira hacia arriba, roll inclina
 * hacia la derecha. Una cara se ve por delante si sus vértices van en el
 * sentido del reloj mirándola desde fuera.
 *
 * Cada frame:
 *
 *   zxv_3d_begin(&cam);
 *   zxv_3d_add(&ship_model, &ship_pos);          // cada objeto en su sitio
 *   zxv_3d_add(&rock_model, &rock_pos);
 *   zxv_3d_draw();                               // de atrás adelante
 *
 * Las caras se ordenan de atrás adelante por su distancia (el algoritmo del
 * pintor): va bien con objetos sencillos; dos caras que se cruzan pueden
 * salir mal, como en los juegos de entonces.
 */
#ifndef ZXV_3D_H
#define ZXV_3D_H

#include <stdint.h>

#include "zxv_math.h"

/* Lo que cabe en un frame (cambia con -D al compilar lib/3d.c): con estos,
 * unos 21 KB de memoria. */
#ifndef ZXV_3D_MAX_FACES
#define ZXV_3D_MAX_FACES    400         /* caras que se ven, entre todos los objetos */
#endif
#ifndef ZXV_3D_MAX_VERTS
#define ZXV_3D_MAX_VERTS    512         /* vértices de un modelo */
#endif
#define ZXV_3D_FACE_SIDES   32          /* lados de una cara, como mucho: recortada contra los 5
                                         * planos de la vista no pasa de 37, y zxv_gfx_poly dibuja 64 */

typedef struct {
    zxv_fix x, y, z;
} zxv_vec3;

/* Una posición: rotación (3 × 3, 16.16) y traslación. */
typedef struct {
    zxv_fix m[3][3];
    zxv_vec3 t;
} zxv_xform;

/* El material de una cara (zxv_model.faces): los colores y lo que hace. */
#define ZXV_3D_MAT(dark, light, flags) ((uint16_t)((dark) | (light) << 4 | (flags)))
#define ZXV_3D_TWO_SIDED    0x0100u     /* se ve por los dos lados */
#define ZXV_3D_FLAT         0x0200u     /* sin luz: siempre del color oscuro */
#define ZXV_3D_DECAL        0x0400u     /* pegada encima de otra (una puerta en una pared): se
                                         * ordena por su vértice más cercano, así va después */

/* Un modelo. faces: por cada cara, el número de vértices (3 a 32), su
 * material (ZXV_3D_MAT) y los índices de sus vértices; una cara tras otra. */
typedef struct {
    const int16_t (*verts)[3];
    const uint16_t *faces;
    uint16_t nverts, nfaces;
    zxv_fix scale;                      /* lo que mide una unidad del modelo */
} zxv_model;

typedef struct {
    zxv_vec3 pos;
    int32_t yaw, pitch, roll;
    int32_t focal;                      /* distancia focal en píxeles (enteros): con 128 y una
                                         * vista de 256, unos 90° de lado a lado */
    int16_t cx, cy;                     /* el centro de la vista en la pantalla */
    int16_t w, h;                       /* lo que mide la vista (para recortar) */
    zxv_fix near;                       /* más cerca no se dibuja (por defecto, 0.25) */
} zxv_camera;

/* Rotación de los ángulos (primero roll, luego pitch, luego yaw) y la
 * posición (x, y, z). */
void zxv_xform_set(zxv_xform *x, int32_t yaw, int32_t pitch, int32_t roll, zxv_fix px, zxv_fix py, zxv_fix pz);
/* El punto p en el mundo (con la escala que se le dé). */
zxv_vec3 zxv_xform_apply(const zxv_xform *x, zxv_vec3 p);
/* a · b: primero b y luego a (un brazo sobre un cuerpo: cuerpo · brazo). */
void zxv_xform_mul(zxv_xform *out, const zxv_xform *a, const zxv_xform *b);

/* La luz: de dónde viene (un vector hacia ella, de cualquier largo) y el
 * tono de las caras que no la reciben, de 0 (el oscuro) a 4 (el claro). Por
 * defecto, de arriba, delante y a la izquierda, con tono 1. */
void zxv_3d_light(zxv_fix x, zxv_fix y, zxv_fix z, int ambient);

/* Líneas en vez de caras: el color de las aristas (-1, caras otra vez). */
void zxv_3d_wireframe(int colour);

/* Empieza un frame con la cámara. */
void zxv_3d_begin(const zxv_camera *cam);
/* Añade un modelo en la posición pos (NULL: en el origen, sin girar).
 * Devuelve las caras que se ven, o -1 si no caben o el modelo no vale. */
int zxv_3d_add(const zxv_model *m, const zxv_xform *pos);
/* Dibuja todo lo añadido, de atrás adelante, en el destino actual. */
void zxv_3d_draw(void);

/* Dónde se ve un punto del mundo: 1 y (sx, sy) si está delante de la
 * cámara; *scale (si no es NULL), los píxeles que mide una unidad allí
 * (16.16). Para poner sprites en el mundo 3D. */
int zxv_3d_project(const zxv_camera *cam, zxv_vec3 p, int *sx, int *sy, zxv_fix *scale);

#endif /* ZXV_3D_H */
