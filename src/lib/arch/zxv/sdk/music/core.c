/*
 * core.c - Reconoce el formato de un módulo y lo hace avanzar paso a paso
 * con su reproductor. Sin la API de la máquina (también se compila en el PC).
 */
#include "music_int.h"
#include "zxv_music.h"

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
    if (pt3_ts_split(m, n, &n1, &n2) && !pt3_init(&s->u.pt3[0], m, n1) &&
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
    }
}

int mus_song_looped(const mus_song_t *s)
{
    switch (s->format) {
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
