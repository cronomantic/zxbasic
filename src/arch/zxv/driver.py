# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""ZX-V output: GNU assembler, linked with the C runtime and the parts of the ZX-V SDK it needs
(src/lib/arch/zxv/sdk) into a .zxv program.

Needs the RISC-V GCC toolchain:
  ZXV_CROSS   the toolchain prefix (riscv64-unknown-elf- by default)
  ZXV_CFLAGS  more options for the runtime (-DZXB_TEST: PRINT also goes to the zxv-cli --test console)

--loading-screen FILE (OPTIONS.loading_screen) puts a loading screen in the .zxv, a PNG or a .gfx (screen.py).
--embed FILE (OPTIONS.embed, repeated) puts files inside the .zxv, for when they are not in the current folder
(zxb_embed.c).
--code-bank NAME[@REGION]=FILE (OPTIONS.code_banks, repeated) puts the SUBs and FUNCTIONs of FILE in a code bank
(banks.py).
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
from src.arch.zxv import banks
from src.arch.zxv.screen import loading_screen
from src.arch.zxv.zxvfile import ZxvError, zxv_from_elf
from src.zxbc.args_parser import FileType

MARCH = "rv32imac_zicsr_zifencei_zba_zbb_zbs_zbkb_zcb"
LIB_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "lib", "arch", "zxv")
RUNTIME_DIR = os.path.join(LIB_DIR, "runtime")
SDK_DIR = os.path.join(LIB_DIR, "sdk")
LOADING_SCREEN = "loading_screen"  # The option (zxbc --loading-screen FILE)
EMBED = "embed"  # The option (zxbc --embed FILE, repeated)
EMBED_NAME_MAX = 63  # The longest file name of ZX-V
CODE_BANKS = banks.CODE_BANKS  # The option (zxbc --code-bank NAME[@REGION]=FILE, repeated)

_banks: banks.Banks | None = None


def code_banks() -> banks.Banks:
    """The code banks of this compilation (--code-bank), read once"""
    global _banks
    if _banks is None:
        specs = OPTIONS[CODE_BANKS].value if CODE_BANKS in OPTIONS else []
        try:
            _banks = banks.Banks(list(specs or []))
        except banks.BankError as e:
            errmsg.error(0, f"zxv: {e}")
            _banks = banks.Banks([])
    return _banks


def reset_code_banks() -> None:
    global _banks
    _banks = None

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

    # The functions of the code banks: renamed, with their bridges (banks.py)
    code, used, funcs = banks.rewrite(code)
    bank_asm = banks.bridges(used, funcs) if used else ""
    for spec_name, _ in code_banks().by_file.values():
        if all(spec_name != name for name, _ in used):
            errmsg.warning(0, f"zxv: --code-bank {spec_name}: no SUB or FUNCTION of its file is used")

    # The variables, between ZXBASIC_USER_DATA and its end (SAVE/LOAD ... DATA without a name)
    start, end = gl.ZXBASIC_USER_DATA, gl.ZXBASIC_USER_DATA + "_END"
    data = (
        ["\t.data", "\t.balign 4", f"{start}:"]
        + data
        + ["\t.data", f"{end}:", f"\t.set {gl.ZXBASIC_USER_DATA_LEN}, {end} - {start}"]
    )
    asm = PROLOGUE.split("\n") + code + data + backend.data
    asm_text = "\n".join(asm) + "\n" + bank_asm + ("\n" if bank_asm else "")

    if OPTIONS.output_file_type == FileType.ASM:
        with open_file(OPTIONS.output_filename, "wt", "utf-8") as f:
            f.write(asm_text)
        return 0

    return build(asm_text, OPTIONS.output_filename, used)


def embed_table(files: list[str]) -> str | None:
    """The assembler of zxb_embed_table (zxb_embed.c) with the files of --embed, or None after an error.

    Each file goes by its name without the folder; two with the same name (without telling capitals apart)
    are an error, because the program could only find one of them."""
    names: dict[str, str] = {}
    for path in files:
        name = os.path.basename(path)
        if not os.path.isfile(path):
            errmsg.error(0, f"zxv: --embed: cannot read '{path}'")
            return None
        if (
            not name.isascii()
            or not name.isprintable()
            or len(name) > EMBED_NAME_MAX
            or any(c in name for c in '\\:*?"<>|')
            or name.endswith((" ", "."))
        ):
            errmsg.error(0, f"zxv: --embed: '{name}' is not a valid ZX-V file name")
            return None
        if name.lower() in names:
            errmsg.error(0, f"zxv: --embed: '{path}' and '{names[name.lower()]}' have the same name")
            return None
        names[name.lower()] = path

    lines = ['\t.section .rodata.zxb_embed_table,"a"', "\t.balign 4", "\t.globl zxb_embed_table", "zxb_embed_table:"]
    lines.append(f"\t.word {len(files)}")
    for i in range(len(files)):
        lines.append(f"\t.word __zxb_embed_name{i}, __zxb_embed_data{i}, __zxb_embed_end{i} - __zxb_embed_data{i}")
    for i, path in enumerate(files):
        full = os.path.abspath(path).replace("\\", "/").replace('"', '\\"')
        lines += [
            f"__zxb_embed_name{i}:",
            f'\t.asciz "{os.path.basename(path)}"',
            "\t.balign 4",
            f"__zxb_embed_data{i}:",
            f'\t.incbin "{full}"',
            f"__zxb_embed_end{i}:",
            "\t.balign 4",
        ]
    return "\n".join(lines) + "\n"


def run(cmd: list[str]) -> bool:
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        errmsg.error(0, f"zxv: '{os.path.basename(cmd[0])}' failed")
        return False
    return True


def build(asm_text: str, output: str, used: list[tuple[str, int]] = ()) -> int:
    cross = os.environ.get("ZXV_CROSS", "riscv64-unknown-elf-")
    sdk = os.path.abspath(SDK_DIR)
    runtime = os.path.abspath(RUNTIME_DIR)
    sources = sorted(
        os.path.join(runtime, x) for x in os.listdir(runtime) if x.endswith(".c") or x.endswith(".S")
    )
    # The music library of the SDK (MUSICPLAY, SFX): with --gc-sections, only if the program uses it
    music = os.path.join(sdk, "music")
    sources += sorted(os.path.join(music, x) for x in os.listdir(music) if x.endswith(".c"))
    # The optional libraries of the SDK (ZX-V D111: zxvinput.bas, zxvsave.bas, zxvfont.bas...): the same
    lib = os.path.join(sdk, "lib")
    sources += sorted(os.path.join(lib, x) for x in os.listdir(lib) if x.endswith(".c"))

    # The loading screen: the ROM shows it while it loads the program and, in mode 0 (the one of the program),
    # the program starts with it (the runtime does not clear the screen: ZXB_LOADING_SCREEN)
    screen, defines = b"", []
    screen_file = OPTIONS[LOADING_SCREEN].value if LOADING_SCREEN in OPTIONS else None
    if screen_file:
        try:
            screen = loading_screen(screen_file)
        except ZxvError as e:
            errmsg.error(0, f"zxv: {e}")
            return 1
        if screen[0] == 0:
            defines.append("-DZXB_LOADING_SCREEN")
    if OPTIONS.array_check:
        defines.append("-DZXB_CHECK_ARRAYS")  # --debug-array: zxb_array checks the indexes

    # The files of --embed: a table the runtime looks in (zxb_embed.c), in the program
    embedded = OPTIONS[EMBED].value if EMBED in OPTIONS else []
    table = embed_table(embedded) if embedded else ""
    if table is None:
        return 1

    with tempfile.TemporaryDirectory(prefix="zxb-zxv-") as tmp:
        prog = os.path.join(tmp, "prog.s")
        with open(prog, "wt", encoding="utf-8") as f:
            f.write(asm_text)
        extra = []
        if table:
            extra.append(os.path.join(tmp, "embed.s"))
            with open(extra[0], "wt", encoding="utf-8") as f:
                f.write(table)

        # The code banks: zxv.ld with their regions, and the bank runtime of the SDK
        ld = os.path.join(sdk, "zxv.ld")
        if used:
            try:
                with open(ld, encoding="utf-8") as f:
                    script = banks.linker_script(f.read(), used)
            except banks.BankError as e:
                errmsg.error(0, f"zxv: {e}")
                return 1
            ld = os.path.join(tmp, "zxv.ld")
            with open(ld, "wt", encoding="utf-8") as f:
                f.write(script)
            extra += [os.path.join(sdk, "zxvbank.c"), os.path.join(sdk, "zxvbankcode.c")]

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
            "-ffunction-sections",  # With --gc-sections, only what the program uses
            "-fdata-sections",
            "-I" + os.path.join(sdk, "include"),
            "-I" + runtime,
            "-I" + music,
            *defines,
            *shlex.split(os.environ.get("ZXV_CFLAGS", "")),
            "-nostdlib",
            "-nostartfiles",
            "-static",
            "-T",
            ld,
            "-Wl,--gc-sections",
            "-Wl,--no-warn-rwx-segments",
            os.path.join(sdk, "crt0.S"),
            os.path.join(sdk, "zxvlib.c"),
            *sources,
            prog,
            *extra,
            "-lgcc",
            "-o",
            elf,
        ]
        if not run(cmd):
            return 1

        name = os.path.splitext(os.path.basename(output))[0][:27]
        try:
            with open(elf, "rb") as f:
                zxv = zxv_from_elf(f.read(), name, screen)
        except ZxvError as e:
            errmsg.error(0, f"zxv: {e}")
            return 1
        with open_file(output, "wb") as f:
            f.write(zxv)

        keep = os.environ.get("ZXV_KEEP_ELF")
        if keep:
            import shutil

            shutil.copy(elf, keep)

    return 0
