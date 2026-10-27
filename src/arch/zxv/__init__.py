# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""ZX-V: a 32-bit RISC-V machine with a ZX Spectrum style BASIC.

Pointers are 32 bits and floats are IEEE doubles. Addresses below 65536
are emulated by the runtime as the ZX Spectrum memory map (screen,
attributes and system variables), as the ZX-V BASIC does.
"""

import src.api.global_
from src.api.constants import TYPE
from src.arch.z80 import beep
from src.arch.zxv import backend
from src.arch.zxv.driver import finish
from src.arch.zxv.translator import FunctionTranslator, Translator, VarTranslator

__all__ = (
    "FunctionTranslator",
    "Translator",
    "VarTranslator",
    "backend",
    "beep",
    "finish",
    "setup",
)


def setup() -> None:
    """Arch initialization (called each time this arch is selected)"""
    TYPE.set_type_size(TYPE.float, 8)  # IEEE double
    TYPE.set_type_size(TYPE.string, 4)  # 32-bit pointer
    src.api.global_.PARAM_ALIGN = 4  # Every parameter takes a word (doubles, two)
    src.api.global_.LOCAL_ALIGN = 4  # RISC-V does not allow misaligned accesses
    src.api.global_.BOUND_TYPE = TYPE.uinteger
    src.api.global_.SIZE_TYPE = TYPE.ubyte
    src.api.global_.PTR_TYPE = TYPE.ulong
    src.api.global_.STR_INDEX_TYPE = TYPE.uinteger
    src.api.global_.MIN_STRSLICE_IDX = 0
    src.api.global_.MAX_STRSLICE_IDX = 65534
    src.api.global_.NULL_STRING_ASM = "\nli a0, 0\n"
    src.api.global_.ASM_RETURNS_VALUE = True  # The result is in a0


backend.Backend()
