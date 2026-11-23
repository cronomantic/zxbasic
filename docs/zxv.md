# ZX-V target (prototype)

`--arch=zxv` compiles for [ZX-V](https://github.com/cronomantic/ZX-V), a 32-bit
RISC-V machine (RV32IMAC at 144 MHz) with a ZX Spectrum style BASIC. The output
is a `.zxv` native program that its BASIC loads with `LOAD "name"`.

```sh
python3 zxbc.py --arch=zxv program.bas    # program.zxv
python3 zxbc.py --arch=zxv -f asm program.bas   # only the assembler (GNU as)
python3 zxbc.py --arch=zxv --loading-screen title.png program.bas   # with a loading screen
python3 zxbc.py --arch=zxv --embed song.pt3 --embed fx.afb game.bas   # with its files inside
```

It only needs the RISC-V GCC toolchain (`riscv64-unknown-elf-gcc`; `ZXV_CROSS`
changes the prefix), the one of the ZX-V SDK. The parts of that SDK the programs
are linked with, and the font of the machine, are in `src/lib/arch/zxv/sdk` and
`src/lib/arch/zxv/runtime` (`tools/zxv_sdk_sync.py PATH/TO/ZX-V` updates them).

## How it works

The front end and the translator to intermediate code are the Z80 ones. The arch
changes:

| | Z80 | ZX-V |
|---|---|---|
| Pointers (`@x`, strings, `PEEK`/`POKE` addresses) | 16 bits | 32 bits |
| `Float` | 5 bytes (ZX Spectrum ROM) | IEEE double (8 bytes) |
| Parameters | 2-byte slots | 4-byte slots (doubles 8) |
| Local variables | packed | aligned (RISC-V does not allow misaligned accesses) |

The backend (`src/arch/zxv/backend`) keeps the model of the Z80 one, so the
translator did not need to change: temporaries live in the stack (the second
operand on top), BASIC functions and `GO SUB` keep the return address in the stack,
`s0` is the frame pointer. It emits GNU assembler, which is linked with the
runtime and the ZX-V SDK, and converted to `.zxv` with `elf2zxv.py`.

The runtime (`src/lib/arch/zxv/runtime`) is in C. Its functions get the `fparam`
values first and then the stack parameters, with the ilp32 ABI.

## Memory below 64K

Addresses below 65536 are emulated, as the ZX-V BASIC does, so programs written for
the Spectrum keep working: the screen (16384) and its attributes (22528) are shown
in mode 0, the font is at 15616, and the system variables and the rest (23296-65535)
are a block of memory, with some variables updated: `FRAMES`, `CHARS`, `UDG`,
`COORDS`, `ATTR P`, `ATTR T`, `BORDCR`. `DIM x AS ... AT address` works there too.

ZX-V draws 60 frames a second, but the Spectrum things keep their time: `PAUSE n`
waits n fiftieths of a second, `FRAMES` (23672) counts fiftieths and `FLASH`
blinks every 16/50 of a second. `TIME` and `WAIT` (in `zxv.bas`) count ZX-V
frames, 60 a second.

Above 64K the addresses are real: the ZX-V RAM, its VRAM and its registers.

## Status

Works and is tested (`tests/zxv/run.py`):

- All the integer types, `Fixed`, `Float` and strings, with the same types and
  conversions as the Z80 target.
- `FOR`, `WHILE`, `DO`, `IF`, `GO TO`, `GO SUB`, `ON ... GO TO / GO SUB`.
- `SUB` and `FUNCTION`, by value and by reference, locals, recursion.
- Strings: slices, `LET a$(x TO y)`, `CHR$`, `CODE`, `STR$`, `VAL`, `LEN`.
- Arrays of any type and dimensions, with bounds and initial values; global,
  local (in the heap, freed when the function ends) and as parameters, with
  `LBOUND` and `UBOUND`; copying them (`a = b`, also arrays of strings).
- Math functions (the same results as the ZX-V BASIC), `RND`, `RANDOMIZE`.
- `READ`, `DATA` (constants and expressions) and `RESTORE`.
- `PRINT` with `AT`, `TAB`, commas, colours, `FLASH` (it blinks every 16/50 of a
  second, as in the Spectrum), `INVERSE`, `OVER`, UDG and block
  graphics; `CLS`, `BORDER`, `PLOT`, `DRAW` (with arcs), `CIRCLE`, `BEEP`, `PAUSE`,
  `INKEY$`, `USR`, `PEEK` and `POKE`; `IN` and `OUT` with the Spectrum ports:
  the keyboard (`xxFEh`, the ZX-V matrix is the Spectrum one), the Kempston
  joystick (`1Fh`) and the border (`OUT 254`).
- `SAVE`, `LOAD` and `VERIFY` with `CODE`, `SCREEN$` and `DATA` (below).
- With `--enable-break`, Esc (BREAK) stops the program (`L BREAK into program`)
  and the error reports say the line: `9 STOP statement, 12`.
- The libraries below.
- When the program ends, its report (`0 OK`, or the error) stays on the bottom
  line until a key is pressed, as in the Spectrum: the ZX-V BASIC starts again and
  clears the screen.

Speed: a `FOR` loop of a million turns with `LET s = s + i * 2 + 1` takes 0.34
seconds (about 20 frames), about 90 times the ZX-V BASIC.

Not yet:

- The libraries that are not in the list below.
- `BOLD`, `ITALIC`.
- `FASTCALL` functions with parameters written in BASIC (their first parameter
  comes in `a0`, for `ASM` blocks).
- Optimization of the generated code (the `-O` levels only change the front end).

## Loading screen

`--loading-screen FILE` (only with `--arch=zxv`) puts a loading screen in the
`.zxv`, as ZX-V's `elf2zxv.py --screen` does: a PNG of 256×192 (mode 0) or
512×384 (mode 1), or the whole frame with its border, 320×240 or 640×480 (the
border takes the colour of the top left corner); or the first screen of a `.gfx`
(`png2gfx.py --screen`, zxv-gfx, or `SAVE "x" SCREEN$`), with its palette if it
has one. A PNG keeps its colours, as `png2gfx.py --screen` does (ZX-V decision
D90): if they are all standard ZX-V colours, the screen has no palette; if not,
it can have up to 16, and carries a palette with them (the standard ones at
their own index, the others in the free ones; more than 16 is an error).

The ROM shows it while it loads the program. A mode 0 screen, the mode of the
programs, stays when the program starts, with its border: the runtime does not
clear it, and `PRINT`, `PLOT` and the rest draw over it (the Spectrum screen,
the one `PEEK`, `SCREEN$`, `ATTR` and `POINT` read, starts empty). Then `CLS`
clears it. A mode 1 screen is only seen while loading.

A screen with a palette is shown with it: the ROM puts it in table 0, so the
program starts with those colours (`PRINT` too), until `MODE` or `PALETTERESET`
(`zxv.bas`) brings the standard ones back. With a mode 1 screen, cleared when the
program starts, its colours go with it.

## Files

`SAVE`, `LOAD` and `VERIFY` work on the SD card of ZX-V (or the emulator's card
folder), as in its BASIC: a name without extension gets `.gfx` for the screen
and `.bin` for the rest (`CODE` and `DATA`). `LOAD` without extension tries that
one and then `.tap`. The names can start with a drive, as in the ZX-V BASIC:
`ROM:` (read only) and `EXT:`, the folder served by another computer over the
board's UART, or by the emulator (`--ext`, File > EXT: drive). Without a server
for `EXT:`, the program stops with `V Disk error (no server for EXT:)`.

- `SCREEN$` in a `.gfx` is the ZX-V screen shown, as its BASIC saves it: a
  graphics file with a screen object (the mode, the border colour, the colours
  if they are not the standard ones, and the pixels of the page shown, 4 bits
  each). The colours are the 16 as they are seen, with `REMAP` and `PALETTE`.
  `LOAD` puts it back, with its mode (if it is not the current one, as `MODE` of
  `zxv.bas` does, which clears both pages), its border and its colours (in
  table 0, with the `REMAP` changes gone); `VERIFY` compares it with the
  screen, its colours too. The Spectrum screen, the one `PEEK`, `SCREEN$`,
  `ATTR` and `POINT` read, does not change.
- Any other name gets the bytes as they are. `SCREEN$` in a `.scr` is the
  Spectrum screen, its 6912 bytes.
- A name ending in `.tap` is a Spectrum tape, with a `CODE` header and the data,
  for Spectrum emulators.

```basic
SAVE "screen" SCREEN$              ' screen.gfx, the ZX-V screen
SAVE "screen.scr" SCREEN$          ' the Spectrum screen, 6912 bytes
SAVE "level" CODE @map(0), 512     ' level.bin, 512 raw bytes
LOAD "level" CODE @map(0)          ' raw bytes need the address
LOAD "screen" SCREEN$
VERIFY "screen" SCREEN$
SAVE "level.tap" CODE @map(0), 512 ' a Spectrum tape
LOAD "level.tap" CODE              ' back where it was saved
```

ZX Basic turns `SCREEN$` into `CODE 16384, 6912`, so that block goes in a `.gfx`
too when the name has no extension or ends in `.gfx`.

A `.bin` does not know where it goes, so `LOAD ... CODE` needs the address; a
`.tap` keeps it, and loads the block where it was saved. A tape header has 16
bits for the address: above 64K (the ZX-V memory), its second parameter, 32768
for `CODE` in the Spectrum, keeps the high half; and a tape block is 64 KB at
most. `DATA` saves the memory of a variable (`SAVE "x" DATA v`), or all of them
without a name; as in the Z80 target, an array needs `CODE`. A missing file
stops the program with `T File not found`, as in the ZX-V BASIC.

## Files inside the program

`--embed FILE` (only with `--arch=zxv`, and as many times as needed) puts a file
inside the `.zxv`, as the C games of ZX-V carry their graphics and music (ZX-V
decision D82). `LOAD`, `VERIFY`, `MUSICLOAD` and `SFXLOAD` look first in the
current folder and, if the file is not there (or there is no card), take the
one inside with that name. So the `.zxv` works alone, also from the `ROM:`
drive, and a file with the same name on the card takes the place of the one
inside, to change it without compiling again.

The name is the one of the file without its folder, and it is compared without
the folder of the name asked for and without telling capitals apart:
`MUSICLOAD "music/song"` finds an embedded `song.pt3`. Two files with the same
name are an error. The files take room in the program, and loading one copies
it (`MUSICLOAD` and `SFXLOAD` keep it in the heap), as when it comes from the
card.

```sh
python3 zxbc.py --arch=zxv --embed game.pt3 --embed game.afb --embed level1.bin game.bas
```

```basic
MUSICLOAD "game"               ' game.pt3: the card's, or the one inside
SFXLOAD "game"
LOAD "level1" CODE @map(0)
```

## Code banks

`--code-bank NAME[@REGION]=FILE` (only with `--arch=zxv`, and as many times as
needed) puts the `SUB`s and `FUNCTION`s defined in `FILE` (the main file or an
`#include`) in a code bank (ZX-V decision D109). They are not in memory with the
rest of the program: each bank goes at the end of the `.zxv` and is loaded from
it the first time one of its functions is called, into a region of memory (0 to
7, 0 by default) that the banks of that region share. The program calls them as
always: each one has a small bridge in the resident part that loads its bank if
needed and jumps to it. So a game can have the code of each stage in its own
file, and only the current one takes memory.

```sh
python3 zxbc.py --arch=zxv --code-bank valley=valley.bas --code-bank space=space.bas game.bas
```

- Only the functions go to the bank. The variables of the file (the `DIM`s
  outside the functions), the strings and the libraries (`zxv.bas`...) stay in
  the resident part, so they keep their values when another bank is loaded.
- Calls between functions of the same bank go straight to them (recursion
  too). A call to another bank goes through its bridge, which loads it.
- **A bank in use is not replaced:** if a function of bank `a` calls (directly
  or not) one of bank `b` in the same region, `b` would have to be loaded over
  the code that is running; instead the program stops with "Bank a in use:
  cannot load b in its region". Put banks that call each other in different
  regions (`b@1=b.bas`).
- Loading a bank takes a few milliseconds from the card or `ROM:`, more from
  `EXT:`: the first call of each stage, behind its title, is a good moment.
- Inside a function of a bank, an `ASM` block must not change the section
  (`.data`, `.text`...).
- It needs a ZX-V ROM with banks (specification 0.2); with an older one, the
  first call stops the program with "the ROM has no banks".

The example `examples/zxbasic/fases` of ZX-V has two stages, each one in its
bank. `tests/zxv/bancos.bas` and `bancosuso.bas` test them.

## Libraries

`#include <...>` looks for them in `src/lib/arch/zxv/stdlib`. They keep the names
and parameters of the Z80 ones, but addresses are `ULong`:

| Library | |
|---|---|
| `string.bas` | `mid$`, `left$`, `right$`, `strpos`, `inStr`, `ucase`, `lcase`, `ltrim`, `rtrim`, `trim`, `isdigit`, `isletter`... |
| `hex.bas` | `hex`, `hex16`, `hex8` |
| `input.bas` | `input(MaxLen)` |
| `keys.bas` | `GetKey`, `MultiKeys`, `GetKeyScanCode` and the `KEYx` scan codes |
| `attr.bas`, `point.bas`, `screen.bas`, `pos.bas`, `csrlin.bas` | `attr`, `setattr`, `attraddr`, `point`, `screen`, `pos`, `csrlin` |
| `putchars.bas`, `clearbox.bas` | `putChars`, `getChars`, `paint`, `paintData`, `getPaintData`, `clearBox` |
| `scroll.bas`, `winscroll.bas` | `ScrollRight`, `ScrollLeft`, `ScrollUp`, `ScrollDown` (a pixel; the whole screen, 2.4 ms); `WinScrollRight`, `WinScrollLeft`, `WinScrollUp`, `WinScrollDown` (a cell, with the attributes) |
| `alloc.bas`, `memcopy.bas` | `allocate`, `callocate`, `deallocate`, `reallocate`, `memavail`, `maxavail`, `MemMove`, `MemCopy`, `MemSet` |
| `random.bas`, `round.bas`, `fmath.bas`, `hmirror.bas`, `asc.bas` | `randInt`, `randFixed`, `randomLimit`, `Round`, `fSin`, `fCos`, `fTan`, `fSqrt`, `hMirror`, `asc` |
| `retrace.bas`, `sinclair.bas` | `waitretrace`; the Sinclair BASIC functions |
| `zxv.bas` | What is only in ZX-V (below) |
| `zxvinput.bas`, `zxvsave.bas`, `zxvfade.bas`, `zxvfont.bas`, `zxvinflate.bas`, `zxvmath.bas` | The optional libraries of the ZX-V SDK for games (below) |

### zxv.bas

The commands of the ZX-V BASIC for its video modes, pages, sprites and the rest,
with the same names and arguments (see its manual):

| | |
|---|---|
| `MODE n[, w, h]`, `PAGE n`, `SCREEN n`, `SCROLL [x, y]`, `REMAP c, n`, `REMAPRESET`, `WAIT [n]` | Modes 0 and 1 (and the big pages of mode 0), the page shown and the one drawn on (double buffer), hardware scroll, colour changes, and a steady pace of frames |
| `PALETTE c, r, g, b`, `PALETTERESET` | The palette: colour `c` (0-15) with any red, green and blue (0-255 each; 215 is the level of the standard colours and 255 the bright one), and the standard colours again |
| `GRAB n, x, y, w, h[, t]`, `GRABSTR n, w, a$`, `SPRITE n, x, y[, f[, c$[, w, h]]]`, `HIT(a, ax, ay, b, bx, by)` | Sprites from the screen or from a string of hex digits, drawn mirrored, with other colours or scaled, and pixel-exact collisions |
| `BOX c, x, y, w, h`, `DISC c, x, y, r`, `POLY c, x1, y1, ...`, `CLIP [x, y, w, h]`, `PIXEL(x, y)` | Filled shapes, clipping, and the colour of any pixel |
| `PATTERN [n[, p]]`, `PATTERNBITS r0, ..., r7[, p]` | A two-colour pattern for `BOX`, `DISC` and `POLY`: one of the 11 of the ZX-V BASIC, or 8 bytes of your own (bit 7, the pixel on the left), with paper `p` (0-15), or nothing under the 0s (`p` = -1, the default); `PATTERN` alone, plain colour again. Needs a ZX-V ROM with patterns (decision D110) |
| `KEY(n)`, `JOY(n)`, `SOUND r, v`, `TIME` | Keys held down, joysticks, the registers of the two AY chips, and the frame counter |
| `MUSICLOAD f$`, `MUSICPLAY`, `MUSICSTOP`, `MUSICPAUSE`, `MUSICCONTINUE`, `MUSIC` | Music, in the background while the program goes on: ZX-V's own `.zxm` (the AY chips and the sample channels), a tracker module (a PT3 from `zxv-tracker` or Vortex Tracker, a TurboSound, PT2, STC, ASC or PSG) or a VGM of AY chips (`.vgm`, or compressed: `.vgz`) |
| `SFXLOAD f$`, `SFX n[, c]`, `SFXSTOP`, `SFXPLAYING` | Sound effects of an AYFX bank (`.afb`) or a single one (`.afx`), on a free channel or on channel `c` (0-5) |
| `SAMPLELOAD f$, n`, `SAMPLE n[, tone, vol, pan, c]`, `SAMPLESTOP [c]`, `SAMPLEPLAYING` | Recorded sounds (WAV) on the sample channels: sound `n` (0-31), `tone` semitones up or down, volume 0-64, pan 0-255, on a free channel or on channel `c` (0-11) |

They draw with the graphics engine of the ROM, on the drawing page (`SCREEN`), in
pixels from its top left corner with the y downwards; colours are 0-15 and 8 is
transparent in sprites. `PRINT`, `PLOT` and `CLS` draw on the top left 256 x 192
pixels of the drawing page, in any mode.

Where it is not as in the ZX-V BASIC: the sprite of a string is `GRABSTR` (`GRAB n,
w, a$` there), `KEY`, `JOY`, `HIT` and `PIXEL` need parentheses, `REMAP` and `SOUND`
take one pair at a time and `PALETTE` one colour, `REMAP` and `PALETTE` only
change table 0 (no `#t`), `REMAPRESET` and `PALETTERESET` are `REMAP` and
`PALETTE` alone, `POLY` up to 8 points, `SCREEN` can not be used with
`screen.bas`, the music and effects are `MUSICLOAD f$` and `SFXLOAD f$`
(`LOAD f$ MUSIC` and `SFX` there), `MUSICPLAY`, `MUSICSTOP`... and `SFXPLAYING`
(the function `SFX` there), and the recorded sounds `SAMPLELOAD f$, n` (`LOAD f$
SAMPLE n` there), `SAMPLE n, tone, vol, pan, c` (`; c` there), `SAMPLESTOP` and
`SAMPLEPLAYING` (the function `SAMPLE` there). `MUSICLOAD` does not take MOD,
S3M or XM modules, which ZX-V's BASIC converts while loading: so as not to
carry the converter in every program, `zxv-music` turns them into a `.zxm`
first (`zxv-music song.xm song.zxm`).

The music and the effects play with the music library of the ZX-V SDK
(`sdk/music`, the same code as in the ROM and in `zxv-tracker`), from the frame
hook: the program goes on while they play, until `MUSICSTOP` or the end of the
program. The module and the bank go in the heap; a `.vgz` takes 32 KB more of
it, to decompress it while it plays. The samples of a `.zxm` go to the sound
memory from the bottom and those of `SAMPLELOAD` from the top, as in ZX-V's
BASIC (nothing of a WAV stays in the RAM). While something plays, `BEEP`
goes as far ahead as the music (35 ms), with the `beeper_event_at` function of
the ZX-V API (55, in the ROMs after ZX-V 0.5.0; in older ones `BEEP` is silent
while the music plays), so the timed queue keeps its order.

`ASM` blocks are RISC-V assembler (GNU syntax). In a function, the parameters
are at `8(s0)`, `12(s0)`... (4 bytes each, 8 a `Float`), and a function that ends
with an `ASM` block returns what it leaves in `a0` (`a0` and `a1` for a `Float`).
`zxb_ccall function` calls a C function of the runtime (it aligns the stack, as
the ABI requires): that is how the libraries are written.

### The libraries of the ZX-V SDK

The optional libraries of the ZX-V SDK (`sdk/lib`, decision D111 of ZX-V),
each one in a file of its own: `#include <zxvinput.bas>` and so on. They call
the same C as the programs in C (copied in `src/lib/arch/zxv/sdk/lib` by
`tools/zxv_sdk_sync.py`), and only what a program uses goes into it: a
program without them does not change by a byte. Their state (the controls of
the players, the fonts, the table of scores) stays in the library, and the
BASIC names it by number.

| File | | |
|---|---|---|
| `zxvinput.bas` | `CTRLUPDATE [p]`, `CTRLHELD([p])`, `CTRLPRESSED([p])`, `CTRLRELEASED([p])`, `CTRLFRAMES(p, a)`, `CTRLREPEAT(p, a[, delay, rate])`, `CTRLRESET [p]`, `CTRLKEY p, a, slot, k`, `CTRLBUTTON p, a, buttons`, `CTRLPAD p, pad`, `CTRLKEEPKEYS p, on`, `CTRLMAP([p])`, `CTRLMAPSIZE`, `NEWKEY`, `KEYNAME(k)`; the actions `CTRLRIGHT`, `CTRLLEFT`, `CTRLDOWN`, `CTRLUP`, `CTRLFIRE`, `CTRLFIRE2`, `CTRLFIRE3`, `CTRLSTART`, `CTRLSELECT`, `CTRLESC` and the keys `KEYA`... `KEYSPACE`, `KEYUPKEY`... | The keyboard and the joysticks at once, as actions, for two players (`p` 0 and 1): player 0 with the cursors or Q A O P, Space or M to fire and joystick 0; player 1, joystick 1 and the keys you give it. What was pressed or released in this frame, repeat for menus, and keys that the player can change (`NEWKEY`, `KEYNAME` in the language of the machine) and save (`CTRLMAP` with `SAVEDATA`). It empties the queue of keys, so what is typed in the game does not reach the BASIC (`CTRLKEEPKEYS` keeps it) |
| `zxvsave.bas` | `SAVEDATA(f$, version, addr, size)`, `LOADDATA(...)`, `CRC32(addr, size)`, `SCORECLEAR [n]`, `SCOREADD(score, name$)`, `SCORE(i)`, `SCORENAME(i)`, `SCORESAVE(f$[, version])`, `SCORELOAD(f$[, version])`; `SAVENOFILE`, `SAVENOCARD`, `SAVEDAMAGED`, `SAVEVERSION` | Saving the data of a game (`@a(0)` of an array, `@v` of a variable) with the version of the data and a CRC, through a temporary file: a power cut leaves the file before, and a file of another version or damaged is not read (the memory does not change). Without a card, an error and the game goes on. And a table of up to 10 scores |
| `zxvfade.bas` | `FADETO addr, frames[, tables]`, `FADECOLOUR rgb, frames[, tables]`, `FADESTEP`, `FADEBUSY`, `FADEWAIT`, `RGBMIX(a, b, k, n)`, `RGB565(r, g, b)` | Palette fades: to black, to white or to other colours (`@pal(0)` of 16 `UInteger`; 0, the standard ones) in some frames, a step each `FADESTEP` while the game goes on, or all with `FADEWAIT` |
| `zxvfont.bas` | `FONTROM n[, fixedw]`, `FONTGRAB n, x, y, w, h, cw, ch[, first, t, fixedw]`, `FONTSPACING n, s`, `FONTDRAW(n, x, y, t$, colour[, scale])`, `FONTWIDTH(n, t$[, scale])`, `FONTHEIGHT(...)`; `FONTOWN` | Text of any size (1-8 times) and colour, proportional or of fixed width, with accents (UTF-8) and line breaks (`CHR$ 10`): four fonts, the 8 x 8 of the ROM or one drawn on the page in cells of any size and in its colours. With `CLIP` and `PATTERN` |
| `zxvinflate.bas` | `GUNZIP(src, n, dst, max)`, `GUNZIPSIZE(src, n)`, `INFLATE(src, n, dst, max)`; `INFLATEDATA`, `INFLATESPACE` | Data compressed with gzip on the PC, uncompressed in the memory (the `.gz` checked with its CRC) |
| `zxvmath.bas` | `TSIN(a)`, `TCOS(a)`, `TATAN2(y, x)`, `ISQRT(v)`, `HYPOT(x, y)`, `RNDSEED s`, `RNDNEXT`, `RNDRANGE(lo, hi)` | Sine and cosine as `Fixed` (angles in turns of 65536, the ones of the graphics engine), the angle of a vector, square roots, distances and random numbers with a seed of their own |

`tests/zxv/sdkinput.bas`, `sdksave.bas`, `sdkfade.bas`, `sdkfont.bas`,
`sdkinflate.bas` and `sdkmath.bas` test them. The moving through tile maps,
the objects on the mode 7 floor and the 3D of the SDK are not here yet: the
first two need the maps and the textures of the ZX-V BASIC, and the 3D its
models in BASIC.

## Licence

The runtime (`src/lib/arch/zxv/runtime`) and the libraries are MIT, like the rest
of `src/lib`, so compiled programs can have any licence, closed ones too. The
runtime has two files of ZX-V (`bmath.c` and `fp.c`, with the fdlibm notice), and
programs are linked with the start-up code, headers, font and music library of
the ZX-V SDK (`src/lib/arch/zxv/sdk`): all MIT as well.

## Tests

```sh
export ZXV_CLI=/path/to/zxv-cli ZXV_ROM=/path/to/rom.bin
python3 tests/zxv/run.py            # all; NAME... for some; --update writes NAME.out
```

Each test is a program (`NAME.bas`) and what it prints (`NAME.out`), and maybe the
colour of some pixels of its screen (`NAME.pix`), the files of its SD card (a
folder, `NAME.sd`, and `NAME.files`, the ones it must have at the end) and more
options for the compiler (`NAME.opt`, run in `tests/zxv`). The runtime is built with
`-DZXB_TEST`, so what the program prints also goes to the `zxv-cli --test` console.
