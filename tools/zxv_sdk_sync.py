#!/usr/bin/env python3
"""Copies into the zxv target the parts of the ZX-V SDK it uses, from a ZX-V repository:

    tools/zxv_sdk_sync.py PATH/TO/ZX-V

src/lib/arch/zxv/sdk gets crt0.S, zxv.ld, zxvlib.c, zxvbank.c and zxvbankcode.c (the banks, --code-bank), the headers, the music library (sdk/music, for
MUSICPLAY and SFX), the optional libraries (sdk/lib, ZX-V D111: zxvinput.bas, zxvsave.bas...) and the SDK licence (MIT), and src/lib/arch/zxv/runtime/zxb_font.c the ZX-V
font, converted by its rom/tools/mkfont.py. Then check with tests/zxv/run.py that the programs
still work.
"""

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TARGET = os.path.join(HERE, "..", "src", "lib", "arch", "zxv")
FILES = ["crt0.S", "zxv.ld", "zxvlib.c", "zxvbank.c", "zxvbankcode.c", "LICENSE"]
HEADERS = ["zxv.h", "zxv_api.h", "zxv_bank.h", "zxv_gfx.h", "zxv_music.h",
           # sdk/lib (ZX-V D111)
           "zxv_math.h", "zxv_m7.h", "zxv_mapmove.h", "zxv_fade.h", "zxv_input.h", "zxv_save.h", "zxv_inflate.h",
           "zxv_font.h", "zxv_3d.h"]


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    zxv = sys.argv[1]
    sdk = os.path.join(zxv, "sdk")
    if not os.path.isfile(os.path.join(sdk, "zxv.ld")):
        print(f"{zxv} is not a ZX-V repository (no sdk/zxv.ld)", file=sys.stderr)
        return 1
    dst = os.path.join(TARGET, "sdk")
    for f in FILES:
        shutil.copyfile(os.path.join(sdk, f), os.path.join(dst, f))
    for f in HEADERS:
        shutil.copyfile(os.path.join(sdk, "include", f), os.path.join(dst, "include", f))
    music = os.path.join(dst, "music")
    os.makedirs(music, exist_ok=True)
    music_files = sorted(f for f in os.listdir(os.path.join(sdk, "music")) if f.endswith((".c", ".h")))
    for f in music_files:
        shutil.copyfile(os.path.join(sdk, "music", f), os.path.join(music, f))
    lib = os.path.join(dst, "lib")
    os.makedirs(lib, exist_ok=True)
    lib_files = sorted(f for f in os.listdir(os.path.join(sdk, "lib")) if f.endswith(".c"))
    for f in lib_files:
        shutil.copyfile(os.path.join(sdk, "lib", f), os.path.join(lib, f))
    font = os.path.join(TARGET, "runtime", "zxb_font.c")
    mkfont = os.path.join(zxv, "rom", "tools", "mkfont.py")
    subprocess.run(
        # With / in the path: mkfont.py writes its last part in a comment of the file
        [sys.executable, mkfont, os.path.join(zxv, "rom", "font", "zxv8x8.txt").replace("\\", "/"), font, "zxb_font"],
        check=True,
    )
    print(f"copied {len(FILES) + len(HEADERS) + len(music_files) + len(lib_files)} files and the font")
    return 0


if __name__ == "__main__":
    sys.exit(main())
