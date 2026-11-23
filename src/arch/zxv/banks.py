# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""Code banks (ZX-V D109): zxbc --code-bank NAME[@REGION]=FILE.bas puts the SUBs and FUNCTIONs defined in
FILE (the main file or an #include) in a bank: they are not in memory with the rest of the program, but
are loaded from the .zxv itself the first time one of them is called, into a region of the user RAM the
banks of that region share. The program calls them as always.

- The translator (translator.py) puts each function of a bank in the section .zxvbank.REGION.NAME.
- Here, in the assembler of the program: those functions are renamed __zxvbank_real..., and each one
  gets a bridge with its own label that loads its bank if needed (sdk/zxvbankcode.c, __zxvbank_enter)
  and jumps to it. Calls between functions of the same bank go straight to them.
- BASIC functions do not return with `ret`: the caller leaves the return address on the stack (and the
  parameters above it) and the function jumps there (backend _leave). So the bridge keeps that address
  on its own stack and puts __zxvbasic_ret in its place: when the function returns there, the bank is
  no longer in use (__zxvbank_leave) and it goes on to the real return address, with the result (a0,
  a1) as it was. a0 and a1 also go through unchanged on the way in (the parameter of a FASTCALL).
- The linker script (zxv.ld with the regions as OVERLAY after the BSS) and the .zxv (zxvfile.py) put
  each bank after the image of the program, with its index.

Like tools/zxvbanks.py of the ZX-V SDK, for C (MIT).
"""

from __future__ import annotations

import os
import re

CODE_BANKS = "code_banks"  # The option (zxbc --code-bank NAME[@REGION]=FILE, repeated)
REGIONS_MAX = 8
NAME_MAX = 15
NEST_MAX = 32  # Bank calls one inside another
SLOT = 24  # What the bridge keeps: return address, bank, function, a0, a1, sp
LMA = 0x80000000  # Where the banks "are loaded" in the ELF (not in the RAM): one area per region
REAL = "__zxvbank_real"
SECTION = ".zxvbank."
RE_LABEL = re.compile(r"^([A-Za-z_.$][\w.$]*):$")
RE_JUMP = re.compile(r"^\tj ([A-Za-z_.$][\w.$]*)$")


class BankError(Exception):
    pass


def parse(spec: str) -> tuple[str, int, str]:
    """NAME[@REGION]=FILE -> (name, region, file)"""
    name, sep, path = spec.partition("=")
    if not sep or not path:
        raise BankError(f"--code-bank {spec}: it must be NAME[@REGION]=FILE")
    region = 0
    if "@" in name:
        name, _, r = name.partition("@")
        if not r.isdigit() or int(r) >= REGIONS_MAX:
            raise BankError(f"--code-bank {spec}: the region goes from 0 to {REGIONS_MAX - 1}")
        region = int(r)
    if not re.fullmatch(r"[A-Za-z0-9_]{1,%d}" % NAME_MAX, name):
        raise BankError(f"--code-bank {spec}: the name has 1 to {NAME_MAX} letters, digits or _")
    return name, region, path


def norm(path: str) -> str:
    return os.path.normcase(os.path.realpath(path))


class Banks:
    """The banks of a compilation: which file goes to which bank"""

    def __init__(self, specs: list[str]):
        self.by_file: dict[str, tuple[str, int]] = {}
        names = set()
        for spec in specs:
            name, region, path = parse(spec)
            if name in names:
                raise BankError(f"--code-bank: the bank '{name}' is repeated")
            names.add(name)
            if not os.path.isfile(path):
                raise BankError(f"--code-bank {spec}: cannot read '{path}'")
            key = norm(path)
            if key in self.by_file:
                raise BankError(f"--code-bank: '{path}' is in two banks")
            self.by_file[key] = (name, region)

    def of_function(self, node) -> tuple[str, int] | None:
        """The bank (name, region) of a FUNCTION node, by the file of its body; None: resident"""
        if not self.by_file:
            return None
        filename = body_file(getattr(node.ref, "body", None))
        return self.by_file.get(norm(filename)) if filename else None


def body_file(node) -> str | None:
    """The file of the first sentence of a function body (where the function is defined: the one of
    the function itself is where it was first used)"""
    if node is None:
        return None
    pending = [node]
    while pending:
        n = pending.pop(0)
        f = getattr(n, "filename", None)
        if f and getattr(n, "token", "") != "BLOCK":
            return f
        pending[0:0] = [c for c in getattr(n, "children", []) if c is not None]
    return None


def section(name: str, region: int) -> str:
    return f"{SECTION}{region}.{name}"


def begin(name: str, region: int) -> str:
    """What the translator puts before a function of a bank"""
    return f'\t.pushsection {section(name, region)},"ax",@progbits'


END = "\t.popsection"


def rewrite(code: list[str]) -> tuple[list[str], list[tuple[str, int]], dict[str, str]]:
    """Renames the functions of the banks in the assembler of the program and makes the calls between
    functions of the same bank direct. Returns the code, the banks used [(name, region)] in order and
    {function label: bank name}."""
    funcs: dict[str, str] = {}
    used: list[tuple[str, int]] = []
    bank, first = None, False
    for line in code:  # First pass: which label starts each function of a bank
        if line.startswith("\t.pushsection " + SECTION):
            region, name = line.split()[1].split(",")[0][len(SECTION) :].split(".", 1)
            bank, first = name, True
            if (name, int(region)) not in used:
                used.append((name, int(region)))
        elif line == END and bank:
            bank = None
        elif bank and first:
            m = RE_LABEL.match(line)
            if m:
                funcs[m.group(1)] = bank
                first = False
    out = []
    bank = None
    for line in code:
        if line.startswith("\t.pushsection " + SECTION):
            bank = line.split()[1].split(",")[0].split(".", 3)[3]
        elif line == END:
            bank = None
        elif bank:
            m = RE_LABEL.match(line)
            if m and funcs.get(m.group(1)) == bank:
                line = f"{REAL}{m.group(1)}:"
            m = RE_JUMP.match(line)
            if m and funcs.get(m.group(1)) == bank:
                line = f"\tj {REAL}{m.group(1)}"  # The same bank: no bridge
        out.append(line)
    return out, used, funcs


def bridges(used: list[tuple[str, int]], funcs: dict[str, str]) -> str:
    """The bridges of the functions of the banks, the common part and the tables of zxvbankcode.c"""
    ids = {name: i for i, (name, _) in enumerate(used)}
    regions = sorted({r for _, r in used})
    L = [
        "# Code banks (ZX-V D109): the bridges, from src/arch/zxv/banks.py",
        '\t.section .text.zxvbasic_bank,"ax",@progbits',
        "\t.balign 4",
        "\t.option push",
        "\t.option norelax",
        "# t5: the bank; t6: the function. The return address is at 0(sp) (BASIC); a0 and a1 go through.",
        "# What the bridge keeps goes in __zxvbasic_bstack, with the interrupts off only to push and pop.",
        "__zxvbasic_call:",
        "\tcsrrci t3, mstatus, 8",
        "\tla t0, __zxvbasic_bsp",
        "\tlw t1, 0(t0)",
        f"\taddi t2, t1, {SLOT}",
        "\tla t4, __zxvbasic_bstack_top",
        "\tbleu t2, t4, 1f",
        "\tandi t3, t3, 8",
        "\tcsrs mstatus, t3",
        "\tandi sp, sp, -16",
        "\ttail __zxvbank_overflow",
        "1:\tsw t2, 0(t0)",
        "\tandi t3, t3, 8",
        "\tcsrs mstatus, t3",
        "\tlw t4, 0(sp)",
        "\tsw t4, 0(t1)",
        "\tsw t5, 4(t1)",
        "\tsw t6, 8(t1)",
        "\tsw a0, 12(t1)",
        "\tsw a1, 16(t1)",
        "\tsw sp, 20(t1)",
        "\tmv a0, t5",
        "\tandi sp, sp, -16",
        "\tcall __zxvbank_enter",
        "\tla t0, __zxvbasic_bsp",
        "\tlw t1, 0(t0)",
        f"\taddi t1, t1, -{SLOT}",
        "\tlw sp, 20(t1)",
        "\tlw t6, 8(t1)",
        "\tlw a0, 12(t1)",
        "\tlw a1, 16(t1)",
        "\tla t4, __zxvbasic_ret",
        "\tsw t4, 0(sp)",
        "\tjr t6",
        "# The function has returned here, with its result in a0 (and a1) and its parameters off the stack.",
        "__zxvbasic_ret:",
        "\tla t0, __zxvbasic_bsp",
        "\tlw t1, 0(t0)",
        f"\taddi t1, t1, -{SLOT}",
        "\tsw a0, 12(t1)",
        "\tsw a1, 16(t1)",
        "\tsw sp, 20(t1)",
        "\tlw a0, 4(t1)",
        "\tandi sp, sp, -16",
        "\tcall __zxvbank_leave",
        "\tcsrrci t3, mstatus, 8",
        "\tla t0, __zxvbasic_bsp",
        "\tlw t1, 0(t0)",
        f"\taddi t1, t1, -{SLOT}",
        "\tlw sp, 20(t1)",
        "\tlw a0, 12(t1)",
        "\tlw a1, 16(t1)",
        "\tlw t4, 0(t1)",
        "\tsw t1, 0(t0)",
        "\tandi t3, t3, 8",
        "\tcsrs mstatus, t3",
        "\tjr t4",
    ]
    for label in sorted(funcs):
        L += [f"{label}:", f"\tli t5, {ids[funcs[label]]}", f"\tla t6, {REAL}{label}", "\tj __zxvbasic_call"]
    L += ["\t.option pop", "", '\t.section .rodata.zxvbank,"a"', "\t.balign 4"]
    L += ["\t.globl __zxvbank_nbanks, __zxvbank_banks, __zxvbank_nregions, __zxvbank_regions"]
    L += [f"__zxvbank_nbanks: .word {len(used)}", "__zxvbank_banks:"]
    L += [f"\t.word __zxvbank_name{i}, {r}" for i, (_, r) in enumerate(used)]
    L += [f"__zxvbank_nregions: .word {max(regions) + 1}", "__zxvbank_regions:"]
    for r in range(max(regions) + 1):
        L.append(f"\t.word __zxvbank_region{r}, __zxvbank_region{r}_end" if r in regions else "\t.word 0, 0")
    L += [f'__zxvbank_name{i}: .asciz "{name}"' for i, (name, _) in enumerate(used)]
    L += ['\t.section .data.zxvbank,"aw"', "\t.balign 4", "__zxvbasic_bsp: .word __zxvbasic_bstack"]
    L += ["# The table of GDB and the emulator: where each bank goes, its size, its place in the ELF, if loaded"]
    L += ["\t.globl _novlys, _ovly_table", f"_novlys: .word {len(used)}", "_ovly_table:"]
    L += [f"\t.word __zxvbank_vma{i}, __zxvbank_size{i}, __zxvbank_lma{i}, 0" for i in range(len(used))]
    L += ['\t.section .bss.zxvbank,"aw",@nobits', "\t.balign 4"]
    L += [f"__zxvbasic_bstack: .space {NEST_MAX * SLOT}", "__zxvbasic_bstack_top:", ""]
    return "\n".join(L)


def linker_script(sdk_ld: str, used: list[tuple[str, int]]) -> str:
    """zxv.ld of the SDK with the regions where its /* ZXVBANKS */ mark is"""
    if "/* ZXVBANKS */" not in sdk_ld:
        raise BankError("the zxv.ld of the SDK has no ZXVBANKS mark (an SDK older than the banks)")
    parts = []
    for r in sorted({r for _, r in used}):
        members = [(i, n) for i, (n, rr) in enumerate(used) if rr == r]
        lines = [f"    OVERLAY ALIGN(16) : NOCROSSREFS AT (0x{LMA + r * 0x01000000:08X}) {{"]
        for _, name in members:
            sec = section(name, r)
            lines += [f"        {sec} {{", f"            *({sec})", "            . = ALIGN(4);", "        }"]
        lines.append("    } > URAM")
        first = section(members[0][1], r)
        lines.append(f"    __zxvbank_region{r} = ABSOLUTE(ADDR({first}));")
        lines.append(f"    __zxvbank_region{r}_end = ABSOLUTE(.);")
        for i, name in members:
            sec = section(name, r)
            lines.append(
                f"    __zxvbank_vma{i} = ABSOLUTE(ADDR({sec})); __zxvbank_size{i} = SIZEOF({sec}); "
                f"__zxvbank_lma{i} = LOADADDR({sec});"
            )
        parts.append("\n".join(lines))
    return sdk_ld.replace("/* ZXVBANKS */", "/* ZXVBANKS (zxbc --code-bank) */\n" + "\n".join(parts))
