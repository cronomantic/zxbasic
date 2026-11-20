/*
 * core.c - Reconoce el formato de un módulo y lo hace avanzar paso a paso
 * con su reproductor. Sin la API de la máquina (también se compila en el PC).
 */
#include "music_int.h"
#include "zxm_format.h"
#include "zxv_music.h"

static int memcmp_(const uint8_t *a, const char *b, int n)
{
    for (int i = 0; i < n; i++)
        if (a[i] != (uint8_t)b[i])
            return 1;
    return 0;
}

static void copy_text(char *d, const uint8_t *m, uint32_t n, uint32_t at, int len)
{
    int end = 0;
    for (int i = 0; i < len; i++) {
        uint8_t c = mus_b(m, n, at + i);
        d[i] = c >= 32 && c < 127 ? (char)c : ' ';
        if (d[i] != ' ')
            end = i + 1;
    }
    d[end] = 0;
}

int mus_song_init(mus_song_t *s, const uint8_t *m, uint32_t n)
{
    uint32_t n1, n2;
    s->format = ZXV_MUSIC_NONE;
    s->chips = 0;
    if (n >= 8 && !memcmp_(m, ZXM_MAGIC, 8)) {   /* el propio, con su firma: el primero */
        /* Sin su memoria (zxv_music_info), solo qué es y cuántos AY usa. */
        if (s->zxm ? !zxm_init(s->zxm, m, n) : n >= ZXM_HEADER) {
            uint8_t ay = mus_b(m, n, ZXM_H_AY);
            s->format = ZXV_MUSIC_ZXM;
            s->chips = ay == 6 ? 3 : ay == 3 ? 1 : 0;
        }
    } else if ((n1 = (uint32_t)vgm_probe(m, n)) != 0) {    /* VGM o .vgz: también con firma */
        /* Sin su memoria, solo qué es (un .gz puede no ser un VGM, o no
         * tener AY: lo dice vgm_init, al descomprimir su cabecera). */
        if (!s->vgm || !vgm_init(s->vgm, m, n, s->work)) {
            s->format = ZXV_MUSIC_VGM;
            s->chips = (s->vgm ? s->vgm->ays : n1) == 2 ? 3 : 1;
        }
    } else if (pt3_ts_split(m, n, &n1, &n2) && !pt3_init(&s->u.pt3[0], m, n1) &&
        !pt3_init(&s->u.pt3[1], m + n1, n2)) {
        s->format = ZXV_MUSIC_TS;
        s->chips = 3;
    } else if (!pt3_init(&s->u.pt3[0], m, n)) {
        s->format = ZXV_MUSIC_PT3;
        s->chips = 1;
    } else if (!psg_init(&s->u.psg, m, n)) {
        s->format = ZXV_MUSIC_PSG;
        s->chips = 1;
    } else if (!pt2_init(&s->u.pt2, m, n)) {    /* los que no tienen firma, al final */
        s->format = ZXV_MUSIC_PT2;
        s->chips = 1;
    } else if (!stc_init(&s->u.stc, m, n)) {
        s->format = ZXV_MUSIC_STC;
        s->chips = 1;
    } else if (!asc_init(&s->u.asc, m, n)) {
        s->format = ZXV_MUSIC_ASC;
        s->chips = 1;
    }
    return s->format;
}

void mus_song_step(mus_song_t *s, mus_ay_t ay[2])
{
    switch (s->format) {
    case ZXV_MUSIC_ZXM:
        if (s->zxm)
            zxm_step(s->zxm, ay);
        break;
    case ZXV_MUSIC_TS:
        pt3_step(&s->u.pt3[1], &ay[1]);
        /* fallthrough */
    case ZXV_MUSIC_PT3:
        pt3_step(&s->u.pt3[0], &ay[0]);
        break;
    case ZXV_MUSIC_PT2:
        pt2_step(&s->u.pt2, &ay[0]);
        break;
    case ZXV_MUSIC_STC:
        stc_step(&s->u.stc, &ay[0]);
        break;
    case ZXV_MUSIC_ASC:
        asc_step(&s->u.asc, &ay[0]);
        break;
    case ZXV_MUSIC_PSG:
        psg_step(&s->u.psg, &ay[0]);
        break;
    case ZXV_MUSIC_VGM:
        if (s->vgm)
            vgm_step(s->vgm, ay);
        break;
    }
}

void mus_song_move(mus_song_t *s, intptr_t d)
{
    switch (s->format) {
    case ZXV_MUSIC_TS:
        s->u.pt3[1].m += d;
        /* fallthrough */
    case ZXV_MUSIC_PT3:
        s->u.pt3[0].m += d;
        break;
    case ZXV_MUSIC_ZXM:
        if (!s->zxm)
            break;
        if (s->zxm_moves)
            s->zxm = (zxm_t *)(void *)((uint8_t *)s->zxm + d);
        s->zxm->m += d;
        s->zxm->ay[0].m += d;
        s->zxm->ay[1].m += d;
        break;
    case ZXV_MUSIC_PT2:
        s->u.pt2.m += d;
        break;
    case ZXV_MUSIC_STC:
        s->u.stc.m += d;
        break;
    case ZXV_MUSIC_ASC:
        s->u.asc.m += d;
        break;
    case ZXV_MUSIC_PSG:
        s->u.psg.m += d;
        break;
    case ZXV_MUSIC_VGM:
        if (!s->vgm)
            break;
        if (s->zxm_moves) {             /* el estado y la ventana van detrás del módulo */
            s->vgm = (vgm_t *)(void *)((uint8_t *)s->vgm + d);
            if (s->vgm->gz)
                s->vgm->z.win += d;
        }
        s->vgm->m += d;
        if (s->vgm->gz)
            s->vgm->z.in += d;
        break;
    }
}

uint32_t mus_song_step_us(const mus_song_t *s)
{
    if (s->format == ZXV_MUSIC_VGM && s->vgm)
        return s->vgm->step_us;
    return s->format == ZXV_MUSIC_ZXM && s->zxm ? 2500000u / s->zxm->tempo : 20000u;
}

const mus_smp_t *mus_song_samples(const mus_song_t *s)
{
    return s->format == ZXV_MUSIC_ZXM && s->zxm && s->zxm->smp_tracks ? s->zxm->out : 0;
}

int mus_song_looped(const mus_song_t *s)
{
    switch (s->format) {
    case ZXV_MUSIC_ZXM:
        return s->zxm ? s->zxm->looped : 0;
    case ZXV_MUSIC_TS:
    case ZXV_MUSIC_PT3:
        return s->u.pt3[0].looped;
    case ZXV_MUSIC_PT2:
        return s->u.pt2.looped;
    case ZXV_MUSIC_STC:
        return s->u.stc.looped;
    case ZXV_MUSIC_ASC:
        return s->u.asc.looped;
    case ZXV_MUSIC_PSG:
        return s->u.psg.looped;
    case ZXV_MUSIC_VGM:
        return s->vgm ? s->vgm->looped || s->vgm->stopped : 0;
    }
    return 0;
}

int zxv_music_info(const void *data, uint32_t size, zxv_music_info_t *info)
{
    static mus_song_t s;                /* grande para la pila */
    const uint8_t *m = data;
    info->title[0] = info->author[0] = 0;
    info->format = mus_song_init(&s, m, size);
    info->chips = s.chips;
    switch (info->format) {
    case ZXV_MUSIC_ZXM:
        copy_text(info->title, m, size, 0x08, 32);
        copy_text(info->author, m, size, 0x28, 32);
        break;
    case ZXV_MUSIC_PT3:
    case ZXV_MUSIC_TS:
        copy_text(info->title, m, size, 0x1E, 32);
        copy_text(info->author, m, size, 0x42, 32);
        break;
    case ZXV_MUSIC_PT2:
        copy_text(info->title, m, size, 101, 30);
        break;
    case ZXV_MUSIC_STC:
        copy_text(info->title, m, size, 7, 18);
        break;
    case ZXV_MUSIC_VGM: {
        /* El título y el autor de su etiqueta GD3 (en inglés, UTF-16), si la
         * tiene (en un .vgz, sin descomprimirlo, no). */
        uint32_t g = size >= 0x18 && m[0] == 'V' ? mus_w(m, size, 0x14) | (uint32_t)mus_w(m, size, 0x16) << 16 : 0;
        if (g && (g += 0x14) + 12 <= size && !memcmp_(m + g, "Gd3 ", 4)) {
            uint32_t p = g + 12;
            for (int k = 0; k < 7 && p + 1 < size; k++) {
                char *d = k == 0 ? info->title : k == 6 ? info->author : NULL;
                int len = 0;
                for (; p + 1 < size; p += 2) {
                    uint16_t c = mus_w(m, size, p);
                    if (!c)
                        break;
                    if (d && len < 32)
                        d[len++] = (char)(c >= 32 && c < 127 ? c : '?');
                }
                p += 2;
                if (d)
                    d[len] = 0;
            }
        }
        break;
    }
    case ZXV_MUSIC_ASC: {
        static const char id[] = "ASM COMPILATION OF ";
        uint32_t at = s.u.asc.hdr + s.u.asc.npos;
        int ok = at + 63 <= size;
        for (int i = 0; ok && i < 19; i++)
            ok = m[at + i] == id[i];
        if (ok) {
            copy_text(info->title, m, size, at + 19, 20);
            copy_text(info->author, m, size, at + 43, 20);
        }
        break;
    }
    }
    return info->format;
}
