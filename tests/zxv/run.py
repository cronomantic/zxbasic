#!/usr/bin/env python3
"""Compiles the ZX-V tests, runs them in zxv-cli and compares what they print with NAME.out.

    run.py [--update] [NAME...]

Needs the RISC-V toolchain in the PATH and:
  ZXV_SDK   the sdk folder of ZX-V
  ZXV_CLI   zxv-cli (the ZX-V emulator without window)
  ZXV_ROM   the ROM (optional: the one of the emulator otherwise)

The runtime is built with -DZXB_TEST, so PRINT also goes to the zxv-cli console.
A test may have NAME.cfg with the zxv-cli options (--frames, --type...), and
NAME.pix with pixels its screen must have: X,Y=RRGGBB in the 720x576 image.
"""

import os
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ZXBC = os.path.join(HERE, "..", "..", "zxbc.py")


def check_pixels(bmp: str, pix: str) -> str:
    """Compares the screenshot with NAME.pix; returns the differences"""
    data = open(bmp, "rb").read()
    (off,) = struct.unpack_from("<I", data, 10)
    w, h = struct.unpack_from("<ii", data, 18)
    row = (w * 3 + 3) & ~3
    out = ""
    for spec in open(pix, encoding="utf-8").read().split():
        pos, want = spec.split("=")
        x, y = map(int, pos.split(","))
        i = off + (h - 1 - y) * row + x * 3
        got = "%02X%02X%02X" % (data[i + 2], data[i + 1], data[i])
        if got != want.upper():
            out += f"pixel ({x},{y}) is {got}, not {want.upper()}\n"
    return out


def run_test(name: str, update: bool) -> bool:
    bas = os.path.join(HERE, name + ".bas")
    with tempfile.TemporaryDirectory(prefix="zxv-test-") as tmp:
        sd = os.path.join(tmp, "sd")
        os.mkdir(sd)
        prog = os.path.join(sd, "prog.zxv")
        env = dict(os.environ, ZXV_CFLAGS="-DZXB_TEST")
        result = subprocess.run(
            [sys.executable, ZXBC, "--arch", "zxv", "-o", prog, bas], env=env, capture_output=True, text=True
        )
        if result.returncode:
            print(f"{name}: compilation failed\n{result.stdout}{result.stderr}")
            return False

        opts = ["--frames", "3000"]
        cfg = os.path.join(HERE, name + ".cfg")
        if os.path.exists(cfg):
            opts = shlex.split(open(cfg, encoding="utf-8").read())
        if "--type" not in opts:
            opts += ["--type", 'load "prog"\\n']
        cmd = [os.environ["ZXV_CLI"], "--sd", sd, "--test", *opts]
        if os.environ.get("ZXV_ROM"):
            cmd += ["--rom", os.environ["ZXV_ROM"]]
        shot = os.path.join(os.environ.get("ZXV_SHOT", tmp), name + ".bmp")
        cmd += ["--screenshot", shot]
        result = subprocess.run(cmd, capture_output=True)
        output = result.stdout.decode("latin-1").replace("\r\n", "\n")
        output += f"[exit {result.returncode}]\n"
        pix = os.path.join(HERE, name + ".pix")
        if os.path.exists(pix):
            output += check_pixels(shot, pix)

    expected_file = os.path.join(HERE, name + ".out")
    if update or not os.path.exists(expected_file):
        with open(expected_file, "w", encoding="latin-1", newline="\n") as f:
            f.write(output)
        print(f"{name}: written\n{output}")
        return True

    expected = open(expected_file, encoding="latin-1").read()
    if output != expected:
        print(f"{name}: FAILED\n--- expected\n{expected}--- got\n{output}")
        return False
    print(f"{name}: ok")
    return True


def main() -> int:
    sys.stdout.reconfigure(errors="backslashreplace")
    args = sys.argv[1:]
    update = "--update" in args
    names = [x for x in args if x != "--update"]
    if not names:
        names = sorted(x[:-4] for x in os.listdir(HERE) if x.endswith(".bas"))
    if not shutil.which(os.environ.get("ZXV_CLI", "")):
        print("Set ZXV_CLI to zxv-cli")
        return 2
    failed = [x for x in names if not run_test(x, update)]
    print(f"{len(names) - len(failed)} of {len(names)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
