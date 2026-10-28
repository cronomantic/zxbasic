REM Polvo de estrellas: a small game for ZX-V, with zxv.bas.
REM
REM Catch the gems and dodge the rocks. Cursor keys, O and P or a joystick
REM to move; space or fire to start. Everything is drawn on the hidden page
REM and then shown (double buffer), at 50 frames a second.
REM
REM   zxbc --arch zxv estrellas.bas

#include <zxv.bas>

CONST SHIPY AS INTEGER = 178
CONST NOBJ AS UBYTE = 8
CONST NSTARS AS UBYTE = 32

DIM ox(NOBJ - 1) AS INTEGER
DIM oy(NOBJ - 1) AS INTEGER
DIM ov(NOBJ - 1) AS UBYTE                   ' speed
DIM ok(NOBJ - 1) AS UBYTE                   ' sprite (0: none)
DIM sx(NSTARS - 1) AS INTEGER
DIM sy(NSTARS - 1) AS INTEGER
DIM sv(NSTARS - 1) AS UBYTE
DIM scol(3) AS UBYTE => {0, 1, 13, 15}      ' the colour of each speed
DIM px AS INTEGER
DIM score, best AS UINTEGER
DIM lives, pg, i, fx, hurt AS UBYTE

REM The sprites: 0 the ship, 1 a gem, 2 a rock
GRABSTR 0, 16, ".......55.............5995............5995...........555555.........55555555......5555dd5555.....555555555555...22..555555..22..2....2222....2.......2..2......"
GRABSTR 1, 8, "...ee.....eeee...eeffee.eeffffee.eeeeee...eeee.....ee..."
GRABSTR 2, 10, "...3333....33bb3333.3333333b3333b3333333333333333.3333b333...333333.."

FOR i = 0 TO NSTARS - 1
    sx(i) = INT(RND * 256): sy(i) = INT(RND * 192): sv(i) = 1 + INT(RND * 3)
NEXT i

PAPER 0: INK 7: BORDER 0
pg = 1                                      ' the hidden page
DO
    title()
    play()
LOOP

SUB title()
    DO
        frame()
        PRINT AT 8, 7; INK 14; "POLVO DE ESTRELLAS"
        PRINT AT 11, 4; INK 13; "COGE LAS GEMAS, ESQUIVA"
        PRINT AT 12, 4; INK 13; "LAS ROCAS. CURSORES, O Y P"
        PRINT AT 16, 6; INK 15; "ESPACIO O FUEGO: JUGAR"
        show()
    LOOP UNTIL KEY(32) OR (JOY(0) BAND 16)
END SUB

SUB play()
    score = 0: lives = 3: px = 120: hurt = 0
    FOR i = 0 TO NOBJ - 1: ok(i) = 0: NEXT i
    DO
        IF KEY(8) OR KEY(CODE "o") OR (JOY(0) BAND 2) THEN px = px - 3
        IF KEY(9) OR KEY(CODE "p") OR (JOY(0) BAND 1) THEN px = px + 3
        IF px < 0 THEN px = 0
        IF px > 240 THEN px = 240

        REM A new gem or rock, more often the more points
        IF RND * 1000 < 40 + score / 4 THEN
            FOR i = 0 TO NOBJ - 1
                IF ok(i) = 0 THEN
                    ok(i) = 1
                    IF RND < 0.4 THEN ok(i) = 2
                    ox(i) = INT(RND * 240): oy(i) = -10
                    ov(i) = 1 + INT(RND * (2 + score / 200))
                    EXIT FOR
                END IF
            NEXT i
        END IF

        FOR i = 0 TO NOBJ - 1
            IF ok(i) THEN
                oy(i) = oy(i) + ov(i)
                IF oy(i) > 192 THEN
                    ok(i) = 0
                ELSEIF HIT(ok(i), ox(i), oy(i), 0, px, SHIPY) THEN
                    IF ok(i) = 1 THEN
                        score = score + 10
                        SOUND 7, 62: SOUND 0, 60: SOUND 1, 0: SOUND 8, 13: fx = 5
                    ELSE
                        lives = lives - 1: hurt = 25
                        SOUND 7, 55: SOUND 6, 24: SOUND 8, 15: fx = 15
                    END IF
                    ok(i) = 0
                END IF
            END IF
        NEXT i
        IF fx THEN
            fx = fx - 1
            IF fx = 0 THEN SOUND 8, 0
        END IF

        frame()
        FOR i = 0 TO NOBJ - 1
            IF ok(i) THEN SPRITE ok(i), ox(i), oy(i)
        NEXT i
        IF hurt THEN
            hurt = hurt - 1
            SPRITE 0, px, SHIPY, 0, "0a0000000a"
        ELSE
            SPRITE 0, px, SHIPY
        END IF
        PRINT AT 0, 0; INK 15; "PUNTOS "; score; AT 0, 20; "VIDAS "; lives
        show()
    LOOP UNTIL lives = 0

    IF score > best THEN best = score
    SOUND 8, 0
    FOR i = 1 TO 100
        frame()
        PRINT AT 10, 11; INK 10; "SE ACABO"
        PRINT AT 12, 8; INK 15; "PUNTOS "; score
        PRINT AT 13, 8; INK 14; "RECORD "; best
        show()
    NEXT i
END SUB

REM The background on the hidden page: black, and the stars falling
SUB frame()
    SCREEN pg
    BOX 0, 0, 0, 256, 192
    FOR i = 0 TO NSTARS - 1
        sy(i) = sy(i) + sv(i)
        IF sy(i) > 191 THEN sy(i) = 0: sx(i) = INT(RND * 256)
        BOX scol(sv(i)), sx(i), sy(i), 1, sv(i)
    NEXT i
END SUB

REM Shows the page drawn and waits for the next frame
SUB show()
    PAGE pg
    pg = 1 - pg
    WAIT
END SUB
