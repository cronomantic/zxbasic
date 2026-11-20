/*
 * zxm_format.h - El formato de música propio de ZX-V, .zxm (D98,
 * dev/propuesta-musica.md): hasta 6 pistas de los dos AY y 12 de los canales
 * de muestras, con un tempo y una lista de posiciones. Lo leen el
 * reproductor (zxm.c) y lo escriben las herramientas (sdk/tools/music).
 *
 * Todo en little endian; los desplazamientos, desde el principio del fichero.
 *
 * La cabecera (ZXM_HEADER bytes): ver ZXM_H_*. Las secciones, donde dice
 * ZXM_H_OFFSETS (8 u32):
 *
 *   posiciones        cada una, u16 filas (1-256) y un u16 por pista (las
 *                     del AY y luego las de muestras): su patrón, o 0xFFFF
 *   tabla de patrones un u32 por patrón: dónde empieza
 *   instrumentos AY   un u32 por instrumento (dónde empieza) y luego cada uno:
 *                     u8 vuelta, u8 largo y 4 bytes por línea, los de una
 *                     muestra del PT3 (pt3.c)
 *   instrumentos de   ZXM_SINS_SIZE bytes cada uno (ZXM_SI_*)
 *   muestras
 *   ornamentos        un u32 por ornamento y luego cada uno: u8 vuelta, u8
 *                     largo y un i8 (semitonos) por línea
 *   tabla de muestras ZXM_SMP_SIZE bytes cada una (ZXM_S_*)
 *   datos de muestras cada muestra como la quiere el chip (spec §5.6),
 *                     desde un múltiplo de 4; el último u32 de los
 *                     desplazamientos dice cuánto ocupan
 *
 * Un patrón: u8 su clase (0 del AY, 1 de muestras), u16 cuántas filas
 * guarda, y las filas. Más allá de esas, filas vacías. Cada fila:
 *
 *   0x00-0x7F  n + 1 filas vacías
 *   0x80-0xFF  una casilla; los bits 0-6 dicen qué campos siguen, en este
 *              orden (ZXM_C_*): nota (u8: 1-96, o ZXM_NOTE_OFF / _CUT),
 *              instrumento (u8), volumen (u8), ornamento (u8), envolvente
 *              (u8 forma, 1-15, o 0 la quita; con ZXM_ENV_SET_ONLY, la pone
 *              en el AY sin usarla en este canal; u16 periodo), efecto 1 y
 *              efecto 2 (u8 el código, una letra; y su parámetro: 3 bytes
 *              g, p y e, retardo y paso i16; 2 bytes i, encendido y
 *              apagado; 1 byte los demás)
 *
 * Al empezar, cada pista del AY tiene el instrumento 1, el ornamento 0 y el
 * volumen 15, como en el PT3. Los instrumentos del AY y los ornamentos acaban
 * antes de 64 KB desde el principio de los instrumentos del AY (lo normal:
 * caben en unos 47 KB).
 */
#ifndef ZXM_FORMAT_H
#define ZXM_FORMAT_H

#define ZXM_MAGIC       "ZXVMUS01"
#define ZXM_HEADER      136

#define ZXM_H_TITLE     0x08            /* 32 bytes, ASCII con ceros detrás */
#define ZXM_H_AUTHOR    0x28            /* 32 bytes */
#define ZXM_H_AY        0x48            /* u8: pistas del AY, 0, 3 o 6 */
#define ZXM_H_SMP       0x49            /* u8: pistas de muestras, 0-12 */
#define ZXM_H_SPEED     0x4A            /* u8: ticks por fila, 1-255 (0, 256: como el PT3) */
#define ZXM_H_TEMPO     0x4B            /* u8: 32-255 (125: 50 ticks por segundo) */
#define ZXM_H_FLAGS     0x4C            /* u8: ZXM_F_* */
#define ZXM_H_AYTABLE   0x4D            /* u8: la tabla de notas del PT3, 0-3 */
#define ZXM_H_SMPVOL    0x4E            /* u8: SMP_VOLUME, 0-255 */
#define ZXM_H_GLOBAL    0x4F            /* u8: volumen global inicial, 0-64 */
#define ZXM_H_POSITIONS 0x50            /* u16: 1-256 */
#define ZXM_H_LOOP      0x52            /* u16: la posición de vuelta */
#define ZXM_H_PATTERNS  0x54            /* u16 */
#define ZXM_H_AYINS     0x56            /* u8: instrumentos del AY, 0-63 */
#define ZXM_H_SMPINS    0x57            /* u8: instrumentos de muestras, 0-128 */
#define ZXM_H_ORNS      0x58            /* u8: ornamentos, 0-64 */
#define ZXM_H_SAMPLES   0x59            /* u8: muestras, 0-255 */
#define ZXM_H_AY2       0x5A            /* u8: el segundo AY, su tabla (bits 0-1) y su PT3 (bits 4-6) */
#define ZXM_H_PAN       0x5C            /* 12 x u8: panorama inicial de cada pista de muestras */
#define ZXM_H_OFFSETS   0x68            /* 8 x u32: ZXM_O_* */

enum {
    ZXM_O_POSITIONS, ZXM_O_PATTERNS, ZXM_O_AYINS, ZXM_O_SMPINS, ZXM_O_ORNS, ZXM_O_SAMPLES,
    ZXM_O_DATA, ZXM_O_DATA_SIZE,
};

#define ZXM_F_LINEAR    0x01            /* tono lineal en las muestras (si no, periodos del Amiga) */
#define ZXM_F_NOINTERP  0x02            /* las muestras sin interpolar */
#define ZXM_F_S3MCLOCK  0x04            /* el reloj del periodo del S3M y el XM (si no, Amiga PAL) */
#define ZXM_F_ST300     0x08            /* deslizar el volumen también en el primer tick */
#define ZXM_F_PT3VER(f) (((f) >> 4) & 7)  /* el PT3 al que imita el AY: 3.0-3.7 */
#define ZXM_F_MODFX     0x80            /* como el ProTracker: 1, 2, 5, 6 y A con 00 no hacen nada (sin memoria) */

#define ZXM_C_NOTE      0x01
#define ZXM_C_INS       0x02
#define ZXM_C_VOL       0x04
#define ZXM_C_ORN       0x08
#define ZXM_C_ENV       0x10
#define ZXM_C_FX1       0x20
#define ZXM_C_FX2       0x40

#define ZXM_ENV_SET_ONLY 0x80           /* envolvente: la del AY, sin usarla en el canal */

#define ZXM_NOTE_OFF    254             /* soltar */
#define ZXM_NOTE_CUT    255             /* cortar */

/* Lo que ocupa el parámetro de un efecto. */
static inline int zxm_fx_len(int code)
{
    return code == 'g' || code == 'p' || code == 'e' ? 3 : code == 'i' ? 2 : 1;
}

/* Un instrumento de muestras. */
#define ZXM_SINS_SIZE   208
#define ZXM_SI_MAP      0               /* 96 x u8: la muestra de cada nota */
#define ZXM_SI_VENV     96              /* envolvente de volumen: 12 puntos (u16 tick, u16 valor) */
#define ZXM_SI_VENV_N   144             /* u8 puntos, u8 sostener, u8 bucle desde, u8 bucle hasta */
#define ZXM_SI_PENV     148             /* la de panorama, igual */
#define ZXM_SI_PENV_N   196
#define ZXM_SI_FADE     200             /* u16: lo que baja el volumen por tick al soltar (de 65.536) */
#define ZXM_SI_VIBRATO  202             /* u8 forma, barrido, profundidad, velocidad */
#define ZXM_SI_FLAGS    206             /* u8: ZXM_SIF_* */
#define ZXM_SIF_VENV    0x01            /* con envolvente de volumen */
#define ZXM_SIF_VSUS    0x02            /* ... con punto de sostener */
#define ZXM_SIF_VLOOP   0x04            /* ... con bucle */
#define ZXM_SIF_PENV    0x08            /* con envolvente de panorama */
#define ZXM_SIF_PSUS    0x10
#define ZXM_SIF_PLOOP   0x20

/* Una muestra de la tabla. */
#define ZXM_SMP_SIZE    20
#define ZXM_S_START     0               /* u32: en los datos de las muestras, múltiplo de 4 */
#define ZXM_S_LEN       4               /* u32: en muestras */
#define ZXM_S_LOOP      8               /* u32: desde dónde vuelve (len o más: sin bucle) */
#define ZXM_S_FORMAT    12              /* u8: 0 8 bits con signo, 1 ADPCM */
#define ZXM_S_RELNOTE   13              /* i8: nota relativa */
#define ZXM_S_FINE      14              /* i8: afinación, 1/128 de semitono */
#define ZXM_S_VOL       15              /* u8: 0-64 */
#define ZXM_S_PAN       16              /* u8: el panorama, si ZXM_SF_PAN (si no, se queda el de la pista) */
#define ZXM_S_FLAGS     17              /* u8: ZXM_SF_* */
#define ZXM_SF_PAN      0x01            /* la muestra trae su panorama (el XM; el MOD y el S3M, no) */

#endif /* ZXM_FORMAT_H */
