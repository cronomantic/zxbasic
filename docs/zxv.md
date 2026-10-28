# ZX-V target (prototype)

`--arch=zxv` compiles for [ZX-V](https://github.com/cronomantic/ZX-V), a 32-bit
RISC-V machine (RV32IMAC at 144 MHz) with a ZX Spectrum style BASIC. The output
is a `.zxv` native program that its BASIC loads with `LOAD "name"`.

```sh
export ZXV_SDK=/path/to/ZX-V/sdk          # crt0.S, zxv.ld, include/, tools/elf2zxv.py
python3 zxbc.py --arch=zxv program.bas    # program.zxv
python3 zxbc.py --arch=zxv -f asm program.bas   # only the assembler (GNU as)
```

It needs the RISC-V GCC toolchain (`riscv64-unknown-elf-gcc`; `ZXV_CROSS` changes
the prefix) and the ZX-V repository: the runtime uses its SDK and the font and
number routines of its ROM.

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
  `LBOUND` and `UBOUND`.
- Math functions (the same results as the ZX-V BASIC), `RND`, `RANDOMIZE`.
- `READ`, `DATA` (constants and expressions) and `RESTORE`.
- `PRINT` with `AT`, `TAB`, commas, colours, `INVERSE`, `OVER`, UDG and block
  graphics; `CLS`, `BORDER`, `PLOT`, `DRAW` (with arcs), `CIRCLE`, `BEEP`, `PAUSE`,
  `INKEY$`, `USR`, `PEEK` and `POKE`; `IN` and `OUT` with the Spectrum ports:
  the keyboard (`xxFEh`, the ZX-V matrix is the Spectrum one), the Kempston
  joystick (`1Fh`) and the border (`OUT 254`).
- The libraries below.
- When the program ends, its report (`0 OK`, or the error) stays on the bottom
  line until a key is pressed, as in the Spectrum: the ZX-V BASIC starts again and
  clears the screen.

Speed: a `FOR` loop of a million turns with `LET s = s + i * 2 + 1` takes 0.34
seconds (17 frames), about 90 times the ZX-V BASIC.

Not yet:

- The libraries that are not in the list below.
- Copying arrays (`a = b`).
- `LOAD`/`SAVE`, `FLASH` (it does not blink), `BOLD`, `ITALIC`.
- `FASTCALL` functions with parameters written in BASIC (their first parameter
  comes in `a0`, for `ASM` blocks).
- The line number of errors.
- Optimization of the generated code (the `-O` levels only change the front end).

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

### zxv.bas

The commands of the ZX-V BASIC for its video modes, pages, sprites and the rest,
with the same names and arguments (see its manual):

| | |
|---|---|
| `MODE n[, w, h]`, `PAGE n`, `SCREEN n`, `SCROLL [x, y]`, `REMAP c, n`, `REMAPRESET`, `WAIT [n]` | Modes 0 and 1 (and the big pages of mode 0), the page shown and the one drawn on (double buffer), hardware scroll, colour changes, and a steady pace of frames |
| `GRAB n, x, y, w, h[, t]`, `GRABSTR n, w, a$`, `SPRITE n, x, y[, f[, c$[, w, h]]]`, `HIT(a, ax, ay, b, bx, by)` | Sprites from the screen or from a string of hex digits, drawn mirrored, with other colours or scaled, and pixel-exact collisions |
| `BOX c, x, y, w, h`, `DISC c, x, y, r`, `POLY c, x1, y1, ...`, `CLIP [x, y, w, h]`, `PIXEL(x, y)` | Filled shapes, clipping, and the colour of any pixel |
| `KEY(n)`, `JOY(n)`, `SOUND r, v`, `TIME` | Keys held down, joysticks, the registers of the two AY chips, and the frame counter |

They draw with the graphics engine of the ROM, on the drawing page (`SCREEN`), in
pixels from its top left corner with the y downwards; colours are 0-15 and 8 is
transparent in sprites. `PRINT`, `PLOT` and `CLS` draw on the top left 256 x 192
pixels of the drawing page, in any mode.

Where it is not as in the ZX-V BASIC: the sprite of a string is `GRABSTR` (`GRAB n,
w, a$` there), `KEY`, `JOY`, `HIT` and `PIXEL` need parentheses, `REMAP` and `SOUND`
take one pair at a time, `POLY` up to 8 points, and `SCREEN` can not be used with
`screen.bas`.

`ASM` blocks are RISC-V assembler (GNU syntax). In a function, the parameters
are at `8(s0)`, `12(s0)`... (4 bytes each, 8 a `Float`), and a function that ends
with an `ASM` block returns what it leaves in `a0` (`a0` and `a1` for a `Float`).
`zxb_ccall function` calls a C function of the runtime (it aligns the stack, as
the ABI requires): that is how the libraries are written.

## Licence

The runtime (`src/lib/arch/zxv/runtime`) and the libraries are MIT, like the rest
of `src/lib`, so compiled programs can have any licence, closed ones too. The
runtime has two files of ZX-V (`bmath.c` and `fp.c`, with the fdlibm notice), and
programs are linked with the start-up code, headers and font of the ZX-V SDK: all
MIT as well.

## Tests

```sh
export ZXV_SDK=/path/to/ZX-V/sdk ZXV_CLI=/path/to/zxv-cli ZXV_ROM=/path/to/rom.bin
python3 tests/zxv/run.py            # all; NAME... for some; --update writes NAME.out
```

Each test is a program (`NAME.bas`) and what it prints (`NAME.out`), and maybe the
colour of some pixels of its screen (`NAME.pix`). The runtime is built with
`-DZXB_TEST`, so what the program prints also goes to the `zxv-cli --test` console.
