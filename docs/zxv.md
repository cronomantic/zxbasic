# ZX-V target

`--arch=zxv` compiles for [ZX-V](https://github.com/cronomantic/ZX-V), a 32-bit
RISC-V machine (RV32IMAC at 144 MHz) with a ZX Spectrum style BASIC. The output
is a `.zxv` native program that its BASIC loads with `LOAD "name"`.

```sh
python3 zxbc.py --arch=zxv program.bas    # program.zxv
python3 zxbc.py --arch=zxv -f asm program.bas   # only the assembler (GNU as)
python3 zxbc.py --arch=zxv --loading-screen title.png program.bas   # with a loading screen
python3 zxbc.py --arch=zxv --embed song.pt3 --embed fx.afb game.bas   # with its files inside
python3 zxbc.py --arch=zxv --code-bank stage1=stage1.bas game.bas     # with a code bank
```

It only needs the RISC-V GCC toolchain (`riscv64-unknown-elf-gcc`; `ZXV_CROSS`
changes the prefix), the one of the ZX-V SDK. The parts of that SDK the programs
are linked with, and the font of the machine, are in `src/lib/arch/zxv/sdk` and
`src/lib/arch/zxv/runtime` (`tools/zxv_sdk_sync.py PATH/TO/ZX-V` updates them).

## Versions

The target is published as tags of the branch `zxv` of this fork, with a
Windows build (`zxbc.exe`, no Python needed) in the
[releases](https://github.com/cronomantic/zxbasic/releases) from
v1.19.0-zxv.2 on. Each entry of this page says in **Since** the first version
that has it.

| Version | What it brought | ZX-V it needs |
|---|---|---|
| v1.19.0-zxv.1 | The target; `zxv.bas` with modes, pages, sprites, filled shapes, keys, joysticks, the AY chips and `TIME` | Older than 0.7.0 |
| v1.19.0-zxv.2 | `SAVE`, `LOAD` and `VERIFY` on the card; `FLASH`; the line in the error reports | Older than 0.7.0 |
| v1.19.0-zxv.3 | Music and sound effects: `MUSICLOAD`, `MUSICPLAY`... `SFXLOAD`, `SFX`... (PT3, TurboSound, PT2, STC, ASC, PSG; AYFX) | Older than 0.7.0 |
| v1.19.0-zxv.4 | `KEY`, `JOY` and `TIME` also work with `--enable-break` | Older than 0.7.0 |
| v1.19.0-zxv.5 | `PALETTE`, `PALETTERESET`; `--loading-screen`; the screen in a `.gfx` and the rest in a `.bin`; the `.zxv` header of ZX-V 0.7.0 | 0.7.0 or later |
| v1.19.0-zxv.6 | `--embed`; the `EXT:` drive (`V Disk error (no server for EXT:)`); `.zxm` and VGM music; `SAMPLELOAD`, `SAMPLE`, `SAMPLESTOP`, `SAMPLEPLAYING` | 0.7.0 or later (the samples, 0.10.0) |
| v1.19.0-zxv.7 | `--code-bank`; tile maps (`TILE`... `CELL`); mode 7 (`TEXTURE`, `FLOOR`, `FLOORMAP`, `ROTATE`, `ROTATEMAP`); `PATTERN`, `PATTERNBITS`; the libraries of the ZX-V SDK in BASIC (`zxvinput.bas`... `zxv3d.bas`); 244 KB of user RAM | 0.7.0 or later; some parts, 0.11.0 or later (below) |

A `.zxv` made with v1.19.0-zxv.4 or older does not load in ZX-V 0.7.0 or
later (the header of the programs grew): compile it again.

**ROMs.** Some things use functions of the ZX-V ROM that only a ROM of
**specification 0.2** has: ZX-V 0.11.0 or later (0.10.0 and the older ones
are specification 0.1). They are code banks (`--code-bank`),
`PATTERN`, `PATTERNBITS`, `ROTATE`, `FLOORMAP`, `ROTATEMAP` and the shading of
`zxv3d.bas`. With an older ROM the program still loads, but the first call to
a bank stops it with "the ROM has no banks", and the rest does not draw what
it should (see each entry). Other needs: `BEEP` while music plays needs ZX-V
0.6.0 or later (before, it is silent), and the sample channels (`SAMPLE`, the
sample tracks of a `.zxm`) ZX-V 0.10.0 or later.

## How it works

The front end and the translator to intermediate code are the Z80 ones. The arch
changes:

| | Z80 | ZX-V |
|---|---|---|
| Pointers (`@x`, strings, `PEEK`/`POKE` addresses) | 16 bits | 32 bits (`ULong`) |
| `Float` | 5 bytes (ZX Spectrum ROM) | IEEE double (8 bytes) |
| Parameters | 2-byte slots | 4-byte slots (doubles 8) |
| Local variables | packed | aligned (RISC-V does not allow misaligned accesses) |
| `ASM` blocks, `USR` | Z80 code | RISC-V code |

The other types are the same: `Byte`, `UByte`, `Integer`, `UInteger`, `Long`,
`ULong`, `Fixed` (16.16) and `String`, with the same conversions. A `Float`
has about 15 significant digits instead of 9, and goes from about 1e-308 to
1e308; the maths functions give the same results as the ZX-V BASIC (they
are its own code). Anything that takes an address (`@x`, `PEEK`, `POKE`,
`USR`, `LOAD ... CODE`, the libraries) takes a `ULong`.

The backend (`src/arch/zxv/backend`) keeps the model of the Z80 one, so the
translator did not need to change: temporaries live in the stack (the second
operand on top), BASIC functions and `GO SUB` keep the return address in the stack,
`s0` is the frame pointer. It emits GNU assembler, which is linked with the
runtime and the ZX-V SDK, and converted to `.zxv` with `elf2zxv.py`.

The runtime (`src/lib/arch/zxv/runtime`) is in C. Its functions get the `fparam`
values first and then the stack parameters, with the ilp32 ABI.

`ASM` blocks are RISC-V assembler (GNU syntax). In a function, the parameters
are at `8(s0)`, `12(s0)`... (4 bytes each, 8 a `Float`), and a function that ends
with an `ASM` block returns what it leaves in `a0` (`a0` and `a1` for a `Float`).
A `FASTCALL` function gets its first parameter in `a0` (`a0` and `a1` a
`Float`; a `String`, its address in `a0`), the others in the stack above
the return address (at `0(sp)`), and has no frame (`s0` is the caller's). So,
as on the Z80, `FASTCALL` is for functions written in `ASM`: BASIC code in
them that reads a parameter gets garbage, whatever its type, with or without
banks. Since v1.19.0-zxv.7 the compiler warns about it on zxv ("... is
FASTCALL: its parameters arrive in registers"; it does not for the Z80).
Write those functions without `FASTCALL`, or read the parameters in `ASM`.
`zxb_ccall function` calls a C function of the runtime (it aligns the stack, as
the ABI requires): that is how the libraries are written. `USR address` calls
RISC-V code at that address and gives what it leaves in `a0`. Z80 machine code
(in `ASM`, in `DATA` for `USR`, or in the Z80 libraries) does not work.

## Memory below 64K

Addresses below 65536 are emulated, as the ZX-V BASIC does, so programs written for
the Spectrum keep working: the screen (16384) and its attributes (22528) are shown
in mode 0, the font is at 15616 (0-16383 read 0 elsewhere and ignore `POKE`),
and the system variables and the rest (23296-65535) are a block of memory,
with some variables updated: `BORDCR` (23624), `FRAMES` (23672), `COORDS`
(23677), `S POSN` (23688, read only), `ATTR P`, `MASK P`, `ATTR T` and
`MASK T` (23693-23696). `PRINT` takes the font from `CHARS` (23606) and the
UDG from `UDG` (23675), so they can be changed as in the Spectrum (the UDG
are at 65368). `DIM x AS ... AT address` works there too.

ZX-V draws 60 frames a second, but the Spectrum things keep their time: `PAUSE n`
waits n fiftieths of a second, `FRAMES` (23672) counts fiftieths and `FLASH`
blinks every 16/50 of a second. `TIME` and `WAIT` (in `zxv.bas`) count ZX-V
frames, 60 a second.

The Spectrum screen is the top left 256 × 192 pixels of the drawing page
(`SCREEN` in `zxv.bas`), in every mode: `PRINT`, `PLOT`, `DRAW`, `CIRCLE` and
`CLS` draw there, and `PEEK`, `SCREEN$`, `ATTR` and `POINT` read it. The ports
are those of the keyboard (`IN` of `xxFEh`, the ZX-V matrix is the Spectrum
one), the Kempston joystick (`IN 31`, joystick 0) and the border (`OUT 254`,
bits 0-2); the others read 255 and ignore `OUT`.

Above 64K the addresses are real: the ZX-V RAM, its VRAM and its registers.

### How much memory a program has

The program goes in the user RAM of ZX-V, 240 KB from `0x20034000` to
`0x2006FFFF` (`src/lib/arch/zxv/sdk/zxv.ld`): first its code and its data,
then the heap, and the last 32 KB are the stack. The heap is all that is left
(`zxb_mem.c`): strings, local arrays, and what `zxv.bas` and the libraries
keep (sprites, tiles, maps, textures, the music module, the effects bank,
fonts). When it is full, the program stops with `4 Out of memory`. The block
of the Spectrum memory (23296-65535) and the Spectrum screen take about
49 KB of the program's data. If the code and the data do not leave 6 KB
for the stack, the link fails ("el programa no cabe", a check of the SDK's
`zxv.ld`) and the compiler stops. Code banks (`--code-bank`) are the way to
have more code than fits.

v1.19.0-zxv.6 and the older versions link for 248 KB, the user RAM of ZX-V
0.10.0 and older; the ROMs after it keep 4 KB for the Wi-Fi.

## Compiler options

The options of the language (`-D`, `-I`, `-i`, `--explicit`, `--strict`,
`--array-base`, `--string-base`, `-Z`/`--sinclair`, the warnings...) work as
in the Z80 targets. For ZX-V:

| Option | |
|---|---|
| `-f zxv` (by default), `-f asm`, `-f ir`, `--parse-only` | A `.zxv` program, the RISC-V assembler, or the intermediate code |
| `--enable-break` | Esc stops the program (`L BREAK into program`) and the error reports say the line: `9 STOP statement, 12` |
| `--loading-screen FILE` | A loading screen (below). Since v1.19.0-zxv.5 |
| `--embed FILE` | A file inside the `.zxv` (below). Since v1.19.0-zxv.6 |
| `--code-bank NAME[@REGION]=FILE` | A code bank (below). Since v1.19.0-zxv.7 |
| `-O n` | Only changes the front end: the RISC-V code is not optimised yet |

Refused (the compiler stops with an error):

- `-f bin`, `tap`, `tzx`, `sna` or `z80`, and `-t`/`--tap`, `-T`/`--tzx`:
  "The zxv architecture only outputs zxv, asm or ir formats".
- `-B`/`--BASIC` and `-a`/`--autorun`: there is no BASIC loader, `LOAD
  "name"` runs the program.
- `--append-binary` and `--append-headless-binary`: use `--embed`.

`--debug-array` (since v1.19.0-zxv.7) checks every subscript of an array
against its bounds when the program runs, and one out of them stops it with
`3 Subscript wrong`, as on the Z80. Without it subscripts are not checked:
an index out of the bounds reads or writes other memory, without an error.
A constant subscript out of the bounds is a warning when compiling, with or
without it.

Accepted, but they do nothing:

- `-S`/`--org`: the program goes where the ROM loads it, `0x20034000`.
- `-H`/`--heap-size` and `--heap-address`: the heap is all the free memory.
- `--debug-memory`: the memory is always checked (`4 Out of memory`).
- `--headerless`, `-N`/`--zxnext` and `-M`/`--mmap` (no map is written).

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
  `INKEY$`, `USR`, `PEEK` and `POKE`; `IN` and `OUT` with the Spectrum ports
  (above).
- `SAVE`, `LOAD` and `VERIFY` with `CODE`, `SCREEN$` and `DATA` (below).
- With `--enable-break`, Esc (BREAK) stops the program (`L BREAK into program`)
  and the error reports say the line: `9 STOP statement, 12`.
- The libraries below.
- When the program ends, its report (`0 OK`, or the error) stays on the bottom
  line until a key is pressed, as in the Spectrum: the ZX-V BASIC starts again and
  clears the screen. The reports are in English, as in ZX Basic.

Speed: a `FOR` loop of a million turns with `LET s = s + i * 2 + 1` takes 0.34
seconds (about 20 frames), about 90 times the ZX-V BASIC.

Accepted, but they do nothing: `BOLD` and `ITALIC` (also inside `PRINT`):
the text comes out as always.

Not yet:

- The libraries that are not in the list below.
- Optimization of the generated code (the `-O` levels only change the front end).

## Loading screen

**Since:** v1.19.0-zxv.5.

`--loading-screen FILE` (only with `--arch=zxv`) puts a loading screen in the
`.zxv`, as ZX-V's `elf2zxv.py --screen` does: a PNG of 256×192 (mode 0) or
512×384 (mode 1), or the whole frame with its border, 320×240 or 640×480 (the
border takes the colour of the top left corner); or the first screen of a `.gfx`
(`png2gfx.py --screen`, zxv-gfx, or `SAVE "x" SCREEN$`), with its palette if it
has one. A PNG keeps its colours, as `png2gfx.py --screen` does: if they are
all standard ZX-V colours, the screen has no palette; if not, it can have up
to 16, and carries a palette with them (the standard ones at their own index,
the others in the free ones; more than 16 is an error).

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

**Since:** v1.19.0-zxv.2 (`.gfx` and `.bin` since v1.19.0-zxv.5; before, a
name without extension was a `.tap`; the report for `EXT:` without a
server, since v1.19.0-zxv.6).

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
without a name; as in the Z80 target, an array needs `CODE`.

| Error | When |
|---|---|
| `F Invalid file name` | The name is empty or has 64 characters or more. |
| `T File not found` | `LOAD` or `VERIFY` of a file that is not there (nor inside the program, `--embed`). |
| `U No SD card` | The name goes to the card and there is none. |
| `V Disk error (no server for EXT:)` | The name goes to `EXT:` and nobody serves it. |
| `V Disk error` | The card failed, it is full, or the drive does not let it (`SAVE` on `ROM:`). |
| `R Loading error` | `VERIFY` found a difference; a `.tap` without a `CODE` block, or cut; a `.gfx` without a screen. |
| `A Invalid argument` | `SAVE ... CODE` of 0 bytes; `LOAD ... CODE` of a file that is not a `.tap`, without an address. |
| `B Integer out of range` | The block is not all in the memory of the program (or all below 64K); a tape block of more than 65535 bytes. |

## Files inside the program

**Since:** v1.19.0-zxv.6.

`--embed FILE` (only with `--arch=zxv`, and as many times as needed) puts a file
inside the `.zxv`, as the C games of ZX-V carry their graphics and music.
`LOAD`, `VERIFY`, `MUSICLOAD`, `SFXLOAD` and `SAMPLELOAD` look first in the
current folder and, if the file is not there (or there is no card), take the
one inside with that name. So the `.zxv` works alone, also from the `ROM:`
drive, and a file with the same name on the card takes the place of the one
inside, to change it without compiling again.

The name is the one of the file without its folder, and it is compared without
the folder of the name asked for and without telling capitals apart:
`MUSICLOAD "music/song"` finds an embedded `song.pt3`. Two files with the same
name, or a name that is not a valid ZX-V file name (more than 63 characters),
are an error. The files take room in the program, and loading one copies
it (`MUSICLOAD` and `SFXLOAD` keep it in the heap), as when it comes from the
card. What writes (`SAVE`, `SAVEDATA` of `zxvsave.bas`) and `LOADDATA` only
see the card.

```sh
python3 zxbc.py --arch=zxv --embed game.pt3 --embed game.afb --embed level1.bin game.bas
```

```basic
MUSICLOAD "game"               ' game.pt3: the card's, or the one inside
SFXLOAD "game"
LOAD "level1" CODE @map(0)
```

## Code banks

**Since:** v1.19.0-zxv.7. Needs a ZX-V ROM of specification 0.2
(ZX-V 0.11.0 or later).

`--code-bank NAME[@REGION]=FILE` (only with `--arch=zxv`, and as many times as
needed) puts the `SUB`s and `FUNCTION`s defined in `FILE` (the main file or an
`#include`) in a code bank. They are not in memory with the
rest of the program: each bank goes at the end of the `.zxv` and is loaded from
it the first time one of its functions is called, into a region of memory (0 to
7, 0 by default) that the banks of that region share. The program calls them as
always: each one has a small bridge in the resident part that loads its bank if
needed and jumps to it. So a game can have the code of each stage in its own
file, and only the current one takes memory.

```sh
python3 zxbc.py --arch=zxv --code-bank valley=valley.bas --code-bank space=space.bas game.bas
```

- **It is one program, not several chained ones.** Everything is compiled
  and linked together and only the code of the functions goes to the bank.
  The global variables (also the `DIM`s outside the functions in the bank's
  file), the strings and the libraries (`zxv.bas`...) stay in the resident
  part: every bank and the main program read and change them, and they keep
  their values when another bank is loaded. That is how lives or points go
  from one stage to the next. Local variables and parameters live on the
  stack while the function runs, as always.
- Calls between functions of the same bank go straight to them (recursion
  too). A call to another bank goes through its bridge, which loads it.
- **A bank in use is not replaced:** if a function of bank `a` calls (directly
  or not) one of bank `b` in the same region, `b` would have to be loaded over
  the code that is running; instead the program stops with "Bank a in use:
  cannot load b in its region". Put banks that call each other in different
  regions (`b@1=b.bas`).
- **At most 32 bank calls one inside another.** What counts is each call that
  goes through a bridge and has not returned yet: a call to a function of a
  bank from the resident part or from another bank. Calls inside the same
  bank go straight and do not count (a recursive function of a bank can go as
  deep as the stack allows), nor do calls to the resident part. A recursion
  that goes from one bank to another (`a` calls `b`, which calls `a`...)
  counts every crossing, and so does a resident function called from a bank
  that calls the bank again. The call number 33 stops the program with "Too
  many nested bank calls".
- Loading a bank takes a few milliseconds from the card or `ROM:`, more from
  `EXT:`: the first call of each stage, behind its title, is a good moment.
- Inside a function of a bank, an `ASM` block must not change the section
  (`.data`, `.text`...).
- With a ROM of specification 0.1 (ZX-V 0.10.0 or older), the first call
  stops the program with "Bank NAME: the ROM has no banks". Other reports of
  that first load: "it is not in the file", "it is damaged", "the program
  changed: load it again" (the `.zxv` was replaced while it ran), "no card",
  "no server for EXT:".

The compiler stops if the option is not `NAME[@REGION]=FILE`, the name does
not have 1 to 15 letters, digits or `_`, the region is not 0 to 7, a name or
a file is repeated, or the file cannot be read; it warns if no `SUB` or
`FUNCTION` of the file is used.

The example `examples/zxbasic/fases` of ZX-V has two stages, each one in its
bank. `tests/zxv/bancos.bas` (also variables declared in a bank's file and
shared by two banks) and `bancosuso.bas` test them.

## Libraries

`#include <...>` looks for them in `src/lib/arch/zxv/stdlib`. They keep the names
and parameters of the Z80 ones, but addresses are `ULong`. All of them are in
every version from v1.19.0-zxv.1 on, except the libraries of the ZX-V SDK
(since v1.19.0-zxv.7).

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
| `zxvinput.bas`, `zxvsave.bas`, `zxvfade.bas`, `zxvfont.bas`, `zxvinflate.bas`, `zxvmath.bas`, `zxvmapmove.bas`, `zxvm7.bas`, `zxv3d.bas` | The optional libraries of the ZX-V SDK for games (below). Since v1.19.0-zxv.7 |

### zxv.bas

The commands of the ZX-V BASIC for its video modes, pages, sprites, tile
maps, mode 7, the sound and the rest, with the same names and arguments as
far as ZX Basic allows. Each entry below says what is the same and only
what changes; the full explanation of each command is in the
[reference of the ZX-V BASIC][b-ref]. They draw with the graphics engine of
the ZX-V ROM; the music plays with the music library of the ZX-V SDK.

```basic
#include <zxv.bas>

GRABSTR 0, 4, "1111122112211111"
FOR x = 0 TO 240 STEP 16
    SPRITE 0, x, 80
NEXT x
BOX 4, 100, 20, 20, 20
```

| Group | Commands |
|---|---|
| Modes and pages | [MODE](#mode), [PAGE](#page), [SCREEN](#screen), [SCROLL](#scroll), [REMAP](#remap), [REMAPRESET](#remapreset), [PALETTE](#palette), [PALETTERESET](#palettereset), [WAIT](#wait) |
| Sprites | [GRAB](#grab), [GRABSTR](#grabstr), [SPRITE](#sprite), [HIT](#hit) |
| Shapes | [BOX](#box), [DISC](#disc), [POLY](#poly), [PATTERN](#pattern), [PATTERNBITS](#patternbits), [CLIP](#clip), [PIXEL](#pixel) |
| Tile maps | [TILE](#tile), [TILESTR](#tilestr), [MAPDIM](#mapdim), [MAPPOKE](#mappoke), [MAPPOKESTR](#mappokestr), [MAP](#map), [CELL](#cell) |
| Mode 7 | [TEXTURE](#texture), [FLOOR](#floor), [FLOORMAP](#floormap), [ROTATE](#rotate), [ROTATEMAP](#rotatemap) |
| Keys, joysticks, AY, time | [KEY](#key), [JOY](#joy), [SOUND](#sound), [TIME](#time) |
| Music | [MUSICLOAD](#musicload), [MUSICPLAY](#musicplay), [MUSICSTOP](#musicstop), [MUSICPAUSE](#musicpause), [MUSICCONTINUE](#musiccontinue), [MUSIC](#music) |
| Sound effects | [SFXLOAD](#sfxload), [SFX](#sfx), [SFXSTOP](#sfxstop), [SFXPLAYING](#sfxplaying) |
| Recorded sounds | [SAMPLELOAD](#sampleload), [SAMPLE](#sample), [SAMPLESTOP](#samplestop), [SAMPLEPLAYING](#sampleplaying) |

**How they work**

- `#include <zxv.bas>` at the start of the program. The names do not tell
  capitals apart: `MODE` and `mode` are the same.
- Each parameter has a ZX Basic type (in the syntax of each entry), and the
  argument is converted to it as in any call of ZX Basic, without an error:
  a `UByte` keeps the low 8 bits (`PAGE 257` is `PAGE 1`), a `UInteger` the
  low 16, a `Byte` and an `Integer` the same with their sign. The ranges and
  errors of each entry are checked on what is left.
- The graphics count pixels from the top left corner of the **drawing page**
  (`SCREEN`), with the y downwards; positions are `Integer` (-32768 to
  32767), and what falls out of the page is not drawn.
- Colours are 0-15 (the [colours of the ZX-V BASIC][b-colours]); in
  sprites, tiles and textures, 8 is transparent.
- `Fixed` parameters (mode 7) are 16.16 numbers, from -32768 to 32767.99998:
  `1.5`, or a `Float` or `Integer` variable, is converted.
- A function without arguments is called with or without brackets: `MUSIC`,
  `TIME()`.
- `PRINT`, `PLOT`, `DRAW`, `CIRCLE` and `CLS` draw on the top left 256 × 192
  pixels of the drawing page, in any mode (see [Memory below
  64K](#memory-below-64k)).

**Errors common to every command**

| Error | When |
|---|---|
| `4 Out of memory` | The heap is full. Sprites, tiles, maps, textures, the music module, the effects bank and the fonts go there. |

With `--enable-break`, every report also says the line. The reports are those
of ZX Basic (in English): `A Invalid argument`, `B Integer out of range`,
`K Invalid colour`... and those of the files (`T File not found`...).

#### Differences with the ZX-V BASIC

This is the only list of them; each entry repeats its own.

- **Brackets.** `KEY` and `JOY` are functions with brackets: `KEY(9)`, not
  `KEY 9`. (`HIT`, `PIXEL` and `CELL` have them in both.)
- **Names.** Commands of two words, or with a string instead of numbers, have
  a name of their own:

  | ZX-V BASIC | ZX Basic |
  |---|---|
  | `GRAB n, w, a$` | `GRABSTR n, w, a$` |
  | `TILE s, n, a$` | `TILESTR s, n, a$` |
  | `MAP DIM`, `MAP POKE` | `MAPDIM`, `MAPPOKE` |
  | `MAP POKE m, x, y, a$` | `MAPPOKESTR m, x, y, a$` |
  | `FLOOR MAP`, `ROTATE MAP` | `FLOORMAP`, `ROTATEMAP` |
  | `PATTERN b1, ..., b8 [, p]` | `PATTERNBITS b1, ..., b8 [, p]` |
  | `REMAP` alone, `PALETTE` alone | `REMAPRESET`, `PALETTERESET` |
  | `LOAD f$ MUSIC` | `MUSICLOAD f$` |
  | `MUSIC PLAY`, `MUSIC STOP`, `MUSIC PAUSE`, `MUSIC CONTINUE` | `MUSICPLAY`, `MUSICSTOP`, `MUSICPAUSE`, `MUSICCONTINUE` |
  | `LOAD f$ SFX`, `SFX STOP`, the function `SFX` | `SFXLOAD f$`, `SFXSTOP`, `SFXPLAYING` |
  | `LOAD f$ SAMPLE n` | `SAMPLELOAD f$, n` |
  | `SAMPLE n, pitch, vol, pan; c` | `SAMPLE n, pitch, vol, pan, c` |
  | `SAMPLE STOP [c]`, the function `SAMPLE` | `SAMPLESTOP [c]`, `SAMPLEPLAYING` |

- **One at a time.** `REMAP` and `SOUND` take one pair, and `PALETTE` one
  colour; `REMAP` and `PALETTE` only change table 0 (no `#t`); `POLY` takes
  up to 8 points (64 there).
- **Not there:** `JOY n, mode` (a joystick as keys); the registers 64-159 of
  `SOUND` (the sample channels); MOD, S3M and XM in `MUSICLOAD` (without an
  extension, it only tries `.zxm`, `.pt3`, `.vgm` and `.vgz`): `zxv-music
  song.xm song.zxm` turns them into a `.zxm` first, so as not to carry the
  converter in every program.
- **The graphics belong to the program.** Sprites, tiles, maps and textures
  live in the heap of the program and go with it: no graphics store kept
  between programs, and no `SAVE`/`LOAD ... GRAPHICS`.
- **The Spectrum screen.** `PRINT`, `PLOT`, `DRAW`, `CIRCLE` and `CLS` only
  use the top left 256 × 192 pixels of the drawing page (in the ZX-V BASIC,
  the whole page: 64 × 48 characters in mode 1). `CLS` only clears that part;
  `MODE` or `BOX` clear the rest.
- **Small changes.** `PAGE` and `SCREEN` with a page that is not 0 or 1 give
  `B Integer out of range` (`A Invalid argument` there). `MODE` also takes
  the pattern away. In `FLOOR` and `FLOORMAP`, a `focal` of 0 is the default
  (an error there). The positions and angles of mode 7 are `Fixed`, not
  `Float`. The music stops when the program ends (it goes on there).
- **Names taken.** `SCREEN` can not be used with `screen.bas` (its `SCREEN$`
  is also `screen` for ZX Basic).

#### What the ZX-V BASIC has and ZX Basic does not

Neither `zxv.bas` nor the runtime have these: `MOUSE`; `RASTER`; `PLAY`;
`CLOCK`; `SPEED`; `LANGUAGE`; `MONITOR`; the files beyond `SAVE`, `LOAD` and
`VERIFY` (`CAT`, `CD` and `DIR$`, `MKDIR` and `RMDIR`, `ERASE`, `MOVE`,
`COPY`, `FORMAT`; `OPEN #`, `CLOSE #` and the streams to files; `MERGE`);
`SAVE`, `LOAD` and `VERIFY ... GRAPHICS`; `ON ERROR`, `ERR` and `ERL` (an
error stops the program); and the ports of the Kempston mouse and of the AY
chips in `IN` and `OUT` (use `SOUND`). The language is ZX Basic's: `SUB`,
`FUNCTION`, `DO ... LOOP` and the rest instead of `DEF PROC`, `REPEAT` or
`SELECT CASE` (see the [ZX Basic documentation](https://zxbasic.readthedocs.io)).

For saving the data of a game there is [`zxvsave.bas`](#zxvsavebas).

#### MODE

```
MODE m [, w, h]           ' m UByte, w UInteger, h UInteger
```

Chooses the screen mode: 0 (256 × 192) or 1 (512 × 384). In mode 0 the page
can be bigger than the screen, and `SCROLL` moves the screen over it. Both
pages are cleared with the `PAPER` colour, page 0 is shown and drawn on, the
`PRINT` position goes to the top, and the scroll, the colour changes
(`REMAP`, `PALETTE`: the four tables), `CLIP` and `PATTERN` go.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `m` | 0 or 1 | The mode. |
| `w`, `h` | mode 0: left out (or 256, 192), 512, 384, 1024, 192 or 256, 768; mode 1: left out or 512, 384 | The size of the pages. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Another mode, or another size. |

Same as [`MODE`][b-mode] in the ZX-V BASIC.

**Differences:** `MODE` also takes the `PATTERN` away. `PRINT`, `PLOT` and
`CLS` stay on the top left 256 × 192 pixels of the page, also in mode 1 and
on the big pages.

**Example**

```basic
#include <zxv.bas>
MODE 1: DISC 2, 256, 192, 150
WAIT 120: MODE 0
```

**Since:** v1.19.0-zxv.1.

**See also:** [SCROLL](#scroll), [PAGE](#page).

#### PAGE

```
PAGE n                    ' n UByte
```

The page that is shown, 0 or 1. Drawing on the hidden one (`SCREEN`) and
then showing it gives animations without flicker.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0 or 1 | The page. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `n` is not 0 or 1. |

Same as [`PAGE`][b-page] in the ZX-V BASIC.

**Differences:** a page that is not 0 or 1 gives `B Integer out of range`
(`A Invalid argument` there).

**Example**

```basic
#include <zxv.bas>
DIM s AS UByte = 1
DO
    SCREEN s: BOX 0, 0, 0, 256, 192: DISC 2, RND * 256, RND * 192, 20
    PAGE s: LET s = 1 - s: WAIT 10
LOOP
```

**Since:** v1.19.0-zxv.1.

**See also:** [SCREEN](#screen), [WAIT](#wait).

#### SCREEN

```
SCREEN n                  ' n UByte
```

The page everything draws on: `PRINT`, `PLOT`, `CLS`, the sprites, the
shapes, the maps... and the one `PIXEL` reads.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0 or 1 | The page. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `n` is not 0 or 1. |

Same as [`SCREEN`][b-page] in the ZX-V BASIC.

**Differences:** a page that is not 0 or 1 gives `B Integer out of range`
(`A Invalid argument` there). It can not be used with `screen.bas`.

**Example:** see [PAGE](#page).

**Since:** v1.19.0-zxv.1.

**See also:** [PAGE](#page).

#### SCROLL

```
SCROLL [x, y]             ' x Integer, y Integer
```

Pixel (`x`, `y`) of the page is seen at the top left corner of the screen,
without drawing anything again; the page wraps round at its edges. `SCROLL`
alone puts it back.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `x`, `y` | -32768 to 32767; 0, 0 if left out | The pixel of the page, taken modulo the size of the page. |

**Errors:** none.

Same as [`SCROLL`][b-scroll] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
MODE 0, 1024, 192: DISC 2, 600, 100, 50
FOR x = 0 TO 1023 STEP 2: SCROLL x, 0: WAIT: NEXT x
```

**Since:** v1.19.0-zxv.1.

**See also:** [MODE](#mode).

#### REMAP

```
REMAP c, n                ' c UByte, n UByte
```

Colour `c` is seen as colour `n`, at once and on all the screen, without
drawing again (table 0).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `c` | 0-15 | The colour that changes. |
| `n` | 0-15 | The colour it is seen as. |

**Errors**

| Error | When |
|---|---|
| `K Invalid colour` | `c` or `n` above 15. |

Same as [`REMAP`][b-remap] in the ZX-V BASIC.

**Differences:** one pair at a time, and only table 0 (no `#t`); `REMAP`
alone is [REMAPRESET](#remapreset).

**Example**

```basic
#include <zxv.bas>
PRINT INK 1; "blinking"
DO
    REMAP 1, 10: PAUSE 25: REMAP 1, 1: PAUSE 25
LOOP
```

**Since:** v1.19.0-zxv.1.

**See also:** [REMAPRESET](#remapreset), [PALETTE](#palette).

#### REMAPRESET

```
REMAPRESET
```

Every colour of table 0 is seen as itself again.

**Errors:** none.

Same as `REMAP` alone in the [ZX-V BASIC][b-remap].

**Differences:** only table 0 (the only one the program changes).

**Example**

```basic
#include <zxv.bas>
PRINT INK 1; "blue"
REMAP 1, 10: PAUSE 50: REMAPRESET
```

**Since:** v1.19.0-zxv.1.

**See also:** [REMAP](#remap).

#### PALETTE

```
PALETTE c, r, g, b        ' c UByte, r, g, b UInteger
```

Colour `c` is seen with that red, green and blue, the nearest one ZX-V shows
(RGB565), at once and on all the screen (table 0). The standard colours have
215 in each component, 255 the bright ones.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `c` | 0-15 | The colour. |
| `r`, `g`, `b` | 0-255 | Red, green and blue. |

**Errors**

| Error | When |
|---|---|
| `K Invalid colour` | `c` above 15. |
| `B Integer out of range` | `r`, `g` or `b` above 255. |

Same as [`PALETTE`][b-palette] in the ZX-V BASIC.

**Differences:** one colour at a time, and only table 0 (no `#t`);
`PALETTE` alone is [PALETTERESET](#palettereset).

**Example**

```basic
#include <zxv.bas>
FOR i = 0 TO 15: BOX i, i * 16, 0, 16, 176: NEXT i
FOR v = 0 TO 255 STEP 5: PALETTE 7, v, v, 255 - v: WAIT: NEXT v
```

**Since:** v1.19.0-zxv.5.

**See also:** [PALETTERESET](#palettereset), [`zxvfade.bas`](#zxvfadebas).

#### PALETTERESET

```
PALETTERESET
```

The standard colours again, in tables 0 to 2.

**Errors:** none.

Same as `PALETTE` alone in the [ZX-V BASIC][b-palette].

**Example**

```basic
#include <zxv.bas>
PRINT INK 1; "orange"
PALETTE 1, 255, 128, 0: PAUSE 50: PALETTERESET
```

**Since:** v1.19.0-zxv.5.

**See also:** [PALETTE](#palette).

#### WAIT

```
WAIT [n]                  ' n UInteger
```

Waits for the next frame, and until `n` frames have gone since the last
`WAIT`: a steady pace for games, whatever each turn of the loop takes. There
are 60 frames a second.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-65535; 1 if left out | Frames since the last `WAIT`. |

**Errors:** none.

Same as [`WAIT`][b-wait] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
FOR x = 0 TO 200: BOX 2, x, 80, 8, 8: WAIT: NEXT x
```

**Since:** v1.19.0-zxv.1.

**See also:** [TIME](#time).

#### GRAB

```
GRAB n, x, y, w, h [, t]  ' n UByte, x, y Integer, w, h UInteger, t Integer
```

Sprite `n` is that rectangle of the drawing page. What falls out of the page
is transparent, and colour 8 in the page becomes 0 (both are black). Defining
a sprite again replaces it.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-255 | The sprite. |
| `x`, `y` | -32768 to 32767 | The top left corner of the rectangle. |
| `w`, `h` | 1-1024 | Its size. |
| `t` | 0-15; -1 (none) if left out | The colour that becomes transparent. Any other value: none. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `w` or `h` is 0 or above 1024. |
| (common) | See [Errors common to every command](#zxvbas). |

Same as [`GRAB`][b-grab] in the ZX-V BASIC (its first form).

**Example**

```basic
#include <zxv.bas>
DISC 2, 8, 8, 7: GRAB 0, 0, 0, 16, 16, 0: CLS
SPRITE 0, 100, 80
```

**Since:** v1.19.0-zxv.1.

**See also:** [GRABSTR](#grabstr), [SPRITE](#sprite).

#### GRABSTR

```
GRABSTR n, w, a$          ' n UByte, w UInteger
```

Sprite `n` from a string, a character a pixel and `w` pixels a row: the hex
digits (`0`-`9`, `a`-`f`, `A`-`F`) are the colours, anything else (`.`, a
space) is transparent. A last row that is short is filled with transparent.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-255 | The sprite. |
| `w` | 1-1024 | The pixels of a row. |
| `a$` | 1 or more characters, up to 1024 rows | The pixels. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | `w` is 0 or above 1024, `a$` is empty, or it makes more than 1024 rows. |
| (common) | See [Errors common to every command](#zxvbas). |

Same as `GRAB n, width, a$` in the [ZX-V BASIC][b-grab].

**Differences:** its name, `GRABSTR`.

**Example**

```basic
#include <zxv.bas>
GRABSTR 0, 8, "..2222..22222222.222222...2222..."
FOR x = 0 TO 240 STEP 16: SPRITE 0, x, 80: NEXT x
```

**Since:** v1.19.0-zxv.1.

**See also:** [GRAB](#grab), [SPRITE](#sprite).

#### SPRITE

```
SPRITE n, x, y [, f [, c$ [, w, h]]]   ' n UByte, x, y Integer, f UByte, w, h UInteger
```

Draws sprite `n` with its top left corner at (`x`, `y`) on the drawing page,
its transparent pixels letting the background show, inside the `CLIP`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-255, a sprite defined | The sprite. |
| `x`, `y` | -32768 to 32767 | Where. |
| `f` | 0 (if left out), 1 mirrored left to right, 2 upside down, 3 both | Mirror. |
| `c$` | `""` (if left out) or hex digits | Colour `i` is drawn as the hex digit `i` of `c$` (counting from 0); any other character, or a string too short, leaves it. |
| `w`, `h` | both 0 (if left out: its own size), or 1-4096 each | Stretched or shrunk to that size. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Sprite `n` is not defined. |
| `B Integer out of range` | `f` above 3; `w` or `h` above 4096, or only one of them 0. |

Same as [`SPRITE`][b-sprite] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
GRABSTR 0, 4, "1111122112211111"
SPRITE 0, 10, 10: SPRITE 0, 30, 10, 0, "0a": SPRITE 0, 50, 10, 0, "", 32, 32
```

**Since:** v1.19.0-zxv.1.

**See also:** [HIT](#hit), [CLIP](#clip).

#### HIT

```
HIT(a, ax, ay, b, bx, by)   ' a, b UByte, the rest Integer; gives UByte
```

A function: 1 if a solid (not transparent) pixel of sprite `a` drawn at
(`ax`, `ay`) would fall on a solid pixel of sprite `b` drawn at (`bx`,
`by`), 0 if not. It draws nothing.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `a`, `b` | 0-255, sprites defined | The two sprites (they can be the same). |
| `ax`, `ay`, `bx`, `by` | -32768 to 32767 | Where each one would be. |

**Returns:** 1 or 0.

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | One of the sprites is not defined. |

Same as [`HIT`][b-hit] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
GRABSTR 0, 4, "1111111111111111"
PRINT HIT(0, 0, 0, 0, 3, 3); HIT(0, 0, 0, 0, 4, 0)
```

shows `10`.

**Since:** v1.19.0-zxv.1.

**See also:** [SPRITE](#sprite), [PIXEL](#pixel).

#### BOX

```
BOX c, x, y, w, h         ' c UByte, the rest Integer
```

A filled rectangle of colour `c` with its top left corner at (`x`, `y`),
with the `CLIP` and the `PATTERN`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `c` | 0-15 | The colour. |
| `x`, `y` | -32768 to 32767 | The top left corner. |
| `w`, `h` | -32768 to 32767 | The size (nothing is drawn with 0 or less). |

**Errors**

| Error | When |
|---|---|
| `K Invalid colour` | `c` above 15. |

Same as [`BOX`][b-box] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
BOX 2, 10, 10, 100, 50
```

**Since:** v1.19.0-zxv.1.

**See also:** [DISC](#disc), [POLY](#poly), [PATTERN](#pattern).

#### DISC

```
DISC c, x, y, r           ' c UByte, the rest Integer
```

A filled circle of colour `c` with its centre at (`x`, `y`) and radius `r`,
with the `CLIP` and the `PATTERN`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `c` | 0-15 | The colour. |
| `x`, `y` | -32768 to 32767 | The centre. |
| `r` | -32768 to 32767 | The radius. |

**Errors**

| Error | When |
|---|---|
| `K Invalid colour` | `c` above 15. |

Same as [`DISC`][b-disc] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
DISC 6, 128, 96, 40
```

**Since:** v1.19.0-zxv.1.

**See also:** [BOX](#box), [PATTERN](#pattern).

#### POLY

```
POLY c, x1, y1 [, x2, y2 ... , x8, y8]   ' c UByte, the rest Integer
```

With one point, a pixel; with two, a line; with three or more, a filled
polygon. With the `CLIP` and the `PATTERN`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `c` | 0-15 | The colour. |
| `x1`, `y1` ... `x8`, `y8` | -32767 to 32767 | The points, up to 8. An `x` of -32768 ends the list there (it is the value of the points left out). |

**Errors**

| Error | When |
|---|---|
| `K Invalid colour` | `c` above 15. |
| `A Invalid argument` | `x1` is -32768 (no point). |

Same as [`POLY`][b-poly] in the ZX-V BASIC.

**Differences:** up to 8 points (64 there).

**Example**

```basic
#include <zxv.bas>
POLY 4, 128, 20, 200, 150, 56, 150
```

**Since:** v1.19.0-zxv.1.

**See also:** [BOX](#box), [PATTERN](#pattern).

#### PATTERN

```
PATTERN [n [, p]]         ' n UByte, p Integer
```

`BOX`, `DISC` and `POLY` (and the text of `FONTDRAW`) draw with pattern `n`:
their colour where the pattern has a 1, and paper `p` where it has a 0, or
nothing there (what is under shows through). The pattern is fixed to the
page, not to the shape. `PATTERN` alone, or `PATTERN 0`, goes back to plain
colour; so does `MODE`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0 (if left out: none) to 11 | 1 a chessboard (a half), 2 a quarter, 3 an eighth, 4 three quarters, 5 horizontal stripes, 6 vertical stripes, 7 a diagonal going up, 8 one going down, 9 a grid, 10 bricks, 11 diamonds. |
| `p` | 0-15, or -1 (if left out) | The paper; -1, nothing under the 0s. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `n` above 11. |
| `K Invalid colour` | `p` below -1 or above 15. |

**ROM:** needs a ZX-V ROM of specification 0.2 (ZX-V 0.11.0 or later).
With an older one the shapes come out in plain colour.

Same as [`PATTERN`][b-pattern] in the ZX-V BASIC (its first form).

**Differences:** `MODE` also takes it away (`RUN` and `CLEAR` there). The
pattern of your own is [PATTERNBITS](#patternbits).

**Example**

```basic
#include <zxv.bas>
PATTERN 10, 2: BOX 15, 32, 96, 192, 96
PATTERN 1: DISC 0, 128, 96, 40: PATTERN
```

**Since:** v1.19.0-zxv.7.

**See also:** [PATTERNBITS](#patternbits), [BOX](#box).

#### PATTERNBITS

```
PATTERNBITS r0, r1, r2, r3, r4, r5, r6, r7 [, p]   ' r0-r7 UByte, p Integer
```

A pattern of your own for `BOX`, `DISC` and `POLY`, of 8 × 8 pixels: a
byte a row, from the top, with bit 7 the pixel on the left (`BIN 10101010`).
The paper is as in [PATTERN](#pattern).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `r0` ... `r7` | 0-255 | The rows. |
| `p` | 0-15, or -1 (if left out) | The paper. |

**Errors**

| Error | When |
|---|---|
| `K Invalid colour` | `p` below -1 or above 15. |

**ROM:** as [PATTERN](#pattern).

Same as `PATTERN b1, ..., b8 [, paper]` in the [ZX-V BASIC][b-pattern].

**Differences:** its name, `PATTERNBITS`.

**Example**

```basic
#include <zxv.bas>
PATTERNBITS BIN 11110000, BIN 11110000, BIN 11110000, BIN 11110000, 15, 15, 15, 15, 1
BOX 6, 64, 48, 128, 96: PATTERN
```

**Since:** v1.19.0-zxv.7.

**See also:** [PATTERN](#pattern).

#### CLIP

```
CLIP [x, y, w, h]         ' all Integer
```

From now on `SPRITE`, `BOX`, `DISC`, `POLY`, `MAP`, `FLOOR`, `ROTATE` and the
libraries that draw (fonts, 3D) only draw inside that rectangle of the page.
`CLIP` alone, or with a `w` below 0, draws everywhere again; so does `MODE`.
`PRINT` and `PLOT` do not use it.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `x`, `y` | -32768 to 32767; 0 if left out | The top left corner. |
| `w`, `h` | -32768 to 32767; `w` -1 and `h` 0 if left out | The size. Give the four: with `w` and not `h`, the rectangle has no height and nothing is drawn. |

**Errors:** none.

Same as [`CLIP`][b-clip] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
CLIP 50, 50, 100, 50: DISC 2, 100, 75, 60: CLIP
```

**Since:** v1.19.0-zxv.1.

**See also:** [BOX](#box), [MAP](#map).

#### PIXEL

```
PIXEL(x, y)               ' x, y Integer; gives Byte
```

A function: the colour of a pixel of the drawing page, whoever drew it, as it
is in the video memory (without `REMAP`).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `x`, `y` | -32768 to 32767 | The pixel. |

**Returns:** the colour, 0-15; -1 out of the page.

**Errors:** none.

Same as [`PIXEL`][b-pixel] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
BOX 4, 10, 10, 20, 20: PRINT PIXEL(15, 15); " "; PIXEL(5, 5); " "; PIXEL(-1, 0)
```

**Since:** v1.19.0-zxv.1.

**See also:** [HIT](#hit).

#### TILE

```
TILE s, n, x, y [, w, h [, t]]   ' s UByte, n UInteger, x, y Integer, w, h UInteger, t Integer
```

Defines tiles for the maps from the drawing page. There are two sets, one of
8 × 8 tiles and one of 16 × 16, with tiles 0 to 4095. Without `w` and `h`,
tile `n` is the `s` × `s` square at (`x`, `y`); with them, that rectangle
is cut in tiles numbered from `n`, row by row. A set grows when a higher
tile is defined, and the new ones are black. What falls out of the page is
transparent, and colour 8 in the page becomes 0.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `s` | 8 or 16 | The set. |
| `n` | 0-4095 | The first tile. |
| `x`, `y` | -32768 to 32767 | The top left corner. |
| `w`, `h` | both 0 (if left out: one tile), or `s` to 4096 each | The rectangle: `w / s` × `h / s` tiles. |
| `t` | -1 (if left out: none) or 0-15 | The colour that becomes transparent. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | `s` is not 8 or 16. |
| `B Integer out of range` | `w` or `h` below `s` or above 4096; the last tile would pass 4095. |
| `K Invalid colour` | `t` below -1 or above 15. |
| (common) | See [Errors common to every command](#zxvbas). |

Same as [`TILE`][b-tile] in the ZX-V BASIC (its first form).

**Example**

```basic
#include <zxv.bas>
BOX 2, 0, 0, 8, 8: POLY 7, 0, 3, 7, 3: TILE 8, 1, 0, 0
```

**Since:** v1.19.0-zxv.7.

**See also:** [TILESTR](#tilestr), [MAPDIM](#mapdim), [MAP](#map).

#### TILESTR

```
TILESTR s, n, a$          ' s UByte, n UInteger
```

Tiles from a string, as in [GRABSTR](#grabstr): `s` × `s` characters a tile,
the hex digits are colours and anything else is transparent. A longer string
defines the next tiles; a last tile that is short is filled with transparent.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `s` | 8 or 16 | The set. |
| `n` | 0-4095 | The first tile. |
| `a$` | 1 or more characters | The pixels. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | `s` is not 8 or 16, or `a$` is empty. |
| `B Integer out of range` | The last tile would pass 4095. |
| (common) | See [Errors common to every command](#zxvbas). |

Same as `TILE size, n, a$` in the [ZX-V BASIC][b-tile].

**Differences:** its name, `TILESTR`.

**Example**

```basic
#include <zxv.bas>
TILESTR 8, 1, "6666666666666666" + "6..66..66..66..6" + "6666666666666666" + "6..66..66..66..6"
```

**Since:** v1.19.0-zxv.7.

**See also:** [TILE](#tile).

#### MAPDIM

```
MAPDIM m, w, h [, s [, r]]   ' m UByte, w, h UInteger, s UByte, r UByte
```

Map `m` has `w` × `h` cells, all tile 0, of the set of side `s`; with `r`
= 1 it repeats for ever in both directions. A map defined again is replaced.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `m` | 0-15 | The map. |
| `w`, `h` | 1-32767 | Its cells across and down. |
| `s` | 8 (if left out) or 16 | The set of its tiles. |
| `r` | 0 (if left out) or 1 | 1: it repeats. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `m` above 15; `w` or `h` 0 or above 32767; `r` above 1. |
| `A Invalid argument` | `s` is not 8 or 16. |
| (common) | See [Errors common to every command](#zxvbas): a map takes `w` × `h` × 2 bytes. |

Same as `MAP DIM` in the [ZX-V BASIC][b-mapdim].

**Differences:** its name, `MAPDIM`.

**Example:** see [MAP](#map).

**Since:** v1.19.0-zxv.7.

**See also:** [MAPPOKE](#mappoke), [MAP](#map), [CELL](#cell).

#### MAPPOKE

```
MAPPOKE m, x, y, v        ' m UByte, x, y UInteger, v UInteger
```

Cell (`x`, `y`) of map `m` is `v`: the tile, plus any of 4096 (mirrored
left to right), 8192 (upside down), 16384 (its colour 8 is transparent) and
32768 (priority, for the filter of [MAP](#map)).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `m` | 0-15, a map defined | The map. |
| `x`, `y` | 0 to its width - 1, 0 to its height - 1 | The column and the row. |
| `v` | 0-65535 | The tile (0-4095) and its bits. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Map `m` is not defined. |
| `B Integer out of range` | `m` above 15, or `x` or `y` out of the map. |

Same as `MAP POKE m, x, y, value` in the [ZX-V BASIC][b-mapdim].

**Differences:** its name, `MAPPOKE`.

**Example:** see [MAP](#map).

**Since:** v1.19.0-zxv.7.

**See also:** [MAPPOKESTR](#mappokestr), [CELL](#cell).

#### MAPPOKESTR

```
MAPPOKESTR m, x, y, a$    ' m UByte, x, y UInteger
```

The cells from (`x`, `y`) to the right, the code of each character as the
tile: `"  ##.."` uses tiles 32, 35 and 46. What passes the right edge of the
map is left out.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `m` | 0-15, a map defined | The map. |
| `x`, `y` | in the map | The first cell. |
| `a$` | any string | The tiles, 0-255. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Map `m` is not defined. |
| `B Integer out of range` | `m` above 15, or `x` or `y` out of the map. |

Same as `MAP POKE m, x, y, a$` in the [ZX-V BASIC][b-mapdim].

**Differences:** its name, `MAPPOKESTR`.

**Example:** see [MAP](#map).

**Since:** v1.19.0-zxv.7.

**See also:** [MAPPOKE](#mappoke).

#### MAP

```
MAP m [, mx, my [, x, y, w, h [, f]]]   ' m UByte, mx, my, x, y, w, h Integer, f UByte
```

Draws map `m` from its pixel (`mx`, `my`) on the whole page or in the
rectangle (`x`, `y`, `w`, `h`), with the `CLIP`. Changing `mx` and `my`
scrolls it. Every tile up to the highest one the map has had must be
defined.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `m` | 0-15, a map defined | The map. |
| `mx`, `my` | -32768 to 32767; 0 if left out | The pixel of the map at the top left corner. |
| `x`, `y` | -32768 to 32767; 0 if left out | Where on the page. |
| `w`, `h` | -32768 to 32767; -1 if left out | The size; below 0, to the edge of the page. |
| `f` | 0 (if left out) every cell, 1 those without priority, 2 only those with it | The filter: the background, then the sprites, then what goes in front. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Map `m` is not defined, or a tile it uses is not. |
| `B Integer out of range` | `m` above 15, or `f` above 2. |

Same as [`MAP`][b-map] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
BOX 5, 0, 0, 8, 8: TILE 8, 32, 0, 0
BOX 2, 0, 0, 8, 8: POLY 7, 0, 3, 7, 3: TILE 8, 35, 0, 0
MAPDIM 0, 40, 1: MAPPOKESTR 0, 0, 0, "####  ####   ######  #########   ####"
FOR x = 0 TO 64: MAP 0, x, 0, 0, 100, 256, 8: WAIT: NEXT x
```

**Since:** v1.19.0-zxv.7.

**See also:** [MAPDIM](#mapdim), [CELL](#cell), [FLOORMAP](#floormap),
[`zxvmapmove.bas`](#zxvmapmovebas).

#### CELL

```
CELL(m, x, y)             ' m UByte, x, y Integer; gives UInteger
```

A function: the cell at column `x` and row `y` of map `m`, with its bits.
In a map that repeats, any `x` and `y` fall in it.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `m` | 0-15, a map defined | The map. |
| `x`, `y` | -32768 to 32767 | The column and the row. |

**Returns:** the value of the cell, 0-65535 (the tile and the bits of
[MAPPOKE](#mappoke)); 0 out of a map that does not repeat. The tile alone is
`CELL(m, x, y) BAND 4095`.

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Map `m` is not defined. |
| `B Integer out of range` | `m` above 15. |

Same as [`CELL`][b-cell] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
MAPDIM 0, 4, 4: MAPPOKE 0, 1, 2, 35 + 4096
PRINT CELL(0, 1, 2); " "; CELL(0, 1, 2) BAND 4095; " "; CELL(0, 9, 9)
```

**Since:** v1.19.0-zxv.7.

**See also:** [MAPPOKE](#mappoke), [`MAPKIND`](#mapkind).

#### TEXTURE

```
TEXTURE n, x, y, s [, c [, t]]   ' n UByte, x, y Integer, s UInteger, c, t Integer
```

Texture `n` for [FLOOR](#floor) and [ROTATE](#rotate) is the `s` × `s`
square of the drawing page at (`x`, `y`). It repeats for ever or, with `c`,
everything outside it is that colour. With `t`, that colour is transparent
(and a colour 8 of the page becomes 0); with `c` the same as `t`, the
outside is not drawn either. What falls out of the page is black.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-3 | The texture. |
| `x`, `y` | -32768 to 32767 | The top left corner. |
| `s` | 2, 4, 8, 16, 32, 64, 128 or 256 | The side. |
| `c` | -1 (if left out: it repeats) or 0-15 | The colour outside. |
| `t` | -1 (if left out: none) or 0-15 | The transparent colour. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `n` above 3. |
| `A Invalid argument` | `s` is not a power of 2 from 2 to 256. |
| `K Invalid colour` | `c` or `t` below -1 or above 15. |
| (common) | See [Errors common to every command](#zxvbas): a texture takes `s` × `s` bytes. |

Same as [`TEXTURE`][b-texture] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
PAPER 0: CLS: DISC 6, 16, 16, 15: DISC 2, 16, 16, 8: TEXTURE 1, 0, 0, 32, 0, 0
```

**Since:** v1.19.0-zxv.7.

**See also:** [FLOOR](#floor), [ROTATE](#rotate).

#### FLOOR

```
FLOOR n, x, y, height, angle, horizon [, focal]
' n UByte, x, y, height, angle, focal Fixed, horizon Integer
```

A floor in perspective with texture `n` (the "mode 7"), seen from its point
(`x`, `y`) at that `height`, looking at `angle`, with the horizon at that
row of the page: the floor fills from the row below it to the bottom of the
page, with the `CLIP`. The camera stays for [`zxvm7.bas`](#zxvm7bas).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-3, a texture defined | The texture. |
| `x`, `y` | `Fixed` | The point of the texture (in texels) under the camera. |
| `height` | `Fixed`, above 0 | How high the camera is. |
| `angle` | `Fixed`, radians | Where it looks: 0 towards the growing `x`. |
| `horizon` | -32768 to 32767 | The row of the horizon. |
| `focal` | `Fixed`, 0 or more; 0 (if left out) is half the width of the page | The focal length: smaller, a wider view. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Texture `n` is not defined. |
| `B Integer out of range` | `n` above 3; `height` 0 or less; `focal` below 0. |

Same as [`FLOOR`][b-floor] in the ZX-V BASIC (with a texture).

**Differences:** the numbers are `Fixed` (up to 32767), not `Float`; a
`focal` of 0 is the default (an error there).

**Example**

```basic
#include <zxv.bas>
DIM a AS Fixed
FOR i = 0 TO 7: BOX i, i * 8, 0, 8, 64: NEXT i: TEXTURE 0, 0, 0, 64
FOR a = 0 TO 6.28 STEP 0.05: FLOOR 0, 32, 32, 20, a, 80: WAIT: NEXT a
```

**Since:** v1.19.0-zxv.7.

**See also:** [FLOORMAP](#floormap), [TEXTURE](#texture),
[`FLOORPROJECT`](#floorproject).

#### FLOORMAP

```
FLOORMAP m, x, y, height, angle, horizon [, focal]
' m UByte, x, y, height, angle, focal Fixed, horizon Integer
```

The same as [FLOOR](#floor) with tile map `m` as the floor (a whole race
track fits in a map): each cell keeps its mirrors and transparency, a map
that repeats repeats on the floor, and outside one that does not, nothing is
drawn. It costs more than a texture.

**Parameters:** as [FLOOR](#floor), with `m` (0-15, a map defined, with all
its tiles) instead of `n`.

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Map `m` is not defined, or a tile it uses is not. |
| `B Integer out of range` | `m` above 15; `height` 0 or less; `focal` below 0. |

**ROM:** needs a ZX-V ROM of specification 0.2 (ZX-V 0.11.0 or later).
With an older one it draws nothing.

Same as `FLOOR MAP` in the [ZX-V BASIC][b-floor].

**Differences:** its name, `FLOORMAP`; the numbers are `Fixed`; a `focal`
of 0 is the default.

**Example**

```basic
#include <zxv.bas>
DIM a AS Fixed
BOX 4, 0, 0, 8, 8: TILE 8, 0, 0, 0: BOX 2, 0, 0, 8, 8: TILE 8, 1, 0, 0
MAPDIM 0, 2, 2, 8, 1: MAPPOKE 0, 0, 0, 1: MAPPOKE 0, 1, 1, 1
FOR a = 0 TO 6.28 STEP 0.05: FLOORMAP 0, 8, 8, 10, a, 95: WAIT: NEXT a
```

**Since:** v1.19.0-zxv.7.

**See also:** [FLOOR](#floor), [MAPDIM](#mapdim).

#### ROTATE

```
ROTATE n, x, y [, angle [, zoom [, cx, cy]]]
' n UByte, x, y, angle, zoom Fixed, cx, cy Integer
```

Texture `n` turned `angle` radians clockwise and `zoom` times bigger around
its point (`x`, `y`), which is seen at (`cx`, `cy`) of the page, over the
whole page or the `CLIP`. Without turning or zooming, it scrolls the
texture.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-3, a texture defined | The texture. |
| `x`, `y` | `Fixed` | The point of the texture. |
| `angle` | `Fixed`, radians; 0 if left out | The turn. |
| `zoom` | `Fixed`, above 0; 1 if left out | 2, twice as big; 0.5, half. |
| `cx`, `cy` | -32767 to 32767; the centre of the page if left out (-32768) | Where the point is seen. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Texture `n` is not defined. |
| `B Integer out of range` | `n` above 3, or `zoom` 0 or less. |

**ROM:** needs a ZX-V ROM of specification 0.2 (ZX-V 0.11.0 or later).
With an older one the ROM has no `m7_affine` (the call does nothing), so the
plane is not turned: do not use it there.

Same as [`ROTATE`][b-rotate] in the ZX-V BASIC (with a texture).

**Differences:** the numbers are `Fixed`, not `Float`.

**Example**

```basic
#include <zxv.bas>
DIM a AS Fixed: DIM s AS UByte = 1
PAPER 0: CLS: DISC 6, 16, 16, 15: DISC 2, 16, 16, 8: BOX 1, 12, 4, 8, 24
TEXTURE 1, 0, 0, 32, 0, 0
FOR a = 0 TO 6.28 STEP 0.05
    SCREEN s: BOX 5, 0, 0, 256, 192: ROTATE 1, 16, 16, a, 2
    PAGE s: LET s = 1 - s: WAIT
NEXT a
```

**Since:** v1.19.0-zxv.7.

**See also:** [ROTATEMAP](#rotatemap), [TEXTURE](#texture).

#### ROTATEMAP

```
ROTATEMAP m, x, y [, angle [, zoom [, cx, cy]]]
' m UByte, x, y, angle, zoom Fixed, cx, cy Integer
```

The same as [ROTATE](#rotate) with tile map `m`. What is out of a map that
does not repeat is not drawn.

**Parameters:** as [ROTATE](#rotate), with `m` (0-15, a map defined, with
all its tiles) instead of `n`.

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Map `m` is not defined, or a tile it uses is not. |
| `B Integer out of range` | `m` above 15, or `zoom` 0 or less. |

**ROM:** needs a ZX-V ROM of specification 0.2 (ZX-V 0.11.0 or later).
With an older one it draws nothing.

Same as `ROTATE MAP` in the [ZX-V BASIC][b-rotate].

**Differences:** its name, `ROTATEMAP`; the numbers are `Fixed`.

**Example**

```basic
#include <zxv.bas>
DIM a AS Fixed
BOX 4, 0, 0, 8, 8: TILE 8, 0, 0, 0: BOX 2, 0, 0, 8, 8: TILE 8, 1, 0, 0
MAPDIM 0, 2, 2, 8, 1: MAPPOKE 0, 0, 0, 1: MAPPOKE 0, 1, 1, 1
FOR a = 0 TO 6.28 STEP 0.05: ROTATEMAP 0, 0, 0, a, 3: WAIT: NEXT a
```

**Since:** v1.19.0-zxv.7.

**See also:** [ROTATE](#rotate), [MAPDIM](#mapdim).

#### KEY

```
KEY(n)                    ' n UInteger; gives UByte
```

A function: 1 if the key is held down now, 0 if not. Several keys can be
held at once: the way to read the keyboard in games.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | `CODE "a"` to `CODE "z"` (or capitals), `CODE "0"` to `CODE "9"`, 8 to 11 (the cursor keys left, right, down, up), 12 Backspace, 13 Enter, 32 Space, or 256 + a USB key code (256-511) | The key. |

**Returns:** 1 or 0.

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | `n` is no key. |

Same as [`KEY`][b-key] in the ZX-V BASIC (the same codes and result).

**Differences:** it goes in brackets, `KEY(9)`, not `KEY 9`.

**Example**

```basic
#include <zxv.bas>
DIM x AS Integer = 128
DO
    LET x = x + KEY(9) - KEY(8): PLOT x, 88: WAIT
LOOP
```

**Since:** v1.19.0-zxv.1.

**See also:** [JOY](#joy), [`zxvinput.bas`](#zxvinputbas).

#### JOY

```
JOY(n)                    ' n UByte; gives UInteger
```

A function: the buttons of joystick or gamepad `n` held now, as a sum of
bits; 0 if it is not connected.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0 to 3 | The joystick (0, the first connected; 1, the second...). |

**Returns:** 0-4095: 1 right, 2 left, 4 down, 8 up, 16 fire 1, 32 fire 2,
64 fire 3, 128 start, 256 select, 512 fire 4, 1024 L and 2048 R (the upper
shoulder buttons).

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `n` is not 0 to 3. |

Same as the function [`JOY`][b-joy] of the ZX-V BASIC.

**Differences:** it goes in brackets, `JOY(0)`; there is no statement `JOY n,
mode` (a joystick as keys for `INKEY$` and `IN`).

**Example**

```basic
#include <zxv.bas>
DO
    PRINT AT 0, 0; JOY(0); "   "
    IF JOY(0) BAND 16 THEN PRINT AT 1, 0; "fire"
    WAIT
LOOP
```

**Since:** v1.19.0-zxv.1.

**See also:** [KEY](#key), [`zxvinput.bas`](#zxvinputbas).

#### SOUND

```
SOUND r, v                ' r UByte, v UByte
```

Writes `v` in register `r` of the AY chips: 0-15 those of the first chip,
16-31 those of the second (the same, 16 higher). What `SOUND` writes on the
chip the music uses lasts until the music writes that register again.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `r` | 0-31 | The register. |
| `v` | 0-255 | The value. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `r` above 31. |

Same as [`SOUND`][b-sound] in the ZX-V BASIC (the meaning of each register
is there).

**Differences:** one pair at a time; the registers 64-159 of the sample
channels are not there.

**Example**

```basic
#include <zxv.bas>
SOUND 7, 62: SOUND 8, 15: SOUND 0, 200: SOUND 1, 0
PAUSE 25: SOUND 8, 0
```

**Since:** v1.19.0-zxv.1.

**See also:** [SFX](#sfx), [MUSICPLAY](#musicplay).

#### TIME

```
TIME                      ' gives ULong
```

A function: the frames since the machine started, 60 a second.

**Returns:** 0 to 4294967295.

**Errors:** none.

Same as [`TIME`][b-time] in the ZX-V BASIC.

**Example**

```basic
#include <zxv.bas>
DIM t AS ULong = TIME
PAUSE 100: PRINT (TIME - t) / 60; " seconds"
```

**Since:** v1.19.0-zxv.1.

**See also:** [WAIT](#wait).

#### MUSICLOAD

```
MUSICLOAD f$
```

Loads a music module, to play it with [MUSICPLAY](#musicplay): ZX-V's own
`.zxm` (the two AY chips and the sample channels: what `zxv-tracker` and
`zxv-music` save), Pro Tracker 3 or Vortex Tracker II (`.pt3`), a
TurboSound (two modules, one on each AY chip), Pro Tracker 2, Sound
Tracker, ASC Sound Master, a PSG dump or a VGM of AY chips (`.vgm`, or
compressed: `.vgz`). Without an extension, the first there is of `f$.zxm`,
`f$.pt3`, `f$.vgm` and `f$.vgz`. It stops the music, takes the place of the
module there was, and goes in the heap (a `.vgz` takes 32 KB more, once, to
decompress it while it plays). The samples of a `.zxm` go to the sound
memory, from the bottom. The file can also be inside the program
(`--embed`).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `f$` | a name of 1-63 characters, with a drive and folders if needed | The file. |

**Errors**

| Error | When |
|---|---|
| `F Invalid file name` | `f$` is empty or has 64 characters or more. |
| `T File not found` | It is not there (nor inside the program). |
| `U No SD card` | It goes to the card and there is none. |
| `V Disk error (no server for EXT:)` | It goes to `EXT:` and nobody serves it. |
| `V Disk error` | The card failed. |
| `R Loading error` | The file is empty, can not be read, or is no module. |
| `4 Out of memory` | No room in the heap; or the samples of a `.zxm` do not fit in the sound memory with those of `SAMPLELOAD`. |

Same as `LOAD name$ MUSIC` in the [ZX-V BASIC][b-music].

**Differences:** its name, `MUSICLOAD f$`. No MOD, S3M or XM: turn them
into a `.zxm` with `zxv-music` (so as not to carry the converter in every
program), and without an extension it does not look for them.

**Example**

```basic
#include <zxv.bas>
MUSICLOAD "tune": MUSICPLAY
PRINT "Playing..."
PAUSE 0: MUSICSTOP
```

**Since:** v1.19.0-zxv.3 (`.zxm`, `.vgm` and `.vgz` since v1.19.0-zxv.6).

**See also:** [MUSICPLAY](#musicplay), [SAMPLELOAD](#sampleload).

#### MUSICPLAY

```
MUSICPLAY
```

Plays the module from the start, in the background while the program goes
on, and at its end goes back to its loop position, for ever. An AY module
plays on the first chip (a TurboSound or a VGM of two chips, on both); a
`.zxm`, on the chips and sample channels its tracks use. It goes on until
`MUSICSTOP` or the end of the program. While it plays, `BEEP` goes as far
ahead as the music (35 ms), so the order of the sounds is kept (with ZX-V
0.6.0 or later; before, `BEEP` is silent while the music plays).

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | No module was loaded. |
| `R Loading error` | A `.vgz` that is not a VGM with an AY chip. |

Same as `MUSIC PLAY` in the [ZX-V BASIC][b-music].

**Differences:** its name, `MUSICPLAY`; the music stops when the program
ends.

**Example:** see [MUSICLOAD](#musicload).

**Since:** v1.19.0-zxv.3.

**See also:** [MUSICSTOP](#musicstop), [MUSIC](#music).

#### MUSICSTOP

```
MUSICSTOP
```

Stops the music. `MUSICPLAY` starts it again from the beginning.

**Errors:** none.

Same as `MUSIC STOP` in the [ZX-V BASIC][b-music].

**Differences:** its name, `MUSICSTOP`.

**Example:** see [MUSICLOAD](#musicload).

**Since:** v1.19.0-zxv.3.

**See also:** [MUSICPAUSE](#musicpause).

#### MUSICPAUSE

```
MUSICPAUSE
```

Silences the music; [MUSICCONTINUE](#musiccontinue) goes on where it was.

**Errors:** none.

Same as `MUSIC PAUSE` in the [ZX-V BASIC][b-music].

**Differences:** its name, `MUSICPAUSE`.

**Example**

```basic
#include <zxv.bas>
MUSICLOAD "tune": MUSICPLAY
PAUSE 100: MUSICPAUSE: PAUSE 50: MUSICCONTINUE
```

**Since:** v1.19.0-zxv.3.

**See also:** [MUSICCONTINUE](#musiccontinue), [MUSIC](#music).

#### MUSICCONTINUE

```
MUSICCONTINUE
```

The music goes on where `MUSICPAUSE` left it.

**Errors:** none.

Same as `MUSIC CONTINUE` in the [ZX-V BASIC][b-music].

**Differences:** its name, `MUSICCONTINUE`.

**Example:** see [MUSICPAUSE](#musicpause).

**Since:** v1.19.0-zxv.3.

**See also:** [MUSICPAUSE](#musicpause).

#### MUSIC

```
MUSIC                     ' gives UByte
```

A function: 1 while the music plays, 0 if not (also while it is paused).

**Returns:** 1 or 0.

**Errors:** none.

Same as the function `MUSIC` of the [ZX-V BASIC][b-music].

**Example**

```basic
#include <zxv.bas>
MUSICLOAD "tune": MUSICPLAY
PRINT MUSIC: MUSICSTOP: PRINT MUSIC
```

**Since:** v1.19.0-zxv.3.

**See also:** [MUSICPLAY](#musicplay).

#### SFXLOAD

```
SFXLOAD f$
```

Loads a bank of AYFX sound effects (`.afb`: what the effects tab of
`zxv-tracker` or the AY Sound FX Editor saves; without an extension,
`f$.afb`), or a single effect (`.afx`, as a bank of one). It stops the
effects, takes the place of the bank there was, and goes in the heap. The
file can also be inside the program (`--embed`).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `f$` | a name of 1-63 characters | The file. |

**Errors**

| Error | When |
|---|---|
| `F Invalid file name`, `T File not found`, `U No SD card`, `V Disk error...` | As in [MUSICLOAD](#musicload). |
| `R Loading error` | The file is empty, can not be read, or is no bank. |
| `4 Out of memory` | No room in the heap. |

Same as `LOAD name$ SFX` in the [ZX-V BASIC][b-sfx].

**Differences:** its name, `SFXLOAD f$`.

**Example**

```basic
#include <zxv.bas>
SFXLOAD "effects"
DO
    IF INKEY$ = " " THEN SFX 0
LOOP
```

**Since:** v1.19.0-zxv.3.

**See also:** [SFX](#sfx).

#### SFX

```
SFX n [, c]               ' n UByte, c Byte
```

Plays effect `n` of the bank without waiting, on a free channel of the chip
the music does not use (if there is none, on the one that has played
longest), or on channel `c`. An effect on a channel of the music covers it
while it plays.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0 to the effects of the bank - 1 | The effect (the first is 0). |
| `c` | -1 (if left out: a free one), 0-2 the channels A, B and C of the first chip, 3-5 those of the second | The channel. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | No bank was loaded. |
| `B Integer out of range` | The effect is not in the bank, or `c` is out of -1 to 5. |

Same as [`SFX`][b-sfx] in the ZX-V BASIC.

**Example:** see [SFXLOAD](#sfxload).

**Since:** v1.19.0-zxv.3.

**See also:** [SFXSTOP](#sfxstop), [SFXPLAYING](#sfxplaying).

#### SFXSTOP

```
SFXSTOP
```

Stops all the effects.

**Errors:** none.

Same as `SFX STOP` in the [ZX-V BASIC][b-sfx].

**Differences:** its name, `SFXSTOP`.

**Example:** `SFX 0: PAUSE 10: SFXSTOP`.

**Since:** v1.19.0-zxv.3.

**See also:** [SFX](#sfx).

#### SFXPLAYING

```
SFXPLAYING                ' gives UByte
```

A function: 1 while any effect plays, 0 if not.

**Returns:** 1 or 0.

**Errors:** none.

Same as the function `SFX` of the [ZX-V BASIC][b-sfx].

**Differences:** its name, `SFXPLAYING`.

**Example**

```basic
#include <zxv.bas>
SFXLOAD "effects": SFX 0
DO
    WAIT
LOOP WHILE SFXPLAYING
PRINT "done"
```

**Since:** v1.19.0-zxv.3.

**See also:** [SFX](#sfx).

#### SAMPLELOAD

```
SAMPLELOAD f$, n          ' n UByte
```

Loads a WAV as sound `n` in the sound memory (96 KB, shared with the samples
of a `.zxm`): 8 or 16 bits, mono or stereo (mixed), at any rate up to 1 MHz,
with its loop if it has one (a `smpl` chunk, the one Audacity saves). It is
kept in 8 bits; nothing of it stays in the RAM. Without an extension,
`f$.wav`. The `SAMPLE` sounds go from the top of the sound memory, below the
ones there are (a sound loaded again stays in its place if it fits); the
samples of a `.zxm` go from the bottom. While it loads, the `SAMPLE` sounds
stop. The file can also be inside the program (`--embed`).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `f$` | a name of 1-63 characters | The file. |
| `n` | 0-31 | The sound. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `n` above 31. |
| `F Invalid file name`, `T File not found`, `U No SD card`, `V Disk error...` | As in [MUSICLOAD](#musicload). |
| `R Loading error` | It is not a WAV of 8 or 16 bits, mono or stereo, PCM; or it has no sound. |
| `4 Out of memory` | It does not fit in the sound memory (with the other sounds and the samples of the module); no 4 KB free in the heap to load it. |

**ROM:** the sample channels need ZX-V 0.10.0 or later.

Same as `LOAD name$ SAMPLE n` in the [ZX-V BASIC][b-sample].

**Differences:** its name, `SAMPLELOAD f$, n`.

**Example:** see [SAMPLE](#sample).

**Since:** v1.19.0-zxv.6.

**See also:** [SAMPLE](#sample), [MUSICLOAD](#musicload).

#### SAMPLE

```
SAMPLE n [, tone, vol, pan, c]   ' n UByte, tone Float, vol UByte, pan UByte, c Byte
```

Plays sound `n` without waiting, together with the music and the rest: on a
sample channel the music does not use (if none is left, on the one that has
had a sound longest), or on channel `c`. A sound with a loop goes on until
`SAMPLESTOP`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-31, a sound loaded | The sound. |
| `tone` | semitones, decimals too; 0 if left out | Up (12 an octave) or down (negative) from the pitch it was recorded at. |
| `vol` | 0-64; 64 if left out | The volume. |
| `pan` | 0 (left) to 255 (right); 128, the centre, if left out | Where it is heard. |
| `c` | -1 (if left out: a free one) or 0-11 | The channel. On a channel of the music, it covers it while it plays. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | Sound `n` is not loaded. |
| `B Integer out of range` | `n` above 31; `vol` above 64; `c` out of -1 to 11; `tone` too high or too low for the channel. |

**ROM:** the sample channels need ZX-V 0.10.0 or later.

Same as [`SAMPLE`][b-sample] in the ZX-V BASIC.

**Differences:** the channel goes after a comma, not a `;`, so to give it
the other values are needed too: `SAMPLE 0, 0, 64, 128, 5`.

**Example**

```basic
#include <zxv.bas>
SAMPLELOAD "hit", 0
DO
    IF INKEY$ = " " THEN SAMPLE 0, RND * 4 - 2
LOOP
```

**Since:** v1.19.0-zxv.6.

**See also:** [SAMPLELOAD](#sampleload), [SAMPLESTOP](#samplestop).

#### SAMPLESTOP

```
SAMPLESTOP [c]            ' c Byte
```

Stops the sound of channel `c`, or all of them.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `c` | -1 (if left out: all) or 0-11 | The channel. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `c` out of -1 to 11. |

Same as `SAMPLE STOP` in the [ZX-V BASIC][b-sample].

**Differences:** its name, `SAMPLESTOP`.

**Example:** `SAMPLE 0: PAUSE 25: SAMPLESTOP`.

**Since:** v1.19.0-zxv.6.

**See also:** [SAMPLE](#sample).

#### SAMPLEPLAYING

```
SAMPLEPLAYING             ' gives UByte
```

A function: 1 while any sound of `SAMPLE` plays, 0 if not.

**Returns:** 1 or 0.

**Errors:** none.

Same as the function `SAMPLE` of the [ZX-V BASIC][b-sample].

**Differences:** its name, `SAMPLEPLAYING`.

**Example**

```basic
#include <zxv.bas>
SAMPLELOAD "hit", 0: SAMPLE 0
DO
    WAIT
LOOP WHILE SAMPLEPLAYING
PRINT "done"
```

**Since:** v1.19.0-zxv.6.

**See also:** [SAMPLE](#sample).

`zxv.bas` also has `zxvSamplePlay`, which `SAMPLE` uses: it is not for the
programs.

[b-ref]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md
[b-colours]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#colours
[b-mode]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#mode
[b-page]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#page-screen
[b-scroll]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#scroll
[b-remap]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#remap
[b-palette]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#palette
[b-wait]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#wait
[b-grab]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#grab
[b-sprite]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#sprite
[b-hit]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#hit
[b-box]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#box
[b-disc]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#disc
[b-poly]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#poly
[b-pattern]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#pattern
[b-clip]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#clip
[b-pixel]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#pixel
[b-tile]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#tile
[b-mapdim]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#map-dim-map-poke
[b-map]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#map
[b-cell]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#cell
[b-texture]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#texture
[b-floor]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#floor
[b-rotate]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#rotate
[b-key]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#key
[b-joy]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#joy
[b-sound]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#sound
[b-time]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#time
[b-music]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#music
[b-sfx]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#sfx
[b-sample]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/basic/reference.md#sample

### The libraries of the ZX-V SDK

**Since:** v1.19.0-zxv.7, all of them.

The optional libraries of the ZX-V SDK (`sdk/lib` of ZX-V), each one in a
file of its own: `#include <zxvinput.bas>` and so on (`zxvmapmove.bas`,
`zxvm7.bas` and `zxv3d.bas` include `zxv.bas` themselves). They call the same
C as the programs in C (copied in `src/lib/arch/zxv/sdk/lib` by
`tools/zxv_sdk_sync.py`), and only what a program uses goes into it: a
program without them does not change by a byte. Their state (the controls of
the players, the fonts, the table of scores) stays in the library, and the
BASIC names it by number. They are not in the ZX-V BASIC; each section links
to the C library it uses, in the [SDK documentation][l-ref].

They need the ROM that `zxv.bas` needs, and a ROM of specification 0.2 (ZX-V
0.11.0 or later) only for the shading of `zxv3d.bas` (with an older
one, the faces between the dark and the light tone come out plain) and for
`PATTERN` in the text of `zxvfont.bas`.

| File | |
|---|---|
| [`zxvinput.bas`](#zxvinputbas) | The keyboard and the joysticks at once, as actions, for up to four players, with keys the player can change |
| [`zxvsave.bas`](#zxvsavebas) | Saving the data of a game without losing it in a power cut, and a table of scores |
| [`zxvfade.bas`](#zxvfadebas) | Palette fades |
| [`zxvfont.bas`](#zxvfontbas) | Text of any size and colour, proportional |
| [`zxvinflate.bas`](#zxvinflatebas) | Data compressed with gzip |
| [`zxvmapmove.bas`](#zxvmapmovebas) | Moving things through a tile map without going through the walls |
| [`zxvm7.bas`](#zxvm7bas) | Things on the mode 7 floor |
| [`zxv3d.bas`](#zxv3dbas) | Simple 3D: models of flat faces |
| [`zxvmath.bas`](#zxvmathbas) | Sine, cosine, atan2 and roots with whole numbers, and random numbers with a seed |

`tests/zxv/sdkinput.bas`, `sdksave.bas`, `sdkfade.bas`, `sdkfont.bas`,
`sdkinflate.bas`, `sdkmath.bas`, `mapmove.bas`, `floorproj.bas` and `sdk3d.bas`
test them.

#### zxvinput.bas

The keyboard and the joysticks at once, as **actions** (up to 16, a bit
each), for up to four players, `p` 0 to 3: player 0 with the cursors or
Q A O P, Space or M to fire and joystick 0; players 1 to 3, their joystick
(1 to 3) and the keys you give them. Once a frame, `CTRLUPDATE`; then what is held, what was pressed or
released in this frame, how long. C: [`zxv_input.h`][l-input].

```basic
#include <zxv.bas>
#include <zxvinput.bas>
DIM x AS Integer = 128
DO
    CTRLUPDATE
    IF CTRLHELD() BAND CTRLLEFT THEN LET x = x - 1
    IF CTRLHELD() BAND CTRLRIGHT THEN LET x = x + 1
    IF CTRLPRESSED() BAND CTRLFIRE THEN BEEP 0.05, 20
    PLOT x, 88: WAIT
LOOP
```

**Constants.** The actions of the map of always, the first nine the same
bits as the buttons of `JOY`: `CTRLRIGHT` 1, `CTRLLEFT` 2, `CTRLDOWN` 4,
`CTRLUP` 8, `CTRLFIRE` 16, `CTRLFIRE2` 32, `CTRLFIRE3` 64, `CTRLSTART` 128,
`CTRLSELECT` 256, `CTRLESC` 512, and, only on the joystick, `CTRLFIRE4`
1024, `CTRLL` 2048 and `CTRLR` 4096 (a game can use the other bits, 8192
to 32768, for actions of its own). The keys, by their USB code (their place on
a US keyboard): `KEYA` to `KEYZ` (4-29), `KEY1` to `KEY9` and `KEY0`
(30-39), `KEYENTER` 40, `KEYESCAPE` 41, `KEYBACKSPACE` 42, `KEYTAB` 43,
`KEYSPACE` 44, `KEYRIGHTKEY` 79, `KEYLEFTKEY` 80, `KEYDOWNKEY` 81,
`KEYUPKEY` 82, `KEYLCTRL` 224, `KEYLSHIFT` 225, `KEYLALT` 226, `KEYRCTRL`
228, `KEYRSHIFT` 229. (They are not the codes of [KEY](#key): `KEY(256 +
KEYA)` is the same as `KEY(CODE "a")`.)

**Errors common to the library**

| Error | When |
|---|---|
| `B Integer out of range` | The player `p` is not 0 to 3. |

`CTRLUPDATE` empties the queue of keys, so what is typed in the game does
not reach `INKEY$`, `INPUT` or the BASIC when the program ends;
[CTRLKEEPKEYS](#ctrlkeepkeys) keeps it.

##### CTRLRESET

```
CTRLRESET [p]             ' p UByte
```

The keys and the joystick of player `p` as at the start, and nothing held.
Player 0: the cursors or Q A O P, fire with Space or M (and the button 1 of
joystick 0), fire 2 with N or Z, fire 3 with B or X, Enter START, Tab
SELECT, Esc `CTRLESC`, and fire 4, L and R only on the joystick. Players 1
to 3: no keys, the same buttons on their joystick (1 to 3).
Each player starts like this the first time it is used.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 (if left out) to 3 | The player. |

**Errors:** (common).

**Example:** `CTRLRESET 1`.

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLKEY](#ctrlkey), [CTRLPAD](#ctrlpad).

##### CTRLKEY

```
CTRLKEY p, action, slot, k   ' p UByte, action UInteger, slot UByte, k UByte
```

Key `k` does `action` for player `p`, in one of its three places (`slot`):
an action can have up to three keys.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 to 3 | The player. |
| `action` | one bit: `CTRLLEFT`... or 1, 2, 4... 32768 | The action. |
| `slot` | 0-2 | Which of its three keys. |
| `k` | a key code (`KEYA`...), or 0 for none | The key. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | `action` is not a single bit. |
| `B Integer out of range` | `slot` above 2. |
| (common) | |

**Example**

```basic
#include <zxv.bas>
#include <zxvinput.bas>
CTRLKEY 1, CTRLLEFT, 0, KEYZ: CTRLKEY 1, CTRLRIGHT, 0, KEYX
CTRLKEY 1, CTRLFIRE, 0, KEYLSHIFT
```

**Since:** v1.19.0-zxv.7.

**See also:** [NEWKEY](#newkey), [CTRLBUTTON](#ctrlbutton).

##### CTRLBUTTON

```
CTRLBUTTON p, action, buttons   ' p UByte, action UInteger, buttons UInteger
```

The joystick buttons (the bits of [JOY](#joy)) that do `action` for player
`p`; 0, none.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 to 3 | The player. |
| `action` | one bit | The action. |
| `buttons` | 0-511 | The buttons, added. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | `action` is not a single bit. |
| (common) | |

**Example:** `CTRLBUTTON 0, CTRLFIRE, 16 + 32` (fire with buttons 1 and 2).

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLPAD](#ctrlpad).

##### CTRLPAD

```
CTRLPAD p, pad            ' p UByte, pad Byte
```

The joystick of player `p`: 0 to 3, or -1 (none).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 to 3 | The player. |
| `pad` | -1 to 3 | The joystick (-1, none). |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `pad` is not -1 to 3. |
| (common) | |

**Example:** `CTRLPAD 0, -1` (player 0, only the keyboard).

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLBUTTON](#ctrlbutton).

##### CTRLKEEPKEYS

```
CTRLKEEPKEYS p, onoff     ' p UByte, onoff UByte
```

With 1, `CTRLUPDATE` of player `p` does not empty the queue of keys:
`INKEY$` and `INPUT` still get them. With 0 (as at the start), it does.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 to 3 | The player. |
| `onoff` | 0, or anything else for yes | Keep the keys. |

**Errors:** (common).

**Example:** `CTRLKEEPKEYS 0, 1`.

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLUPDATE](#ctrlupdate).

##### CTRLUPDATE

```
CTRLUPDATE [p]            ' p UByte
```

Reads the keyboard and the joystick of player `p`: call it once a frame,
before asking what is held.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 (if left out) to 3 | The player. |

**Errors:** (common).

**Example:** see [zxvinput.bas](#zxvinputbas).

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLHELD](#ctrlheld).

##### CTRLHELD

```
CTRLHELD([p])             ' p UByte; gives UInteger
```

The actions held now, added (`CTRLLEFT + CTRLFIRE`...). Test one with
`BAND`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 (if left out) to 3 | The player. |

**Returns:** the bits of the actions, 0-65535.

**Errors:** (common).

**Example:** `IF CTRLHELD() BAND CTRLUP THEN LET y = y - 1`.

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLPRESSED](#ctrlpressed), [CTRLFRAMES](#ctrlframes).

##### CTRLPRESSED

```
CTRLPRESSED([p])          ' p UByte; gives UInteger
```

The actions that were pressed in this frame (held now and not in the frame
before): to fire once a press.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 (if left out) to 3 | The player. |

**Returns:** the bits of the actions.

**Errors:** (common).

**Example:** `IF CTRLPRESSED() BAND CTRLFIRE THEN shoot()`.

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLRELEASED](#ctrlreleased), [CTRLREPEAT](#ctrlrepeat).

##### CTRLRELEASED

```
CTRLRELEASED([p])         ' p UByte; gives UInteger
```

The actions that were released in this frame.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 (if left out) to 3 | The player. |

**Returns:** the bits of the actions.

**Errors:** (common).

**Example:** `IF CTRLRELEASED() BAND CTRLFIRE THEN LET charging = 0`.

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLPRESSED](#ctrlpressed).

##### CTRLFRAMES

```
CTRLFRAMES(p, action)     ' p UByte, action UInteger; gives UInteger
```

How many frames `action` has been held; 0 if it is not.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 to 3 | The player. |
| `action` | one bit | The action. |

**Returns:** the frames, 0-65535.

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | `action` is not a single bit. |
| (common) | |

**Example:** `IF CTRLFRAMES(0, CTRLFIRE) > 30 THEN superShot()`.

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLHELD](#ctrlheld).

##### CTRLREPEAT

```
CTRLREPEAT(p, actions [, delay, rate])   ' p UByte, the rest UInteger; gives UByte
```

1 when one of `actions` has just been pressed and, while it is held, every
`rate` frames from frame `delay` on: for moving through a menu.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 to 3 | The player. |
| `actions` | the bits of one or more actions | The actions. |
| `delay` | frames; 10 if left out | When it starts repeating. |
| `rate` | frames; 3 if left out | How often it repeats. |

**Returns:** 1 or 0.

**Errors:** (common).

**Example:** `IF CTRLREPEAT(0, CTRLDOWN) THEN LET option = option + 1`.

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLPRESSED](#ctrlpressed).

##### CTRLMAP

```
CTRLMAP([p])              ' p UByte; gives ULong
```

The address of the keys and buttons of player `p` (its map, `CTRLMAPSIZE`
bytes), to save them with [SAVEDATA](#savedata) and get them back with
[LOADDATA](#loaddata).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `p` | 0 (if left out) to 3 | The player. |

**Returns:** the address.

**Errors:** (common).

**Example**

```basic
#include <zxv.bas>
#include <zxvinput.bas>
#include <zxvsave.bas>
IF LOADDATA("keys.cfg", 1, CTRLMAP(), CTRLMAPSIZE()) <> 0 THEN CTRLRESET
```

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLMAPSIZE](#ctrlmapsize).

##### CTRLMAPSIZE

```
CTRLMAPSIZE()             ' gives UInteger
```

The bytes of the map of a player: 82.

**Returns:** 82.

**Errors:** none.

**Example:** see [CTRLMAP](#ctrlmap).

**Since:** v1.19.0-zxv.7.

**See also:** [CTRLMAP](#ctrlmap).

##### NEWKEY

```
NEWKEY()                  ' gives Integer
```

The key pressed since the call before (its code, `KEYA`...; the lowest if
there are several), or -1. The first call only takes note of what is held
and gives -1. For letting the player change the keys.

**Returns:** a key code, 1-255, or -1.

**Errors:** none.

**Example**

```basic
#include <zxv.bas>
#include <zxvinput.bas>
DIM k AS Integer = NEWKEY()
PRINT "Key for fire?"
DO
    WAIT: LET k = NEWKEY()
LOOP UNTIL k >= 0
CTRLKEY 0, CTRLFIRE, 0, k: PRINT KEYNAME(k)
```

**Since:** v1.19.0-zxv.7.

**See also:** [KEYNAME](#keyname), [CTRLKEY](#ctrlkey).

##### KEYNAME

```
KEYNAME(k)                ' k UByte; gives String
```

The name of key `k` to show it, in the language of the machine: "Q",
"Space", "Up", "F12"... The keys of symbols change place from one keyboard
to another, so they come out as "Key" and their code in hex ("Key 33").

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `k` | 0-255 | The key code. |

**Returns:** the name.

**Errors:** none.

**Example:** `PRINT KEYNAME(KEYSPACE)` shows `Space`.

**Since:** v1.19.0-zxv.7.

**See also:** [NEWKEY](#newkey).

#### zxvsave.bas

Saving the data of a game (scores, options, a game in progress) so that a
power cut or a card taken out does not leave it damaged: the data go with
the version of your data, their size and a CRC to a temporary file, `f$~`,
which then takes the place of `f$`. A file of another version or size, or
damaged, is not read, and the memory does not change. The functions give an
error code and **never stop the program**: without a card, from `ROM:` or
with `EXT:` offline, the game goes on, only without saving. C:
[`zxv_save.h`][l-save].

```basic
#include <zxv.bas>
#include <zxvsave.bas>
DIM opt(3) AS UByte
IF LOADDATA("game.sav", 1, @opt(0), 4) <> 0 THEN LET opt(0) = 1
' ...
IF SAVEDATA("game.sav", 1, @opt(0), 4) = SAVENOCARD THEN PRINT "No card"
```

**Constants.** The errors (negative): `SAVENOFILE` -1 (it is not there),
`SAVENOCARD` -12, `SAVEDAMAGED` -64 (not a file of `SAVEDATA`, or
damaged), `SAVEVERSION` -65 (of another version or size). The others are
those of the ZX-V files, negative too: -3 a name that is not valid, -9 the
drive does not let it (`ROM:`), -11 the card failed, -13 the card is full,
-18 no server for `EXT:` (all in the [SDK reference][l-errors]).

##### SAVEDATA

```
SAVEDATA(f$, version, addr, size)   ' version UInteger, addr ULong, size ULong; gives Long
```

Saves `size` bytes from `addr` (`@a(0)` of an array, `@v` of a variable) in
file `f$`, with the `version` of your data. Change the version when what you
save changes: the old files then give `SAVEVERSION` instead of being read
wrong.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `f$` | a path; the name of the file up to 62 characters (`f$~` is used while writing) | The file. |
| `version` | 0-65535 | The version of your data. |
| `addr` | an address | What to save. |
| `size` | bytes | How much. |

**Returns:** 0, or an error (negative): `SAVENOCARD`, -3, -9, -11, -13,
-18...

**Errors:** none (they are returned).

**Example:** see [zxvsave.bas](#zxvsavebas).

**Since:** v1.19.0-zxv.7.

**See also:** [LOADDATA](#loaddata).

##### LOADDATA

```
LOADDATA(f$, version, addr, size)   ' version UInteger, addr ULong, size ULong; gives Long
```

Reads at `addr` what `SAVEDATA` saved with that `version` and `size`. A
file that is not there, of another version or size, or damaged leaves the
memory as it was. Up to 256 bytes, so does any error. With more there is no
room for a copy: it checks the file first and then reads it at `addr`
(checking it again); if the card fails just then (taken out at that moment),
the error comes back with the memory half read. So, with any error, put the
values of always, as in the example.

**Parameters:** as [SAVEDATA](#savedata).

**Returns:** 0, or an error (negative): `SAVENOFILE`, `SAVENOCARD`,
`SAVEDAMAGED`, `SAVEVERSION`...

**Errors:** none (they are returned).

**Example:** see [zxvsave.bas](#zxvsavebas).

**Since:** v1.19.0-zxv.7.

**See also:** [SAVEDATA](#savedata).

##### CRC32

```
CRC32(addr, size)         ' addr ULong, size ULong; gives ULong
```

The CRC-32 (the one of zip and PNG) of `size` bytes from `addr`.

**Returns:** the CRC.

**Errors:** none.

**Example:** `PRINT CRC32(@opt(0), 4)`.

**Since:** v1.19.0-zxv.7.

##### SCORECLEAR

```
SCORECLEAR [n]            ' n UByte
```

An empty table of `n` scores (all 0, without a name). The table starts with
5.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 1-10; 5 if left out | How many places. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `n` is 0 or above 10. |

**Example:** `SCORECLEAR 10`.

**Since:** v1.19.0-zxv.7.

**See also:** [SCOREADD](#scoreadd).

##### SCOREADD

```
SCOREADD(score, name$)    ' score ULong; gives Byte
```

Puts the score in its place of the table (from higher to lower; with a
draw, after the ones there) and drops the last one.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `score` | 0-4294967295 | The score. |
| `name$` | up to 7 characters (the rest are left out) | The name. |

**Returns:** its place (0 the best), or -1 if it does not get in.

**Errors:** none.

**Example**

```basic
#include <zxv.bas>
#include <zxvsave.bas>
IF SCORELOAD("scores.sav") <> 0 THEN SCORECLEAR
IF SCOREADD(1250, "SCH") >= 0 THEN LET r = SCORESAVE("scores.sav")
FOR i = 0 TO 4: PRINT SCORENAME(i); " "; SCORE(i): NEXT i
```

**Since:** v1.19.0-zxv.7.

**See also:** [SCORE](#score), [SCORESAVE](#scoresave).

##### SCORE

```
SCORE(i)                  ' i UByte; gives ULong
```

The score of place `i` of the table.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `i` | 0 (the best) to the places - 1 | The place. |

**Returns:** the score.

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `i` is not a place of the table. |

**Example:** see [SCOREADD](#scoreadd).

**Since:** v1.19.0-zxv.7.

##### SCORENAME

```
SCORENAME(i)              ' i UByte; gives String
```

The name of place `i` of the table.

**Parameters:** as [SCORE](#score).

**Returns:** the name, up to 7 characters.

**Errors:** as [SCORE](#score).

**Example:** see [SCOREADD](#scoreadd).

**Since:** v1.19.0-zxv.7.

##### SCORESAVE

```
SCORESAVE(f$ [, version])   ' version UInteger; gives Long
```

Saves the table (its places too) as [SAVEDATA](#savedata) does.

**Parameters:** `f$` as in SAVEDATA; `version`, 1 if left out.

**Returns:** as SAVEDATA.

**Errors:** none (they are returned).

**Example:** see [SCOREADD](#scoreadd).

**Since:** v1.19.0-zxv.7.

##### SCORELOAD

```
SCORELOAD(f$ [, version])   ' version UInteger; gives Long
```

Loads the table as [LOADDATA](#loaddata) does; if it fails, the table does
not change. What it reads must also be a table: 1 to 10 places and names of
up to 7 characters; a file of the right size with anything else (made with
`SAVEDATA`, or by hand) gives `SAVEDAMAGED`, as a damaged one.

**Parameters:** `f$`; `version`, 1 if left out.

**Returns:** as LOADDATA (and `SAVEDAMAGED` if it is no table).

**Errors:** none (they are returned).

**Example:** see [SCOREADD](#scoreadd).

**Since:** v1.19.0-zxv.7.

#### zxvfade.bas

Palette fades: the colours go to black, to white or to others in some
frames, without touching the pixels. A step each `FADESTEP` while the game
goes on, or all at once with `FADEWAIT`. It changes the palettes, as
[PALETTE](#palette) does. C: [`zxv_fade.h`][l-fade].

```basic
#include <zxv.bas>
#include <zxvfade.bas>
FADECOLOUR RGB565(0, 0, 0), 30: FADEWAIT    ' to black in half a second
' ... the next level
FADETO 0, 30: FADEWAIT                      ' to the colours of always
```

**Constants:** `FADEALL` 15, the four palette tables (bit `t`, table `t`).

None of these give errors.

##### FADETO

```
FADETO addr, frames [, tables]   ' addr ULong, frames UInteger, tables UByte
```

Starts a fade from the colours there are to 16 colours (RGB565) in that many
frames; if there was another fade, from where it was.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `addr` | `@pal(0)` of a `DIM pal(15) AS UInteger`, or 0 | The 16 colours; 0, the standard ones. |
| `frames` | 0-65535 | How long; 0, at once. |
| `tables` | 1-15; 15 (`FADEALL`) if left out | Which palette tables (bit `t`, table `t`). |

**Example:** see [zxvfade.bas](#zxvfadebas).

**Since:** v1.19.0-zxv.7.

**See also:** [FADESTEP](#fadestep), [RGB565](#rgb565).

##### FADECOLOUR

```
FADECOLOUR rgb, frames [, tables]   ' rgb UInteger, frames UInteger, tables UByte
```

The same as [FADETO](#fadeto), with the 16 colours the same: `RGB565(0, 0,
0)` is to black, `RGB565(255, 255, 255)` to white.

**Parameters:** `rgb`, a colour of [RGB565](#rgb565); the rest as FADETO.

**Example:** see [zxvfade.bas](#zxvfadebas).

**Since:** v1.19.0-zxv.7.

##### FADESTEP

```
FADESTEP
```

One frame more of the fade (nothing if there is none): call it once a frame
while the game goes on.

**Example**

```basic
#include <zxv.bas>
#include <zxvfade.bas>
FADECOLOUR RGB565(255, 255, 255), 20
DO
    FADESTEP: WAIT
LOOP WHILE FADEBUSY()
```

**Since:** v1.19.0-zxv.7.

**See also:** [FADEBUSY](#fadebusy).

##### FADEBUSY

```
FADEBUSY()                ' gives UByte
```

1 while there is fade left, 0 if not.

**Returns:** 1 or 0.

**Example:** see [FADESTEP](#fadestep).

**Since:** v1.19.0-zxv.7.

##### FADEWAIT

```
FADEWAIT
```

Does the whole fade, a step each frame, and then goes on.

**Example:** see [zxvfade.bas](#zxvfadebas).

**Since:** v1.19.0-zxv.7.

##### RGBMIX

```
RGBMIX(a, b, k, n)        ' all UInteger; gives UInteger
```

The colour `k`/`n` of the way from `a` to `b` (RGB565), the nearest one.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `a`, `b` | RGB565 | The two colours. |
| `k`, `n` | 0-65535 | How far: 0 gives `a`, `n` (or more, or `n` 0) gives `b`. |

**Returns:** the colour, RGB565.

**Example:** `LET c = RGBMIX(RGB565(255, 0, 0), RGB565(0, 0, 255), 1, 2)`.

**Since:** v1.19.0-zxv.7.

##### RGB565

```
RGB565(r, g, b)           ' r, g, b UByte; gives UInteger
```

A colour of red, green and blue from 0 to 255, as [PALETTE](#palette)
takes them, in RGB565 (the colours of ZX-V).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `r`, `g`, `b` | 0-255 | Red, green and blue. |

**Returns:** the colour, 0-65535.

**Example:** see [zxvfade.bas](#zxvfadebas).

**Since:** v1.19.0-zxv.7.

#### zxvfont.bas

Text of any size (1-8 times) and colour, proportional or of fixed width,
with line breaks (`CHR$ 10`): four fonts (0-3), the 8 × 8 one of the ROM or
one drawn on the page in cells of any size, in its own colours. It draws on
the drawing page with the `CLIP` and the `PATTERN`. C: [`zxv_font.h`][l-font].

```basic
#include <zxv.bas>
#include <zxvfont.bas>
FONTROM 0
DIM w AS Integer = FONTWIDTH(0, "GAME OVER", 3)
DIM x AS Integer = FONTDRAW(0, (256 - w) / 2, 80, "GAME OVER", 14, 3)
```

**Constants:** `FONTOWN` -1, the colour of `FONTDRAW` that keeps the colours
of the font.

**Errors common to the library**

| Error | When |
|---|---|
| `B Integer out of range` | The font `n` is not 0-3. |
| `A Invalid argument` | Font `n` is not defined (by `FONTROM` or `FONTGRAB`). |

##### FONTROM

```
FONTROM n [, fixedw]      ' n UByte, fixedw UByte
```

Font `n` is the 8 × 8 one of the ROM: proportional (a pixel between
letters) or, with `fixedw` = 1, every letter 8 pixels wide, as the machine
writes.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-3 | The font. |
| `fixedw` | 0 (if left out) or 1 | Fixed width. |

**Errors:** `B Integer out of range`, as in the common ones.

**Example:** see [zxvfont.bas](#zxvfontbas).

**Since:** v1.19.0-zxv.7.

##### FONTGRAB

```
FONTGRAB n, x, y, w, h, cw, ch [, first, t, fixedw]
' n UByte, x, y Integer, w, h UInteger, cw, ch UByte, first UInteger, t Integer, fixedw UByte
```

Font `n` is that rectangle of the drawing page, cut in cells of `cw` × `ch`
with the characters from `first` on, left to right and top to bottom (up to
224), in their colours. Colour `t` becomes transparent (and a colour 8 of
the page, black). The width of each letter is what it has drawn, unless
`fixedw` is 1.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-3 | The font. |
| `x`, `y` | -32768 to 32767 | The top left corner. |
| `w`, `h` | 1-1024 | The size of the rectangle. |
| `cw`, `ch` | 1-255, not more than `w` and `h` | A cell. |
| `first` | 0-65535; 32 (the space) if left out | The first character. |
| `t` | -1 (if left out: none) or 0-15 | The transparent colour. |
| `fixedw` | 0 (if left out) or 1 | Fixed width. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `n` above 3; `w` or `h` 0 or above 1024; `cw` or `ch` 0. |
| `K Invalid colour` | `t` below -1 or above 15. |
| `A Invalid argument` | A cell bigger than the rectangle, or a sheet that does not work as a font. |
| (common) | See [Errors common to every command](#zxvbas): the font is copied to the heap. |

**Example**

```basic
#include <zxv.bas>
#include <zxvfont.bas>
PRINT AT 0, 0; "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
FONTGRAB 1, 0, 0, 208, 8, 8, 8, 65, 0
CLS: DIM x AS Integer = FONTDRAW(1, 10, 10, "HELLO", FONTOWN, 2)
```

**Since:** v1.19.0-zxv.7.

##### FONTSPACING

```
FONTSPACING n, s          ' n UByte, s UByte
```

The pixels between letters of font `n` (1 in the proportional ones, at the
start).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-3, a font defined | The font. |
| `s` | 0-255 | The pixels. |

**Errors:** (common).

**Example:** `FONTSPACING 0, 2`.

**Since:** v1.19.0-zxv.7.

##### FONTDRAW

```
FONTDRAW(n, x, y, t$, colour [, scale])
' n UByte, x, y Integer, colour Integer, scale UByte; gives Integer
```

Draws `t$` with its top left corner at (`x`, `y`), of that colour and
`scale` times bigger. `CHR$ 10` goes to the next line, back to `x`. A
character that is not in the font comes out as `?`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-3, a font defined | The font. |
| `x`, `y` | -32768 to 32767 | Where. |
| `t$` | any string | The text. |
| `colour` | 0-15, or `FONTOWN` (-1): the colours of the font | The colour. |
| `scale` | 1-8; 1 if left out | How much bigger. |

**Returns:** the `x` where the last line ends.

**Errors**

| Error | When |
|---|---|
| `K Invalid colour` | `colour` below -1 or above 15. |
| `B Integer out of range` | `scale` 0 or above 8. |
| (common) | |

**Example:** see [zxvfont.bas](#zxvfontbas).

**Since:** v1.19.0-zxv.7.

**See also:** [FONTWIDTH](#fontwidth).

##### FONTWIDTH

```
FONTWIDTH(n, t$ [, scale])   ' n UByte, scale UByte; gives Integer
```

The width of the longest line of `t$`, in pixels: to centre it.

**Parameters:** as [FONTDRAW](#fontdraw).

**Returns:** the width.

**Errors:** `B Integer out of range` (`scale`), and the common ones.

**Example:** see [zxvfont.bas](#zxvfontbas).

**Since:** v1.19.0-zxv.7.

##### FONTHEIGHT

```
FONTHEIGHT(n, t$ [, scale])  ' n UByte, scale UByte; gives Integer
```

The height of all the lines of `t$`, in pixels.

**Parameters:** as [FONTDRAW](#fontdraw).

**Returns:** the height.

**Errors:** as [FONTWIDTH](#fontwidth).

**Example:** `PRINT FONTHEIGHT(0, "ONE" + CHR$ 10 + "TWO", 2)`.

**Since:** v1.19.0-zxv.7.

#### zxvinflate.bas

Data compressed on the PC with gzip (`gzip -9`) or deflate, uncompressed in
the memory: load them with `LOAD ... CODE`, carry them inside the program
(`--embed`), or put them in an array. C: [`zxv_inflate.h`][l-inflate].

```basic
#include <zxv.bas>
#include <zxvinflate.bas>
DIM gz(4095) AS UByte
DIM level(16383) AS UByte
LOAD "level1.gz" CODE @gz(0)
DIM n AS Long = GUNZIP(@gz(0), 4096, @level(0), 16384)
IF n < 0 THEN PRINT "Damaged"
```

**Constants:** the errors, `INFLATEDATA` -66 (it is not deflate, or it is
damaged or cut) and `INFLATESPACE` -67 (it does not fit in `max`).

None of these stop the program: the errors are returned.

##### GUNZIP

```
GUNZIP(src, n, dst, max)  ' all ULong; gives Long
```

The `.gz` (one member) of `n` bytes at `src`, uncompressed at `dst`, checked
with its CRC and its length.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `src`, `n` | an address and bytes | The `.gz`. |
| `dst`, `max` | an address and bytes | Where it goes, and the room there. |

**Returns:** the bytes uncompressed, or `INFLATEDATA` or `INFLATESPACE`.

**Example:** see [zxvinflate.bas](#zxvinflatebas).

**Since:** v1.19.0-zxv.7.

##### GUNZIPSIZE

```
GUNZIPSIZE(src, n)        ' all ULong; gives Long
```

How much the `.gz` takes uncompressed (it says so at its end), without
uncompressing it: to make room for it.

**Returns:** the bytes, or `INFLATEDATA` if it does not look like a `.gz`.

**Example:** `PRINT GUNZIPSIZE(@gz(0), 4096)`.

**Since:** v1.19.0-zxv.7.

##### INFLATE

```
INFLATE(src, n, dst, max)    ' all ULong; gives Long
```

The same as [GUNZIP](#gunzip) with deflate alone, without the `.gz` around
it (and without its CRC), as Python's `zlib.compressobj(9, zlib.DEFLATED,
-15)` makes it.

**Returns:** as GUNZIP.

**Example:** `LET n = INFLATE(@raw(0), 1000, @level(0), 16384)`.

**Since:** v1.19.0-zxv.7.

#### zxvmapmove.bas

Moving things through a tile map (of [MAPDIM](#mapdim)) without going
through the walls, however fast: the core of a platform game. Each tile is
empty, solid, a platform (it only stops what comes down from above) or of a
type of the game (ladders, coins...) that `MAPFIND` finds. The positions
are `Fixed`, in pixels of the map. It includes `zxv.bas`. C:
[`zxv_mapmove.h`][l-mapmove].

```basic
#include <zxvmapmove.bas>
DIM px AS Fixed = 16
DIM py AS Fixed = 16
DIM vx AS Fixed = 1
DIM vy AS Fixed
MAPDIM 0, 32, 24: FOR c = 0 TO 31: MAPPOKE 0, c, 20, 1: NEXT c
TILEKIND 8, 1, TILESOLID
DO
    LET vy = vy + 0.25
    IF MAPMOVE(0, px, py, 12, 16, vx, vy) BAND HITDOWN THEN LET vy = 0
    WAIT
LOOP
```

**Constants.** The types of tile: `TILEEMPTY` 0, `TILESOLID` 1,
`TILEPLATFORM` 2 (3-255 are the game's: they stop nothing). What `MAPMOVE`
gives: `HITLEFT` 1, `HITRIGHT` 2, `HITUP` 4, `HITDOWN` 8. The option
`MOVETHROUGH` 1: through the platforms (to go down from one).

Until the first `TILEKIND` of a set, tile 0 is empty and the rest are
solid. The tiles of the map need not be defined (it does not draw).

**Errors common to the library** (those that take a map)

| Error | When |
|---|---|
| `B Integer out of range` | `m` above 15. |
| `A Invalid argument` | Map `m` is not defined. |

##### TILEKIND

```
TILEKIND s, tile, kind    ' s UByte, tile UInteger, kind UByte
```

The type of a tile of the set of side `s`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `s` | 8 or 16 | The set. |
| `tile` | 0-4095 | The tile. |
| `kind` | `TILEEMPTY`, `TILESOLID` (it stops on its four sides), `TILEPLATFORM`, or 3-255 | Its type. |

**Errors**

| Error | When |
|---|---|
| `A Invalid argument` | `s` is not 8 or 16. |
| `B Integer out of range` | `tile` above 4095. |

**Example:** `TILEKIND 8, 1, TILESOLID: TILEKIND 8, 2, TILEPLATFORM`; and
`TILEKIND 8, 3, 3`, a type of the game (coins).

**Since:** v1.19.0-zxv.7.

##### MAPOUTSIDE

```
MAPOUTSIDE m, kind        ' m UByte, kind UByte
```

The type of what is out of map `m` when it does not repeat: `TILESOLID` at
the start, so the edges are walls.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `m` | 0-15 (the map need not exist yet) | The map. |
| `kind` | 0-255 | The type. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `m` above 15. |

**Example:** `MAPOUTSIDE 0, TILEEMPTY` (things can leave the map).

**Since:** v1.19.0-zxv.7.

##### MAPMOVE

```
MAPMOVE(m, x, y, w, h, dx, dy [, flags])
' m UByte, x, y Fixed (ByRef), w, h UInteger, dx, dy Fixed, flags UByte; gives UByte
```

Moves the rectangle of `w` × `h` pixels with its top left corner at (`x`,
`y`) `dx` and then `dy` through map `m`, stopping each axis at the first
cell that blocks it, touching it: it does not jump walls however fast it
goes. `x` and `y` must be `Fixed` **variables**: `MAPMOVE` changes them. A
rectangle already inside something solid goes out the way it moves.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `m` | 0-15, a map defined | The map. |
| `x`, `y` | `Fixed` variables | The position, which changes. |
| `w`, `h` | 1-32767 | The size. |
| `dx`, `dy` | `Fixed` | How much to move. |
| `flags` | 0 (if left out) or `MOVETHROUGH` | Through the platforms. |

**Returns:** the sides that hit, added: `HITLEFT`, `HITRIGHT`, `HITUP`,
`HITDOWN` (on the ground).

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `w` or `h` is 0 or above 32767. |
| (common) | |

**Example:** see [zxvmapmove.bas](#zxvmapmovebas).

**Since:** v1.19.0-zxv.7.

**See also:** [MAPONGROUND](#maponground).

##### MAPONGROUND

```
MAPONGROUND(m, x, y, w, h [, flags])   ' x, y Fixed; gives UByte
```

1 if the rectangle stands on something: just under its feet there is
something that stops it coming down (solid, or a platform without
`MOVETHROUGH`).

**Parameters:** as [MAPMOVE](#mapmove), without `dx` and `dy`; `x` and `y`
can be any `Fixed` value.

**Returns:** 1 or 0.

**Errors:** (common).

**Example:** `IF MAPONGROUND(0, px, py, 12, 16) AND KEY(32) THEN LET vy = -4`.

**Since:** v1.19.0-zxv.7.

##### MAPFIND

```
MAPFIND(m, x, y, w, h, kind)   ' x, y Fixed, kind UByte; gives UByte
```

1 if the rectangle touches a cell of type `kind`; then `MAPFOUNDX` and
`MAPFOUNDY` give the first one, from top to bottom and left to right, in
cells: to take a coin away.

**Parameters:** as [MAPMOVE](#mapmove), with `kind` (0-255) instead of the
movement.

**Returns:** 1 or 0.

**Errors:** (common).

**Example:** `IF MAPFIND(0, px, py, 12, 16, 3) THEN MAPPOKE 0, MAPFOUNDX(),
MAPFOUNDY(), 0` (a coin, type 3, taken away).

**Since:** v1.19.0-zxv.7.

##### MAPFOUNDX, MAPFOUNDY

```
MAPFOUNDX()               ' gives Integer
MAPFOUNDY()               ' gives Integer
```

The column and the row of the cell the last `MAPFIND` found (inside the map,
also when it repeats).

**Returns:** the column or the row.

**Errors:** none.

**Example:** see [MAPFIND](#mapfind).

**Since:** v1.19.0-zxv.7.

##### MAPKIND

```
MAPKIND(m, px, py)        ' m UByte, px, py Integer; gives UByte
```

The type of the cell at pixel (`px`, `py`) of map `m`.

**Returns:** the type, 0-255.

**Errors:** (common).

**Example:** `IF MAPKIND(0, px + 6, py + 16) = 4 THEN PRINT "On a ladder"`.

**Since:** v1.19.0-zxv.7.

#### zxvm7.bas

Things on the mode 7 floor: where a point of the floor of the last
[FLOOR](#floor) or [FLOORMAP](#floormap) is seen and how big, to draw cars,
trees or cones on it with `SPRITE` scaled; and which point of the floor is
at a pixel. It includes `zxv.bas`. C: [`zxv_m7.h`][l-m7].

```basic
#include <zxvm7.bas>
DIM s AS Fixed
GRABSTR 1, 2, "4444": BOX 3, 0, 0, 8, 8: TEXTURE 0, 0, 0, 8
FLOOR 0, 0, 0, 10, 0, 95
IF FLOORPROJECT(100, 0) THEN
    LET s = 32 * FLOORSCALE()                 ' the tree is 32 texels high
    SPRITE 1, FLOORX() - s / 4, FLOORY() - s, 0, "", s / 2, s
END IF
```

**Errors common to the library**

| Error | When |
|---|---|
| `A Invalid argument` | There was no `FLOOR` or `FLOORMAP` before. |

##### FLOORPROJECT

```
FLOORPROJECT(x, y [, h])  ' x, y, h Fixed; gives UByte
```

Where the point (`x`, `y`) of the floor (or at height `h` over it) is seen
with the camera of the last `FLOOR` or `FLOORMAP`: 1 if it is in front of
it (more than a texel away), and then `FLOORX`, `FLOORY`, `FLOORSCALE` and
`FLOORDEPTH` say where. It can fall out of the screen: the sprite is clipped
by itself.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `x`, `y` | `Fixed`, texels | The point of the floor. |
| `h` | `Fixed`; 0 if left out | The height over it. |

**Returns:** 1 or 0.

**Errors:** (common).

**Example:** see [zxvm7.bas](#zxvm7bas).

**Since:** v1.19.0-zxv.7.

##### FLOORX, FLOORY, FLOORSCALE, FLOORDEPTH

```
FLOORX()                  ' gives Integer
FLOORY()                  ' gives Integer
FLOORSCALE()              ' gives Fixed
FLOORDEPTH()              ' gives Fixed
```

What the last `FLOORPROJECT` that gave 1 worked out: the pixel of the page
(`FLOORX`, `FLOORY`), the pixels a texel measures there (`FLOORSCALE`) and
how far it is (`FLOORDEPTH`: draw from the farthest to the nearest).

**Errors:** none.

**Example:** see [zxvm7.bas](#zxvm7bas).

**Since:** v1.19.0-zxv.7.

##### FLOORAT

```
FLOORAT(sx, sy)           ' sx, sy Integer; gives UByte
```

The point of the floor seen at pixel (`sx`, `sy`) of the page: exactly the
texel the last `FLOOR` drew there. 1, and then `FLOORATX` and `FLOORATY`
give it; 0 at the horizon or above it.

**Returns:** 1 or 0.

**Errors:** (common).

**Example:** `IF FLOORAT(128, 150) THEN PRINT FLOORATX(); ","; FLOORATY()`.

**Since:** v1.19.0-zxv.7.

##### FLOORATX, FLOORATY

```
FLOORATX()                ' gives Fixed
FLOORATY()                ' gives Fixed
```

The point of the floor the last `FLOORAT` found, in texels.

**Errors:** none.

**Example:** see [FLOORAT](#floorat).

**Since:** v1.19.0-zxv.7.

#### zxv3d.bas

Simple 3D: models of flat faces with light and shading by patterns, like the
3D games of the 8 and 16 bits. `obj2c.py --bas` of the ZX-V SDK turns an
`.obj` (Blender) into two arrays and a `SUB NAME_model(n)` that calls
`MODEL3D`. Each frame: `BEGIN3D`, an `ADD3D` per object and `DRAW3D`, which
draws them from back to front on the drawing page, with the `CLIP`.
Coordinates are `Fixed` (`x` right, `y` up, `z` forwards); angles in turns of
65536 (16384 is a quarter turn), as `TSIN`. It includes `zxv.bas`. C:
[`zxv_3d.h`][l-3d].

```basic
#include <zxv3d.bas>
#include "house.bas"               ' obj2c.py house.obj house.bas house --bas
DIM a AS Long
DIM n AS Integer
house_model(0)
CAMERA3D -3, 2, -7, 3640, -1820
FOR a = 0 TO 65535 STEP 256
    BOX 5, 0, 0, 256, 80: BOX 4, 0, 80, 256, 112
    BEGIN3D: LET n = ADD3D(0, 0, 1, 0, a): DRAW3D: WAIT
NEXT a
```

**ROM:** the shading uses `PATTERN`: with a ROM of specification 0.1 (ZX-V
0.10.0 or older) the faces between the dark and the light tone are drawn
plain.

**Constants:** the options of a face, for [MAT3D](#mat3d): `TWOSIDED3D` 256
(seen from both sides), `FLAT3D` 512 (always the dark colour, no light),
`DECAL3D` 1024 (stuck on top of another face: a door on a wall).

##### MAT3D

```
MAT3D(dark, light [, flags])   ' dark, light UByte, flags UInteger; gives UInteger
```

The material of a face, for a model made by hand (`obj2c.py` writes them
already done): its dark and its light colour, and its options.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `dark`, `light` | 0-15 | The colours (higher values keep their low 4 bits). |
| `flags` | 0 (if left out), or `TWOSIDED3D`, `FLAT3D`, `DECAL3D` added | Options. |

**Returns:** the material.

**Errors:** none.

**Example:** `LET m = MAT3D(1, 5, TWOSIDED3D)`.

**Since:** v1.19.0-zxv.7.

##### MODEL3D

```
MODEL3D n, verts, nverts, faces, nfaces, scale
' n UByte, verts ULong, nverts UInteger, faces ULong, nfaces UInteger, scale ULong
```

Model `n` from its arrays (`@NAME_v(0)`, `@NAME_f(0)`: `obj2c.py --bas`
writes them, and a `SUB NAME_model(n)` that calls this). They are not
copied: they must stay where they are (global arrays).

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `n` | 0-15 | The model. |
| `verts`, `nverts` | an address; 1-512 vertices | The vertices. |
| `faces`, `nfaces` | an address; 1-65535 faces | The faces. |
| `scale` | above 0, a raw 16.16 number (65536 is 1) | What a unit of the model measures. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `n` above 15; `nverts` 0 or above 512; `nfaces` 0; `scale` 0; `verts` or `faces` 0. |

**Example:** see [zxv3d.bas](#zxv3dbas).

**Since:** v1.19.0-zxv.7.

##### CAMERA3D

```
CAMERA3D x, y, z [, yaw, pitch, roll [, focal [, cx, cy, w, h]]]
' x, y, z Fixed, yaw, pitch, roll Long, focal Integer, cx, cy Integer, w, h UInteger
```

Where the camera is and where it looks, and the view: by default the whole
drawing page, with its centre in the middle.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `x`, `y`, `z` | `Fixed` | The position. |
| `yaw`, `pitch`, `roll` | turns of 65536; 0 if left out | It turns right, looks up, leans right. |
| `focal` | 0-32767 pixels; 0 (if left out): half the view, about 90° across | The focal length. |
| `cx`, `cy` | -1 (if left out: the centre of the view) to 32767 | The centre. |
| `w`, `h` | 0 (if left out: the page) to 4096 | The size of the view. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `focal` below 0; `cx` or `cy` below -1; `w` or `h` above 4096. |

**Example:** see [zxv3d.bas](#zxv3dbas).

**Since:** v1.19.0-zxv.7.

##### LIGHT3D

```
LIGHT3D x, y, z [, ambient]   ' x, y, z Fixed, ambient UByte
```

Where the light comes from (a vector towards it), and the tone of the faces
it does not reach: from 0 (the dark colour) to 4 (the light one). At the
start, from above, in front and to the left, 1.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `x`, `y`, `z` | `Fixed` | Towards the light. |
| `ambient` | 0-4; 1 if left out | The tone without light. |

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `ambient` above 4. |

**Example:** `LIGHT3D 1, 2, -1, 0`.

**Since:** v1.19.0-zxv.7.

##### WIREFRAME3D

```
WIREFRAME3D colour        ' colour Integer
```

Only the edges, in that colour; -1, the faces again.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `colour` | 0-15, or -1 | The colour of the edges. |

**Errors**

| Error | When |
|---|---|
| `K Invalid colour` | `colour` below -1 or above 15. |

**Example:** `WIREFRAME3D 7`.

**Since:** v1.19.0-zxv.7.

##### BEGIN3D

```
BEGIN3D
```

Starts a frame with the camera of `CAMERA3D`: what was added before is
forgotten.

**Errors:** none.

**Example:** see [zxv3d.bas](#zxv3dbas).

**Since:** v1.19.0-zxv.7.

##### ADD3D

```
ADD3D(m, x, y, z [, yaw, pitch, roll [, scale]])
' m UByte, x, y, z Fixed, yaw, pitch, roll Long, scale Fixed; gives Integer
```

Model `m` at (`x`, `y`, `z`), turned and scaled, for the next `DRAW3D`.

**Parameters**

| Parameter | Values | What it is |
|---|---|---|
| `m` | 0-15, a model defined | The model. |
| `x`, `y`, `z` | `Fixed` | Where. |
| `yaw`, `pitch`, `roll` | turns of 65536; 0 if left out | How it is turned. |
| `scale` | `Fixed`, above 0; 1 if left out | How big. |

**Returns:** the faces of it that are seen, or -1 if they do not fit (400
in all a frame).

**Errors**

| Error | When |
|---|---|
| `B Integer out of range` | `m` above 15 or not defined, or `scale` 0 or less. |

**Example:** see [zxv3d.bas](#zxv3dbas).

**Since:** v1.19.0-zxv.7.

##### DRAW3D

```
DRAW3D
```

Draws all that was added since `BEGIN3D`, from back to front, on the
drawing page, with the `CLIP`. It does not use the [PATTERN](#pattern) of
the program (the shading has its own patterns): the faces come out the same
with or without it, and the `PATTERN` is still there for the `BOX`, `DISC`,
`POLY` and `FONTDRAW` that come after.

**Errors:** none.

**Example:** see [zxv3d.bas](#zxv3dbas).

**Since:** v1.19.0-zxv.7.

##### PROJECT3D

```
PROJECT3D(x, y, z)        ' x, y, z Fixed; gives UByte
```

1 if the point of the world is in front of the camera, and then `PROJ3DX`,
`PROJ3DY` and `PROJ3DSCALE` say where it is seen: to put sprites in the 3D
world.

**Returns:** 1 or 0.

**Errors:** none.

**Example:** `IF PROJECT3D(0, 2, 5) THEN SPRITE 0, PROJ3DX(), PROJ3DY()`.

**Since:** v1.19.0-zxv.7.

##### PROJ3DX, PROJ3DY, PROJ3DSCALE

```
PROJ3DX()                 ' gives Integer
PROJ3DY()                 ' gives Integer
PROJ3DSCALE()             ' gives Fixed
```

What the last `PROJECT3D` that gave 1 worked out: the pixel, and the pixels
a unit measures there.

**Errors:** none.

**Example:** see [PROJECT3D](#project3d).

**Since:** v1.19.0-zxv.7.

#### zxvmath.bas

Whole-number maths for games: sine and cosine as `Fixed` with the table of
the graphics engine, the angle of a vector, square roots, distances, and
random numbers with a seed of their own. Angles go in turns of 65536 (16384
is 90 degrees), the ones of the engine; `fmath.bas` has `fSin` and `fCos`
in degrees. C: [`zxv_math.h`][l-math].

None of these give errors.

##### TSIN, TCOS

```
TSIN(a)                   ' a Long; gives Fixed
TCOS(a)                   ' a Long; gives Fixed
```

Sine and cosine of an angle in turns of 65536, the same as the graphics
engine of the ROM.

**Parameters:** `a`, any `Long` (it is taken modulo 65536).

**Returns:** -1 to 1, `Fixed`.

**Example:** `LET vx = speed * TCOS(angle): LET vy = speed * TSIN(angle)`.

**Since:** v1.19.0-zxv.7.

##### TATAN2

```
TATAN2(y, x)              ' x, y Long; gives UInteger
```

The angle of the vector (`x`, `y`): 0 towards the growing `x`, 16384
towards the growing `y` (down on the screen). `TATAN2(0, 0)` is 0.

**Returns:** 0-65535, a few units off at most.

**Example:** `LET a = TATAN2(py - ey, px - ex)` (towards the player).

**Since:** v1.19.0-zxv.7.

##### ISQRT

```
ISQRT(v)                  ' v ULong; gives ULong
```

The square root, rounded down: `ISQRT(10)` is 3.

**Example:** `PRINT ISQRT(1000000)`.

**Since:** v1.19.0-zxv.7.

##### HYPOT

```
HYPOT(x, y)               ' x, y Long; gives ULong
```

The length of (`x`, `y`), rounded down, without overflowing with big
values: a distance.

**Example:** `IF HYPOT(px - ex, py - ey) < 16 THEN PRINT "Caught"`.

**Since:** v1.19.0-zxv.7.

##### RNDSEED

```
RNDSEED s                 ' s ULong
```

The seed of `RNDNEXT` and `RNDRANGE`: the same seed gives the same numbers
(for levels that are always the same). 0 is the same as 1. Without
`RNDSEED`, they start with the same seed each time the program runs.

**Example:** `RNDSEED TIME` (different each time).

**Since:** v1.19.0-zxv.7.

##### RNDNEXT

```
RNDNEXT()                 ' gives ULong
```

A random number of 32 bits (xorshift32).

**Example:** `PRINT RNDNEXT()`.

**Since:** v1.19.0-zxv.7.

##### RNDRANGE

```
RNDRANGE(lo, hi)          ' lo, hi Long; gives Long
```

A random number from `lo` to `hi`, both included (if `hi` is below `lo`,
they are swapped).

**Example:** `LET die = RNDRANGE(1, 6)`.

**Since:** v1.19.0-zxv.7.

[l-ref]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md
[l-errors]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/reference.md
[l-input]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md#keyboard-and-gamepads-at-once-zxv_inputh
[l-save]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md#saving-data-zxv_saveh
[l-fade]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md#palette-fades-zxv_fadeh
[l-font]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md#fonts-zxv_fonth
[l-inflate]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md#decompressing-zxv_inflateh
[l-mapmove]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md#moving-through-a-tile-map-zxv_mapmoveh
[l-m7]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md#objects-on-the-mode-7-floor-zxv_m7h
[l-3d]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md#simple-3d-zxv_3dh
[l-math]: https://github.com/cronomantic/ZX-V/blob/main/docs/en/sdk/libraries.md#maths-zxv_mathh

## Licence

The runtime (`src/lib/arch/zxv/runtime`) and the libraries are MIT, like the rest
of `src/lib`, so compiled programs can have any licence, closed ones too. The
runtime has two files of ZX-V (`bmath.c` and `fp.c`, with the fdlibm notice), and
programs are linked with the start-up code, headers, font, music library and
optional libraries of the ZX-V SDK (`src/lib/arch/zxv/sdk`): all MIT as well.

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
