#!/usr/bin/env python3
"""Copies into the zxv target the parts of the ZX-V SDK it uses, from a ZX-V repository:

    tools/zxv_sdk_sync.py PATH/TO/ZX-V

src/lib/arch/zxv/sdk gets crt0.S, zxv.ld, zxvlib.c, the headers, the music library (sdk/music, for
MUSICPLAY and SFX) and the SDK licence (MIT), and src/lib/arch/zxv/runtime/zxb_font.c the ZX-V
font, converted by its rom/tools/mkfont.py. Then check with tests/zxv/run.py that the programs
still work.
"""

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TARGET = os.path.join(HERE, "..", "src", "lib", "arch", "zxv")
FILES = ["crt0.S", "zxv.ld", "zxvlib.c", "LICENSE"]
HEADERS = ["zxv.h", "zxv_api.h", "zxv_gfx.h", "zxv_music.h"]


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
    font = os.path.join(TARGET, "runtime", "zxb_font.c")
    mkfont = os.path.join(zxv, "rom", "tools", "mkfont.py")
    subprocess.run(
        [sys.executable, mkfont, os.path.join(zxv, "rom", "font", "zxv8x8.txt"), font, "zxb_font"], check=True
    )
    print(f"copied {len(FILES) + len(HEADERS) + len(music_files)} files and the font")
    return 0


if __name__ == "__main__":
    sys.exit(main())
