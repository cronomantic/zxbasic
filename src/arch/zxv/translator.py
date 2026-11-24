# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""The Z80 translators, with what is different in ZX-V"""

import struct

import src.api.global_ as gl
from src.api import errmsg
from src.api.config import OPTIONS
from src.api.constants import CLASS, CONVENTION, SCOPE, TYPE
from src.arch.z80.backend.runtime import Labels as RuntimeLabel
from src.arch.z80.visitor.function_translator import FunctionTranslator as Z80FunctionTranslator
from src.arch.z80.visitor.translator import Translator as Z80Translator
from src.arch.z80.visitor.var_translator import VarTranslator as Z80VarTranslator
from src.arch.zxv import banks

__all__ = "FunctionTranslator", "Translator", "VarTranslator"


class ZxvTranslation:
    """What changes from the Z80 translators"""

    BYTE_PARAM_OFFSET = 0  # Bytes are at the start of their parameter word

    @staticmethod
    def float_bytes(value) -> list[str]:
        """Floats are IEEE doubles, little-endian"""
        return ["%02X" % x for x in struct.pack("<d", float(value))]

    def visit_BEEP(self, node):
        # No Z80 timings for constant values: the runtime always gets duration and pitch
        yield node.children[1]
        self.ic_param(TYPE.float, node.children[1].t)
        yield node.children[0]
        self.ic_fparam(TYPE.float, node.children[0].t)
        self.runtime_call(RuntimeLabel.BEEP, 0)

    def visit_BLOCK(self, node):
        # With --enable-break, each statement keeps its line in zxb_line before it runs, for
        # the error reports (CHKBREAK comes after the line, so it only has the previous one)
        line = None
        for child in node.children:
            lineno = getattr(child, "lineno", 0)
            if OPTIONS.enable_break and child.token not in ("BLOCK", "CHKBREAK", "LABEL") and lineno:
                if lineno != line:
                    self.ic_inline(f"\tli t0, {lineno}\n\tla t1, zxb_line\n\tsw t0, 0(t1)")
                line = lineno
            elif child.token == "LABEL":
                line = None  # A jump may land here
            yield self.visit(child)

    def visit_CHKBREAK(self, node):
        # With --enable-break: Esc (BREAK) stops the program. No line parameter (zxb_line has
        # it), so a0 and a1 keep the result a function's ASM block may have left there
        if self.PREV_TOKEN != node.token:
            self.runtime_call(RuntimeLabel.CHECK_BREAK, 0)

    def emit_jump_tables(self):
        # Number of entries and 32-bit addresses
        for table_ in self.JUMP_TABLES:
            self.ic_varx(table_.label, gl.PTR_TYPE, [str(len(table_.addresses))] + [x.mangled for x in table_.addresses])


class Translator(ZxvTranslation, Z80Translator):
    pass


def _reads_parameter(body) -> int:
    """The line of the first BASIC statement of body that uses a parameter of its function, or 0"""
    stack = list(body)
    seen = set()
    while stack:
        n = stack.pop()
        # The tree has cycles (RETURN points to its function, a call to the function it calls):
        # each node once, and not into other functions
        if n is None or id(n) in seen or getattr(n, "token", None) in ("ASM", "FUNCTION", "FUNCDECL", "SUB"):
            continue
        seen.add(id(n))
        if getattr(n, "scope", None) == SCOPE.parameter and getattr(n, "token", None) in ("VAR", "VARARRAY", "ARRAY"):
            return getattr(n, "lineno", 0) or 1
        stack.extend(getattr(n, "children", ()) or ())
    return 0


class FunctionTranslator(ZxvTranslation, Z80FunctionTranslator):
    def visit_FUNCTION(self, node):
        # FASTCALL with a BASIC body that reads a parameter: the parameter arrives in a0 (a0 and a1
        # for a Float), not in the stack frame the BASIC reads, so it gets garbage, as on the Z80
        # (where the compiler says nothing). Only on zxv, a warning
        if node.convention == CONVENTION.fastcall:
            line = _reads_parameter(node.ref.body)
            if line:
                kind = "SUB" if node.class_ == CLASS.sub else "FUNCTION"
                errmsg.warning(
                    line,
                    f"{kind} '{node.name}' is FASTCALL: its parameters arrive in registers (a0, a1), "
                    "so BASIC code that reads them gets wrong values; read them in ASM or drop FASTCALL",
                    fname=getattr(node, "filename", None),
                )

        # A function of a code bank (zxbc --code-bank, banks.py) goes in the section of its bank
        from src.arch.zxv import driver

        bank = driver.code_banks().of_function(node)
        if bank:
            self.ic_inline(banks.begin(*bank))
        yield from super().visit_FUNCTION(node)
        if bank:
            self.ic_inline(banks.END)


class VarTranslator(ZxvTranslation, Z80VarTranslator):
    TRANSLATOR = Translator
