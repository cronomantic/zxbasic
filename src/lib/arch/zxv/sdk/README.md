# The ZX-V SDK, as the zxv target uses it

Copies of files of the [ZX-V](https://github.com/cronomantic/ZX-V) SDK, so that
compiling for ZX-V only needs the RISC-V GCC toolchain:

| File | |
|---|---|
| `crt0.S` | The start of a `.zxv` program |
| `zxv.ld` | Its memory map, for the linker |
| `zxvlib.c` | The few C library functions GCC may call by itself |
| `include/` | The addresses of the machine and the calls of its ROM API |

The runtime has the ZX-V font too (`../runtime/zxb_font.c`), converted by its
`rom/tools/mkfont.py`. They are MIT (`LICENSE`), like the runtime, so they do not
restrict the programs compiled with them.

To update them from a ZX-V repository: `tools/zxv_sdk_sync.py PATH/TO/ZX-V`.
