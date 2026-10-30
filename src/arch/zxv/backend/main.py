# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""Backend for ZX-V: RISC-V (RV32IMAC + Zba/Zbb/Zbs) assembler for GNU as.

It follows the same model as the Z80 backend, so the translator does not need
to change:

- Temporaries live in the stack, in the order the translator uses them: the
  second operand of an instruction is always on top. Every slot is 4 bytes,
  except floats (doubles, 8 bytes: low word at the lower address).
- Values are kept in 32-bit registers, zero or sign extended to their type.
- BASIC functions and GOSUB keep the return address in the stack, like the
  Z80 `call`: `s0` is the frame pointer (old `s0` at 0(s0), return address at
  4(s0) and parameters from 8(s0)); locals are below `s0`.
- The runtime is in C (ilp32): `fparam` values go to the first argument
  registers and the stack parameters are popped into the next ones, top first.
- Accesses through computed addresses below 65536 go to the runtime, which
  emulates the ZX Spectrum memory map there (screen, attributes, system vars),
  as the ZX-V BASIC does. Misaligned accesses go there too.
"""

from __future__ import annotations

import re
import struct

from src.api.errmsg import error
from src.arch.interface.backend import BackendInterface
from src.arch.interface.quad import Quad

__all__ = ("Backend", "RUNTIME_SIGNATURES", "c_name")

SIZE = {"u8": 1, "i8": 1, "bool": 1, "u16": 2, "i16": 2, "u32": 4, "i32": 4, "f16": 4, "f": 8, "str": 4}
TYPES = tuple(SIZE)
RE_INSTR = re.compile(r"^(?P<op>[a-z]+?)(?P<t>i8|u8|i16|u16|i32|u32|f16|f|str|bool)?$")

CORE = ".core."

# Runtime routines (.core.NAME) that take parameters from the stack: their types,
# top of the stack first. They follow the `fparam` ones in the C call.
RUNTIME_SIGNATURES: dict[str, tuple[str, ...]] = {
    "PRINT_AT": ("u8",),
    "PLOT": ("u8",),
    "DRAW": ("i16",),
    "DRAW3": ("i16", "i16"),
    "CIRCLE": ("i8", "i8"),
    "BEEP": ("f",),
    "STRSLICE": ("u16", "u16", "str"),
    "LETSUBSTR": ("u32", "u32", "u8", "str"),
    "ARRAYSTR_FREE_MEM": ("u16",),
    "LBOUND": ("u16",),
    "UBOUND": ("u16",),
    "STR_ARRAYCOPY": ("u16", "u32", "u32"),
    "SAVE_CODE": ("u32", "u32", "str"),
    "LOAD_CODE": ("u8", "u32", "u32", "str"),
}

# Routines without stack parameters (only fparams or none)
RUNTIME_SIMPLE = {
    "CLS",
    "BORDER",
    "COPY_ATTR",
    "PRINT_EOL",
    "PRINT_COMMA",
    "PRINT_TAB",
    "PRINTSTR",
    "PRINTU8",
    "PRINTI8",
    "PRINTU16",
    "PRINTI16",
    "PRINTU32",
    "PRINTI32",
    "PRINTF16",
    "PRINTF",
    "STR_FAST",
    "VAL",
    "ASC",
    "MEM_FREE",
    "SIN",
    "COS",
    "TAN",
    "ASIN",
    "ACOS",
    "ATAN",
    "EXP",
    "LN",
    "SQRT",
    "RND",
    "RANDOMIZE",
    "PAUSE",
    "INKEY",
    "STOP",
    "ERROR",
    "USR",
    "SGNI8",
    "SGNU8",
    "SGNI16",
    "SGNU16",
    "SGNI32",
    "SGNU32",
    "SGNF16",
    "SGNF",
    "READ",
    "RESTORE",
}
for _attr in ("INK", "PAPER", "BRIGHT", "FLASH", "INVERSE", "OVER", "BOLD", "ITALIC"):
    RUNTIME_SIMPLE.add(_attr)
    RUNTIME_SIMPLE.add(_attr + "_TMP")


def c_name(label: str) -> str:
    """C function of a runtime routine: .core.__PRINTSTR -> zxb_PRINTSTR"""
    return "zxb_" + label[len(CORE) :].lstrip("_")


def is_number(op: str) -> bool:
    try:
        float(op)
    except ValueError:
        return False
    return True


def float_words(value: float) -> tuple[int, int]:
    lo, hi = struct.unpack("<II", struct.pack("<d", value))
    return lo, hi


class Backend(BackendInterface):
    """Implements a backend for ZX-V (RISC-V)"""

    MEMORY: list[Quad] = []

    def init(self) -> None:
        self.MEMORY.clear()
        self.out: list[str] = []
        self.data: list[str] = []  # Data generated while emitting code (local initializers...)
        self.section = ""
        self.label_count = 0
        self.fast: list[tuple[str, str]] = []  # Pending fparams: (type, operand)
        self.fast_regs = 0  # Argument registers used by them
        self.low_labels: dict[str, int] = {}  # Labels defined at a fixed address below 64K (DIM ... AT)
        self.end_label = "__zxb_end"

    # -----------------------------------------------------------------
    # Output helpers
    # -----------------------------------------------------------------
    def asm(self, *lines: str) -> None:
        self.out.extend(lines)

    def new_label(self) -> str:
        self.label_count += 1
        return f".Lzxv{self.label_count}"

    def text(self) -> None:
        if self.section != ".text":
            self.section = ".text"
            self.asm("\t.text")

    def datasec(self) -> None:
        if self.section != ".data":
            self.section = ".data"
            self.asm("\t.data")

    def ccall(self, func: str) -> None:
        """Calls a C function: the stack is realigned to 16 bytes as the ABI requires"""
        self.asm("\tmv s1, sp", "\tandi sp, sp, -16", f"\tcall {func}", "\tmv sp, s1")

    @staticmethod
    def regs(ty: str, first: str = "a0") -> tuple[str, ...]:
        """Registers for a value of the given type (a pair for floats)"""
        if ty != "f":
            return (first,)
        return first, "a%i" % (int(first[1]) + 1)

    def push(self, ty: str, regs: tuple[str, ...]) -> None:
        if ty == "f":
            self.asm("\taddi sp, sp, -8", f"\tsw {regs[0]}, 0(sp)", f"\tsw {regs[1]}, 4(sp)")
        else:
            self.asm("\taddi sp, sp, -4", f"\tsw {regs[0]}, 0(sp)")

    def pop(self, ty: str, regs: tuple[str, ...]) -> None:
        if ty == "f":
            self.asm(f"\tlw {regs[0]}, 0(sp)", f"\tlw {regs[1]}, 4(sp)", "\taddi sp, sp, 8")
        else:
            self.asm(f"\tlw {regs[0]}, 0(sp)", "\taddi sp, sp, 4")

    def norm(self, ty: str, r: str) -> None:
        """Zero or sign extends a value to its type"""
        if ty in ("u8", "bool"):
            self.asm(f"\tandi {r}, {r}, 255")
        elif ty == "i8":
            self.asm(f"\tsext.b {r}, {r}")
        elif ty == "u16":
            self.asm(f"\tzext.h {r}, {r}")
        elif ty == "i16":
            self.asm(f"\tsext.h {r}, {r}")

    def load_mem(self, ty: str, regs: tuple[str, ...], base: str, off: int = 0) -> None:
        size = SIZE[ty]
        if ty == "f":
            self.asm(f"\tlw {regs[0]}, {off}({base})", f"\tlw {regs[1]}, {off + 4}({base})")
        elif size == 4:
            self.asm(f"\tlw {regs[0]}, {off}({base})")
        elif size == 2:
            self.asm(f"\t{'lh' if ty == 'i16' else 'lhu'} {regs[0]}, {off}({base})")
        else:
            self.asm(f"\t{'lb' if ty == 'i8' else 'lbu'} {regs[0]}, {off}({base})")

    def store_mem(self, ty: str, regs: tuple[str, ...], base: str, off: int = 0) -> None:
        size = SIZE[ty]
        if ty == "f":
            self.asm(f"\tsw {regs[0]}, {off}({base})", f"\tsw {regs[1]}, {off + 4}({base})")
        else:
            self.asm(f"\t{ {4: 'sw', 2: 'sh', 1: 'sb'}[size]} {regs[0]}, {off}({base})")

    def checked_load(self, ty: str, regs: tuple[str, ...]) -> None:
        """Loads from the address in t0, emulating the Spectrum memory below 64K"""
        size = SIZE[ty]
        slow, done = self.new_label(), self.new_label()
        self.asm("\tsrli t1, t0, 16", f"\tbeqz t1, {slow}")
        if size > 1:
            self.asm(f"\tandi t1, t0, {min(size, 4) - 1}", f"\tbnez t1, {slow}")
        self.load_mem(ty, regs, "t0")
        self.asm(f"\tj {done}", f"{slow}:", f"\tli t1, {size}")
        self.asm("\tmv t2, a0", "\tmv t3, a1", "\tcall __zxb_rd")
        self.norm(ty, "a0")
        if regs[0] != "a0":
            self.asm(f"\tmv {regs[0]}, a0")
            if ty == "f":
                self.asm(f"\tmv {regs[1]}, a1")
            if regs[0] != "a1":
                self.asm("\tmv a0, t2")
            if "a1" not in regs:
                self.asm("\tmv a1, t3")
        elif ty != "f":
            self.asm("\tmv a1, t3")
        self.asm(f"{done}:")

    def checked_store(self, ty: str) -> None:
        """Stores a0 (a0:a1) at the address in t0, emulating the Spectrum memory below 64K"""
        size = SIZE[ty]
        slow, done = self.new_label(), self.new_label()
        self.asm("\tsrli t1, t0, 16", f"\tbeqz t1, {slow}")
        if size > 1:
            self.asm(f"\tandi t1, t0, {min(size, 4) - 1}", f"\tbnez t1, {slow}")
        self.store_mem(ty, self.regs(ty), "t0")
        self.asm(f"\tj {done}", f"{slow}:", f"\tli t1, {size}", "\tcall __zxb_wr", f"{done}:")

    def immediate(self, ty: str, op: str, regs: tuple[str, ...]) -> None:
        if ty == "f":
            lo, hi = float_words(float(op))
            self.asm(f"\tli {regs[0]}, {lo}", f"\tli {regs[1]}, {hi}")
        elif ty == "f16":
            self.asm(f"\tli {regs[0]}, {int(float(op) * 65536) & 0xFFFFFFFF}")
        else:
            v = int(float(op)) & (1 << 8 * SIZE[ty]) - 1
            if ty[0] == "i" and v >> (8 * SIZE[ty] - 1):
                v -= 1 << 8 * SIZE[ty]
            self.asm(f"\tli {regs[0]}, {v}")

    def address(self, addr: int) -> bool:
        """Puts a fixed address in t0. Returns True if it has to be emulated."""
        self.asm(f"\tli t0, {addr & 0xFFFFFFFF}")
        return addr & 0xFFFFFFFF < 0x10000

    @staticmethod
    def is_temp(op: str) -> bool:
        """Temporaries are popped from the stack (strings: and owned; $t are not)"""
        op = op.lstrip("*")
        return op[0] not in "_#$." and not is_number(op)

    def get(self, op: str, ty: str, regs: tuple[str, ...]) -> None:
        """Loads an operand into regs (see the Z80 backend for the operand syntax):
        N (immediate), *N (memory at N), #label (address), _var (memory at label),
        *_var (memory pointed by the var), t (temporary, popped), *t (memory pointed by t)
        $t (popped too)
        """
        indirect = op[0] == "*"
        if indirect:
            op = op[1:]
        immediate = op[0] == "#"
        if immediate:
            op = op[1:]

        if op in self.low_labels:  # DIM ... AT address: like a number
            if indirect and not immediate:
                raise NotImplementedError("zxv: pointer variable at a fixed address below 64K")
            indirect = not immediate or indirect
            op = str(self.low_labels[op])

        if is_number(op):
            if not indirect:
                self.immediate(ty, op, regs)
                return
            addr = int(float(op))
            if self.address(addr) or addr % min(SIZE[ty], 4):
                self.checked_load(ty, regs)
            else:
                self.load_mem(ty, regs, "t0")
            return

        if immediate:
            self.asm(f"\tla t0, {op}")
            if indirect:
                self.load_mem(ty, regs, "t0")
            else:
                self.asm(f"\tmv {regs[0]}, t0")
            return

        if op[0] in "_.":  # Memory variable
            self.asm(f"\tla t0, {op}")
            if indirect:
                self.asm("\tlw t0, 0(t0)")
                self.checked_load(ty, regs)
            else:
                self.load_mem(ty, regs, "t0")
            return

        # Temporary in the stack
        if indirect:
            self.pop("u32", ("t0",))
            self.checked_load(ty, regs)
        else:
            self.pop(ty, regs)

    def get_address(self, op: str) -> bool:
        """Puts in t0 the destination address of a store. Returns True if it is a checked one"""
        indirect = op[0] == "*"
        if indirect:
            op = op[1:]
        immediate = op[0] == "#"
        if immediate:
            op = op[1:]

        if op in self.low_labels:  # DIM ... AT address
            if indirect:
                raise NotImplementedError("zxv: pointer variable at a fixed address below 64K")
            return self.address(self.low_labels[op])

        if is_number(op):
            if self.address(int(op)) and indirect:
                raise NotImplementedError("zxv: pointer at a fixed address below 64K")
            if indirect:
                self.asm("\tlw t0, 0(t0)")
            return True

        if immediate or op[0] in "_.":
            self.asm(f"\tla t0, {op}")
            if indirect:
                self.asm("\tlw t0, 0(t0)")
            return indirect

        self.pop("u32", ("t0",))
        if indirect:
            self.asm("\tlw t0, 0(t0)")
        return True

    # -----------------------------------------------------------------
    # Emission
    # -----------------------------------------------------------------
    def emit(self, *, optimize: bool = True) -> list[str]:
        self.out = []
        for quad in self.MEMORY:
            instr = str(quad.instr)
            args = list(quad.args)
            m = RE_INSTR.match(instr)
            op, ty = (m.group("op"), m.group("t")) if m else (instr, None)
            if self.fast and op not in ("fparam", "call", "fpload"):
                raise NotImplementedError(f"zxv: '{instr}' between fparam and call")

            func = getattr(self, f"_{op}", None)
            if func is None:
                error(0, f"zxv backend: '{instr}' not implemented yet")
                continue

            self.asm(f"\t# {instr} {', '.join(args)}".replace("\n", " "))
            if op == "label" and args[0].startswith(".DATA."):  # The DATA lines
                self.datasec()
            elif op not in DATA_OPS:
                self.text()
            if ty is None:
                func(*args)
            else:
                func(ty, *args)

        return self.out

    @staticmethod
    def emit_prologue() -> list[str]:
        return []

    @staticmethod
    def emit_epilogue() -> list[str]:
        return []

    # -----------------------------------------------------------------
    # Arithmetic
    # -----------------------------------------------------------------
    def binary(self, ty: str, op1: str, op2: str) -> None:
        """Second operand into a1 (a2:a3), first one into a0 (a0:a1)"""
        self.get(op2, ty, self.regs(ty, "a2" if ty == "f" else "a1"))
        self.get(op1, ty, self.regs(ty))

    def int_op(self, ty: str, dest: str, op1: str, op2: str, instr: str) -> None:
        self.binary(ty, op1, op2)
        self.asm(f"\t{instr} a0, a0, a1")
        self.norm(ty, "a0")
        self.push(ty, ("a0",))

    def float_op(self, dest: str, op1: str, op2: str, func: str) -> None:
        self.binary("f", op1, op2)
        self.ccall(func)
        self.push("f", ("a0", "a1"))

    def _add(self, ty, dest, op1, op2):
        if ty == "str":
            return self.str_op(dest, op1, op2, "zxb_addstr", push=True)
        if ty == "f":
            return self.float_op(dest, op1, op2, "__adddf3")
        self.int_op(ty, dest, op1, op2, "add")

    def _sub(self, ty, dest, op1, op2):
        if ty == "f":
            return self.float_op(dest, op1, op2, "__subdf3")
        self.int_op(ty, dest, op1, op2, "sub")

    def _mul(self, ty, dest, op1, op2):
        if ty == "f":
            return self.float_op(dest, op1, op2, "__muldf3")
        if ty == "f16":
            self.binary(ty, op1, op2)
            self.asm("\tmul t0, a0, a1", "\tmulh t1, a0, a1", "\tsrli t0, t0, 16", "\tslli t1, t1, 16")
            self.asm("\tor a0, t0, t1")
            return self.push(ty, ("a0",))
        self.int_op(ty, dest, op1, op2, "mul")

    def _div(self, ty, dest, op1, op2):
        if ty == "f":
            return self.float_op(dest, op1, op2, "__divdf3")
        if ty == "f16":
            self.binary(ty, op1, op2)
            self.ccall("zxb_divf16")
            return self.push(ty, ("a0",))
        self.int_op(ty, dest, op1, op2, "div" if ty[0] == "i" else "divu")

    def _mod(self, ty, dest, op1, op2):
        if ty == "f":
            return self.float_op(dest, op1, op2, "zxb_modf")
        if ty == "f16":
            self.binary(ty, op1, op2)
            self.ccall("zxb_modf16")
            return self.push(ty, ("a0",))
        self.int_op(ty, dest, op1, op2, "rem" if ty[0] == "i" else "remu")

    def _pow(self, ty, dest, op1, op2):
        self.float_op(dest, op1, op2, "zxb_pow")

    def shift(self, ty, dest, op1, op2, instr):
        self.get(op2, "u8", ("a1",))
        self.get(op1, ty, ("a0",))
        self.asm(f"\t{instr} a0, a0, a1")
        self.norm(ty, "a0")
        self.push(ty, ("a0",))

    def _shl(self, ty, dest, op1, op2):
        self.shift(ty, dest, op1, op2, "sll")

    def _shr(self, ty, dest, op1, op2):
        self.shift(ty, dest, op1, op2, "sra" if ty[0] == "i" else "srl")

    def _band(self, ty, dest, op1, op2):
        self.int_op(ty, dest, op1, op2, "and")

    def _bor(self, ty, dest, op1, op2):
        self.int_op(ty, dest, op1, op2, "or")

    def _bxor(self, ty, dest, op1, op2):
        self.int_op(ty, dest, op1, op2, "xor")

    def _bnot(self, ty, dest, op1):
        self.get(op1, ty, ("a0",))
        self.asm("\tnot a0, a0")
        self.norm(ty, "a0")
        self.push(ty, ("a0",))

    def _neg(self, ty, dest, op1):
        self.get(op1, ty, self.regs(ty))
        if ty == "f":
            self.asm("\tbinvi a1, a1, 31")
        else:
            self.asm("\tneg a0, a0")
            self.norm(ty, "a0")
        self.push(ty, self.regs(ty))

    def _abs(self, ty, dest, op1):
        self.get(op1, ty, self.regs(ty))
        if ty == "f":
            self.asm("\tbclri a1, a1, 31")
        else:
            self.asm("\tneg t0, a0", "\tmax a0, a0, t0")
            self.norm(ty, "a0")
        self.push(ty, self.regs(ty))

    # -----------------------------------------------------------------
    # Comparisons and booleans (results are u8: 0 or 1)
    # -----------------------------------------------------------------
    def compare(self, ty, op1, op2, cond):
        if ty == "str":
            return self.str_op(None, op1, op2, "zxb_strcmp", cond=cond)

        if ty == "f":
            self.binary(ty, op1, op2)
            self.ccall("zxb_cmpf")  # -1, 0, 1 (2 if unordered)
            self.asm(
                *{
                    "lt": ["\tslti a0, a0, 0"],
                    "gt": ["\taddi a0, a0, -1", "\tseqz a0, a0"],
                    "le": ["\tslti a0, a0, 1"],
                    "ge": ["\tsltiu a0, a0, 2"],
                    "eq": ["\tseqz a0, a0"],
                    "ne": ["\tsnez a0, a0"],
                }[cond]
            )
            return self.push("u8", ("a0",))

        self.binary(ty, op1, op2)
        slt = "slt" if ty[0] in "if" else "sltu"
        self.asm(
            *{
                "lt": [f"\t{slt} a0, a0, a1"],
                "gt": [f"\t{slt} a0, a1, a0"],
                "le": [f"\t{slt} a0, a1, a0", "\txori a0, a0, 1"],
                "ge": [f"\t{slt} a0, a0, a1", "\txori a0, a0, 1"],
                "eq": ["\tsub a0, a0, a1", "\tseqz a0, a0"],
                "ne": ["\tsub a0, a0, a1", "\tsnez a0, a0"],
            }[cond]
        )
        self.push("u8", ("a0",))

    def _lt(self, ty, dest, op1, op2):
        self.compare(ty, op1, op2, "lt")

    def _gt(self, ty, dest, op1, op2):
        self.compare(ty, op1, op2, "gt")

    def _le(self, ty, dest, op1, op2):
        self.compare(ty, op1, op2, "le")

    def _ge(self, ty, dest, op1, op2):
        self.compare(ty, op1, op2, "ge")

    def _eq(self, ty, dest, op1, op2):
        self.compare(ty, op1, op2, "eq")

    def _ne(self, ty, dest, op1, op2):
        self.compare(ty, op1, op2, "ne")

    def truth(self, ty: str, regs: tuple[str, ...], dest: str) -> None:
        """dest = 1 if the value is not 0"""
        if ty == "f":
            self.asm(f"\tslli t1, {regs[1]}, 1", f"\tor t1, t1, {regs[0]}", f"\tsnez {dest}, t1")
        else:
            self.asm(f"\tsnez {dest}, {regs[0]}")

    def logic(self, ty, op1, op2, instr):
        r2 = self.regs(ty, "a2")
        self.get(op2, ty, r2)
        self.truth(ty, r2, "a2")
        self.asm("\taddi sp, sp, -4", "\tsw a2, 0(sp)")
        self.get(op1, ty, self.regs(ty))
        self.truth(ty, self.regs(ty), "a0")
        self.asm("\tlw a2, 0(sp)", "\taddi sp, sp, 4", f"\t{instr} a0, a0, a2")
        self.push("u8", ("a0",))

    def _and(self, ty, dest, op1, op2):
        self.logic(ty, op1, op2, "and")

    def _or(self, ty, dest, op1, op2):
        self.logic(ty, op1, op2, "or")

    def _xor(self, ty, dest, op1, op2):
        self.logic(ty, op1, op2, "xor")

    def _not(self, ty, dest, op1):
        self.get(op1, ty, self.regs(ty))
        self.truth(ty, self.regs(ty), "a0")
        self.asm("\txori a0, a0, 1")
        self.push("u8", ("a0",))

    # -----------------------------------------------------------------
    # Jumps
    # -----------------------------------------------------------------
    def _jump(self, label):
        self.asm(f"\tj {label}")

    def jcond(self, ty, op, label, branch):
        if ty == "str":
            self.get(op, "str", ("a0",))
            self.asm(f"\tli a1, {int(self.is_temp(op))}")
            self.ccall("zxb_lenstr")
        else:
            self.get(op, ty, self.regs(ty))
            if ty == "f":
                self.truth(ty, ("a0", "a1"), "a0")
        self.asm(f"\t{branch} a0, {label}")

    def _jzero(self, ty, op, label):
        self.jcond(ty, op, label, "beqz")

    def _jnzero(self, ty, op, label):
        self.jcond(ty, op, label, "bnez")

    def _jgezero(self, ty, op, label):
        if ty[0] == "u":
            self.get(op, ty, ("a0",))
            return self.asm(f"\tj {label}")
        if ty == "f":
            self.get(op, ty, ("a0", "a1"))
            self.asm("\tli a2, 0", "\tli a3, 0")
            self.ccall("zxb_cmpf")
            return self.asm(f"\tbgez a0, {label}")
        self.get(op, ty, ("a0",))
        self.asm(f"\tbgez a0, {label}")

    # -----------------------------------------------------------------
    # Memory
    # -----------------------------------------------------------------
    def _load(self, ty, dest, src):
        if ty == "str":
            self.get(src, "str", ("a0",))
            if not self.is_temp(src):
                self.ccall("zxb_loadstr")
            return self.push("str", ("a0",))
        self.get(src, ty, self.regs(ty))
        self.push(ty, self.regs(ty))

    def _store(self, ty, dest, value):
        if ty == "str":
            return self._storestr_at(dest, value)
        self.get(value, ty, self.regs(ty))
        if self.get_address(dest):
            self.checked_store(ty)
        else:
            self.store_mem(ty, self.regs(ty), "t0")

    def _storestr_at(self, dest, value):
        """Copies a string into a string variable: X is the variable, *X has its address"""
        self.get(value, "str", ("a1",))
        owned = self.is_temp(value)
        if dest[0] == "*":
            self.get(dest[1:], "u32", ("a0",))
        else:
            self.asm(f"\tla a0, {dest.lstrip('#')}")
        self.asm(f"\tli a2, {int(owned)}")
        self.ccall("zxb_storestr")

    def frame_offset(self, offset: str) -> tuple[bool, int]:
        indirect = offset[0] == "*"
        i = int(offset.lstrip("*"))
        if i >= 0:
            i += 8  # Saved s0 and return address
        return indirect, i

    def _pload(self, ty, dest, offset):
        indirect, i = self.frame_offset(offset)
        if indirect:
            self.asm(f"\tlw t0, {i}(s0)")
            self.load_mem(ty, self.regs(ty), "t0")
        else:
            self.load_mem(ty, self.regs(ty), "s0", i)
        if ty == "str" and dest[0] != "$":
            self.ccall("zxb_loadstr")
        self.push(ty, self.regs(ty))

    def _fpload(self, ty, dest, offset):
        indirect, i = self.frame_offset(offset)
        if indirect:
            self.asm(f"\tlw t0, {i}(s0)")
            self.load_mem(ty, self.regs(ty), "t0")
        else:
            self.load_mem(ty, self.regs(ty), "s0", i)
        if ty == "str" and dest[0] != "$":
            self.ccall("zxb_loadstr")
        self.fast_value(ty, dest, loaded=True)

    def _pstore(self, ty, offset, value):
        indirect, i = self.frame_offset(offset)
        if ty == "str":
            self.get(value, "str", ("a1",))
            if indirect:
                self.asm(f"\tlw a0, {i}(s0)")
            else:
                self.asm(f"\taddi a0, s0, {i}")
            self.asm(f"\tli a2, {int(self.is_temp(value))}")
            return self.ccall("zxb_storestr")
        self.get(value, ty, self.regs(ty))
        if indirect:
            self.asm(f"\tlw t0, {i}(s0)")
            self.store_mem(ty, self.regs(ty), "t0")
        else:
            self.store_mem(ty, self.regs(ty), "s0", i)

    def _paddr(self, offset, dest):
        indirect, i = self.frame_offset(offset)
        if indirect:
            self.asm(f"\tlw a0, {i}(s0)")
        else:
            self.asm(f"\taddi a0, s0, {i}")
        self.push("u32", ("a0",))

    # Arrays: the indexes are in the stack (first one on top); the runtime
    # computes the element address and how many bytes to pop.
    def array_addr(self, op: str, frame: bool = False) -> None:
        """Leaves the element address in a4"""
        if frame:
            indirect, i = self.frame_offset(op)
            if indirect:
                self.asm(f"\tlw a0, {i}(s0)")
            else:
                self.asm(f"\taddi a0, s0, {i}")
        else:
            indirect = op[0] == "*"
            op = op.lstrip("*")
            if op[0] in "_.#":
                self.asm(f"\tla a0, {op.lstrip('#')}")
            elif is_number(op):
                self.asm(f"\tli a0, {int(op)}")
            else:
                self.pop("u32", ("a0",))
            if indirect:
                self.asm("\tlw a0, 0(a0)")
        self.asm("\tmv a1, sp")
        self.ccall("zxb_array")
        self.asm("\tadd sp, sp, a1", "\tmv a4, a0")

    def aload(self, ty, op, frame):
        self.array_addr(op, frame)
        self.load_mem(ty, self.regs(ty), "a4")
        if ty == "str":
            self.ccall("zxb_loadstr")
        self.push(ty, self.regs(ty))

    def astore(self, ty, op, value, frame):
        self.array_addr(op, frame)
        if ty == "str":
            self.get(value, "str", ("a1",))
            self.asm("\tmv a0, a4", f"\tli a2, {int(self.is_temp(value))}")
            return self.ccall("zxb_storestr")
        self.get(value, ty, self.regs(ty))
        self.store_mem(ty, self.regs(ty), "a4")

    def _aaddr(self, dest, op):
        self.array_addr(op)
        self.push("u32", ("a4",))

    def _paaddr(self, dest, op):
        self.array_addr(op, frame=True)
        self.push("u32", ("a4",))

    def _aload(self, ty, dest, op):
        self.aload(ty, op, False)

    def _paload(self, ty, dest, op):
        self.aload(ty, op, True)

    def _astore(self, ty, op, value):
        self.astore(ty, op, value, False)

    def _pastore(self, ty, op, value):
        self.astore(ty, op, value, True)

    def _cast(self, dest, tfrom, tto, op):
        tfrom = "u8" if tfrom == "bool" else tfrom
        self.get(op, tfrom, self.regs(tfrom))
        if tto == "bool":
            self.truth(tfrom, self.regs(tfrom), "a0")
            return self.push("u8", ("a0",))

        if tfrom == tto:
            pass
        elif tto == "f":
            if tfrom == "f16":
                self.ccall("zxb_f16tof")
            else:
                self.ccall("__floatunsidf" if tfrom == "u32" else "__floatsidf")
        elif tfrom == "f":
            self.ccall("zxb_ftof16" if tto == "f16" else "zxb_ftoi")
            self.norm(tto, "a0")
        elif tto == "f16":
            self.asm("\tslli a0, a0, 16")
        elif tfrom == "f16":
            self.asm("\tsrai a0, a0, 16")
            self.norm(tto, "a0")
        else:
            self.norm(tto, "a0")
        self.push(tto, self.regs(tto))

    # -----------------------------------------------------------------
    # Strings
    # -----------------------------------------------------------------
    def str_op(self, dest, op1, op2, func, *, push=False, cond=None):
        """Calls func(a, b, flags): flags bit 0 (1) if a must be freed, bit 1 (2) if b"""
        self.get(op2, "str", ("a1",))
        self.get(op1, "str", ("a0",))
        self.asm(f"\tli a2, {int(self.is_temp(op1)) | 2 * int(self.is_temp(op2))}")
        self.ccall(func)
        if cond is not None:
            self.asm(
                *{
                    "lt": ["\tslti a0, a0, 0"],
                    "gt": ["\tsgtz a0, a0"],
                    "le": ["\tslti a0, a0, 1"],
                    "ge": ["\tnot a0, a0", "\tsrli a0, a0, 31"],
                    "eq": ["\tseqz a0, a0"],
                    "ne": ["\tsnez a0, a0"],
                }[cond]
            )
            return self.push("u8", ("a0",))
        self.push("str", ("a0",))

    def _len(self, ty, dest, op):
        self.get(op, "str", ("a0",))
        self.asm(f"\tli a1, {int(self.is_temp(op))}")
        self.ccall("zxb_lenstr")
        self.push("u16", ("a0",))

    # -----------------------------------------------------------------
    # Parameters, calls and functions
    # -----------------------------------------------------------------
    def _param(self, ty, op):
        if ty == "str":
            self.get(op, "str", ("a0",))
            if op[0] in "#_":  # Must be duplicated
                self.ccall("zxb_loadstr")
            return self.push("str", ("a0",))
        self.get(op, ty, self.regs(ty))
        self.push(ty, self.regs(ty))

    def next_regs(self, ty: str) -> tuple[str, ...]:
        """Argument registers for the next parameter (ilp32: doubles in an even pair)"""
        if ty == "f":
            self.fast_regs += self.fast_regs % 2
        n = 2 if ty in ("f", "str") else 1
        regs = tuple("a%i" % i for i in range(self.fast_regs, self.fast_regs + n))
        self.fast_regs += n
        if self.fast_regs > 8:
            raise NotImplementedError("zxv: too many parameters for a runtime call")
        return regs

    def fast_value(self, ty, op, loaded=False):
        regs = self.next_regs(ty)
        if not loaded:
            self.get(op, ty, regs[:1] if ty == "str" else regs)
        elif regs[0] != "a0":
            raise NotImplementedError("zxv: fpload must be the first fparam")
        if ty == "str":  # Second register: 1 if it must be freed
            self.asm(f"\tli {regs[1]}, {int(self.is_temp(op))}")
        self.fast.append((ty, op))

    def _fparam(self, ty, op):
        self.fast_value(ty, op)

    def _call(self, label, size):
        size = int(size)
        if label.startswith(CORE):
            self.runtime_call(label[len(CORE) :].lstrip("_"))
        else:
            ret = self.new_label()
            self.asm("\taddi sp, sp, -4", f"\tla t0, {ret}", "\tsw t0, 0(sp)", f"\tj {label}", f"{ret}:")
        self.fast = []
        self.fast_regs = 0
        if size == 8:
            self.push("f", ("a0", "a1"))
        elif size:
            self.push("u32", ("a0",))

    def runtime_call(self, name):
        if name in ("ON_GOTO", "ON_GOSUB"):
            return self.on_goto(name)
        if name == "CHR":
            return self.chr()
        if name == "CHECK_BREAK":  # It keeps every register
            return self.asm("\tcall __zxb_chkbreak")

        stack = RUNTIME_SIGNATURES.get(name)
        if stack is None and name not in RUNTIME_SIMPLE:
            error(0, f"zxv: '{name}' is not supported yet")
            return
        for ty in stack or ():
            self.pop(ty, self.next_regs("u32" if ty == "str" else ty))
        self.ccall(c_name(CORE + name))

    def on_goto(self, name):
        """ON x GO TO / GO SUB: the runtime returns the address (0 if out of range)"""
        self.pop("u32", self.next_regs("u32"))  # The table
        self.ccall("zxb_on_goto")
        skip = self.new_label()
        self.asm(f"\tbeqz a0, {skip}")
        if name == "ON_GOSUB":
            self.asm("\taddi sp, sp, -4", f"\tla t0, {skip}", "\tsw t0, 0(sp)")
        self.asm("\tjr a0", f"{skip}:")

    def chr(self):
        """CHR$ a, b, c...: the codes are in the stack and the count is the fparam"""
        ty, count = self.fast[0]
        if not is_number(count):
            raise NotImplementedError("zxv: CHR$ with a variable number of codes")
        self.asm("\tmv a1, sp")
        self.ccall("zxb_CHR")
        self.asm(f"\taddi sp, sp, {4 * int(count)}")

    def _enter(self, size):
        if size == "__fastcall__":
            return
        self.asm("\taddi sp, sp, -4", "\tsw s0, 0(sp)", "\tmv s0, sp")
        size = -(-int(size) // 4) * 4
        if size:
            loop = self.new_label()
            self.asm(f"\taddi sp, sp, -{size}", "\tmv t0, sp", f"{loop}:", "\tsw zero, 0(t0)")
            self.asm("\taddi t0, t0, 4", f"\tbltu t0, s0, {loop}")

    def _leave(self, size):
        if size == "__fastcall__":
            self.asm("\tlw t0, 0(sp)", "\taddi sp, sp, 4", "\tjr t0")
            return
        self.asm("\tmv sp, s0", "\tlw s0, 0(sp)", "\tlw t0, 4(sp)", f"\taddi sp, sp, {8 + int(size)}", "\tjr t0")

    def _ret(self, *args):
        if len(args) == 1:  # ret LABEL (no value)
            return self.asm(f"\tj {args[0]}")
        ty, op, label = args
        self.get(op, ty, self.regs(ty))
        if ty == "str" and not self.is_temp(op):
            self.ccall("zxb_loadstr")
        self.asm(f"\tj {label}")

    def _end(self, code):
        self.get(code, "u32", ("a0",))
        self.asm(f"\tj {self.end_label}")

    def _label(self, label):
        self.asm(f"{label}:")
        if label == ".DATA.__DATA__0":  # The first DATA, for READ
            self.asm("\t.globl __zxb_data0", "\t.set __zxb_data0, .DATA.__DATA__0")

    def _deflabel(self, label, value):
        self.asm(f"\t.set {label}, {value}")

    def _inline(self, code):
        # The translator's own comments are Z80 style (';' starts a statement in GNU as)
        self.asm(*("#" + x.lstrip(";") if x.lstrip().startswith(";") else x for x in code.split("\n")))

    def _in(self, port):
        self.get(port, "u16", ("a0",))
        self.ccall("zxb_IN")
        self.push("u8", ("a0",))

    def _out(self, port, value):
        self.get(value, "u8", ("a1",))
        self.get(port, "u16", ("a0",))
        self.ccall("zxb_OUT")

    def _exchg(self):
        """Keeps the result of a function (a0, a1) while its locals are freed: they come in pairs"""
        self.exchg_saved = not getattr(self, "exchg_saved", False)
        if self.exchg_saved:
            self.push("f", ("a0", "a1"))
        else:
            self.pop("f", ("a0", "a1"))

    def _nop(self):
        pass

    def _memcopy(self, dest, src, size):
        """Copies size bytes from the address src to dest (a = b with arrays)"""
        self.get(size, "u32", ("a2",))
        self.get(src, "u32", ("a0",))
        self.get(dest, "u32", ("a1",))
        self.ccall("zxb_lib_memmove")

    def _org(self, value):
        pass  # The program is relocated by the linker

    # -----------------------------------------------------------------
    # Data
    # -----------------------------------------------------------------
    def data_label(self, label):
        self.datasec()
        self.asm("\t.balign 4", f"{label}:")

    def _var(self, label, size):
        self.data_label(label)
        self.asm(f"\t.zero {int(size)}")

    def _vard(self, label, values):
        self.data_label(label)
        self.vard_values(eval(values))

    def vard_values(self, values):
        for x in values:
            if x[0] == "#":
                self.asm(f"\t.half {x[2:]}" if x[1] == "#" else f"\t.byte {x[1:]}")
            elif len(x) <= 2:
                self.asm(f"\t.byte 0x{x}")
            else:
                self.asm(f"\t.half 0x{x}")

    def data_values(self, ty, values):
        # Z80 style hexadecimal (81h)
        values = [f"0x{x[:-1]}" if re.match(r"^[0-9][0-9A-Fa-f]*[hH]$", str(x)) else x for x in values]
        if ty in ("i8", "u8"):
            self.asm(*(f"\t.byte {x}" for x in values))
        elif ty in ("i16", "u16"):
            self.asm(*(f"\t.half {x}" for x in values))
        elif ty in ("i32", "u32"):
            self.asm(*(f"\t.word {x}" for x in values))
        elif ty == "f16":
            self.asm(*(f"\t.word {int(float(x) * 65536) & 0xFFFFFFFF}" for x in values))
        elif ty == "f":
            for x in values:
                lo, hi = float_words(float(x))
                self.asm(f"\t.word {lo}, {hi}")
        elif ty == "str":
            self.asm(*('\t.ascii "%s"' % x.replace("\\", "\\\\").replace('"', '\\"') for x in values))
        else:
            raise NotImplementedError(f"zxv: data of type {ty}")

    def _varx(self, label, ty, values):
        self.data_label(label)
        self.data_values(ty, eval(values))

    def _data(self, ty, values):
        self.datasec()
        self.data_values(ty, eval(values))

    def _larrd(self, offset, table, size, values, bounds):
        """A local array: its variable (in the frame) gets the dimensions table, the
        data (allocated, with the initial values if any) and the LBOUND and UBOUND tables"""
        label = self.new_label()
        out, self.out = self.out, self.data
        self.asm("\t.data", "\t.balign 4", f"{label}:")
        self.vard_values(eval(table))
        values = eval(values)
        if values:
            init = self.new_label()
            self.asm("\t.balign 4", f"{init}:")
            self.vard_values(values)
        self.out = out
        lbound, ubound = eval(bounds) or ["0", "0"]
        self.asm(f"\taddi a0, s0, {-int(offset)}", f"\tla a1, {label}", f"\tli a2, {int(size)}")
        self.asm(f"\tla a3, {init}" if values else "\tli a3, 0")
        self.asm(f"\tla a4, {lbound}" if lbound != "0" else "\tli a4, 0")
        self.asm(f"\tla a5, {ubound}" if ubound != "0" else "\tli a5, 0")
        self.ccall("zxb_local_array")

    def lvar(self, offset, nbytes, label):
        """Copies the initial value of a local variable"""
        loop = self.new_label()
        self.asm(f"\tla t0, {label}", f"\taddi t1, s0, {-int(offset)}", f"\tli t2, {nbytes}", f"{loop}:")
        self.asm("\tlbu t3, 0(t0)", "\tsb t3, 0(t1)", "\taddi t0, t0, 1", "\taddi t1, t1, 1")
        self.asm("\taddi t2, t2, -1", f"\tbnez t2, {loop}")

    def _lvard(self, offset, values):
        label = self.new_label()
        values = eval(values)
        out, self.out = self.out, self.data
        self.asm("\t.data", "\t.balign 4", f"{label}:")
        self.vard_values(values)
        self.out = out
        nbytes = sum(1 if x[0] == "#" and x[1] != "#" or len(x) <= 2 else 2 for x in values)
        self.lvar(offset, nbytes, label)

    def _lvarx(self, offset, ty, values):
        label = self.new_label()
        values = eval(values)
        out, self.out = self.out, self.data
        self.asm("\t.data", "\t.balign 4", f"{label}:")
        self.data_values(ty, values)
        self.out = out
        self.lvar(offset, SIZE[ty] * len(values), label)


DATA_OPS = {"var", "vard", "varx", "data", "deflabel"}
