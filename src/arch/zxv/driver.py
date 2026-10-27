# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""ZX-V output: GNU assembler, linked with the C runtime and the ZX-V SDK into a .zxv program.

Needs the RISC-V GCC toolchain and the ZX-V SDK:
  ZXV_SDK     the sdk folder of ZX-V (crt0.S, zxv.ld, include/, tools/elf2zxv.py)
  ZXV_CROSS   the toolchain prefix (riscv64-unknown-elf- by default)
  ZXV_CFLAGS  more options for the runtime (-DZXB_TEST: PRINT also goes to the zxv-cli --test console)
"""

import os
import shlex
import subprocess
import sys
import tempfile

import src.api.optimize
from src.api import errmsg
from src.api import global_ as gl
from src.api.config import OPTIONS
from src.api.utils import open_file
from src.zxbc.args_parser import FileType

MARCH = "rv32imac_zicsr_zifencei_zba_zbb_zbs_zbkb_zcb"
RUNTIME_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "lib", "arch", "zxv", "runtime")

PROLOGUE = """\
\t.option norelax
# zxb_ccall FUNCTION: calls a C function from an ASM block (the stack is aligned to 16)
\t.macro zxb_ccall func
\tmv s1, sp
\tandi sp, sp, -16
\tcall \\func
\tmv sp, s1
\t.endm
\t.text
\t.globl main
\t.type main, @function
main:
\taddi sp, sp, -16
\tsw ra, 12(sp)
\tsw s0, 8(sp)
\tsw s1, 4(sp)
\tla t0, __zxb_sp
\tsw sp, 0(t0)
\tcall zxb_init
\tmv s0, sp
\tj __zxb_main
__zxb_end:
\tla t0, __zxb_sp
\tlw sp, 0(t0)
\tcall zxb_finish
\tlw ra, 12(sp)
\tlw s0, 8(sp)
\tlw s1, 4(sp)
\taddi sp, sp, 16
\tret
\t.bss
\t.balign 4
__zxb_sp:
\t.zero 4
\t.text
__zxb_main:"""


def finish(backend) -> int:
    """Emits the program (its code is in backend.MEMORY) and builds the output file"""
    from src.arch import target
    from src.zxbc import zxbparser

    code_quads = list(backend.MEMORY)
    backend.MEMORY[:] = []

    # Global variables
    var_checker = src.api.optimize.VariableVisitor()
    var_checker.visit(zxbparser.data_ast)
    target.VarTranslator(backend=backend).visit(zxbparser.data_ast)
    if gl.has_errors:
        return 1
    var_quads = list(backend.MEMORY)

    # Variables at a fixed address below 64K (DIM x AT 23560) are emulated
    for quad in var_quads + code_quads:
        if quad.instr == "deflabel":
            try:
                value = int(quad.args[1])
            except ValueError:
                continue
            if value < 0x10000:
                backend.low_labels[quad.args[0]] = value

    try:
        backend.MEMORY[:] = code_quads
        code = backend.emit()
        backend.MEMORY[:] = var_quads
        data = backend.emit()
    except NotImplementedError as e:
        errmsg.error(0, str(e))
        return 1
    finally:
        backend.MEMORY[:] = []
    if gl.has_errors:
        return 1

    asm = PROLOGUE.split("\n") + code + data + backend.data
    asm_text = "\n".join(asm) + "\n"

    if OPTIONS.output_file_type == FileType.ASM:
        with open_file(OPTIONS.output_filename, "wt", "utf-8") as f:
            f.write(asm_text)
        return 0

    return build(asm_text, OPTIONS.output_filename)


def run(cmd: list[str]) -> bool:
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        errmsg.error(0, f"zxv: '{os.path.basename(cmd[0])}' failed")
        return False
    return True


def build(asm_text: str, output: str) -> int:
    sdk = os.environ.get("ZXV_SDK")
    if not sdk or not os.path.isfile(os.path.join(sdk, "zxv.ld")):
        errmsg.error(0, "zxv: set ZXV_SDK to the sdk folder of ZX-V (or use -f asm)")
        return 1
    cross = os.environ.get("ZXV_CROSS", "riscv64-unknown-elf-")
    rom = os.path.join(sdk, "..", "rom")
    runtime = os.path.abspath(RUNTIME_DIR)
    sources = sorted(
        os.path.join(runtime, x) for x in os.listdir(runtime) if x.endswith(".c") or x.endswith(".S")
    )

    with tempfile.TemporaryDirectory(prefix="zxb-zxv-") as tmp:
        prog = os.path.join(tmp, "prog.s")
        with open(prog, "wt", encoding="utf-8") as f:
            f.write(asm_text)

        font = os.path.join(tmp, "font.c")
        if not run(
            [
                sys.executable,
                os.path.join(rom, "tools", "mkfont.py"),
                os.path.join(rom, "font", "zxv8x8.txt"),
                font,
                "zxb_font",
            ]
        ):
            return 1

        elf = os.path.join(tmp, "prog.elf")
        cmd = [
            cross + "gcc",
            f"-march={MARCH}",
            "-mabi=ilp32",
            "-Os",
            "-g",
            "-std=gnu11",
            "-ffreestanding",
            "-fno-builtin",
            "-fno-tree-loop-distribute-patterns",
            "-I" + os.path.join(sdk, "include"),
            "-I" + runtime,
            *shlex.split(os.environ.get("ZXV_CFLAGS", "")),
            "-nostdlib",
            "-nostartfiles",
            "-static",
            "-T",
            os.path.join(sdk, "zxv.ld"),
            "-Wl,--gc-sections",
            "-Wl,--no-warn-rwx-segments",
            os.path.join(sdk, "crt0.S"),
            os.path.join(sdk, "zxvlib.c"),
            *sources,
            font,
            prog,
            "-lgcc",
            "-o",
            elf,
        ]
        if not run(cmd):
            return 1

        name = os.path.splitext(os.path.basename(output))[0][:27]
        if not run([sys.executable, os.path.join(sdk, "tools", "elf2zxv.py"), elf, output, "--name", name]):
            return 1

        keep = os.environ.get("ZXV_KEEP_ELF")
        if keep:
            import shutil

            shutil.copy(elf, keep)

    return 0
