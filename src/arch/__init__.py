# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

import importlib
from types import ModuleType

from src.api import global_
from src.api.constants import TYPE
from src.api.debug import __DEBUG__

__all__ = (
    "zx48k",
    "zxnext",
    "zx81sd",
    "zxv",
)

AVAILABLE_ARCHITECTURES = __all__
target: ModuleType


def set_target_arch(target_arch: str):
    global target
    assert target_arch in AVAILABLE_ARCHITECTURES, f"Invalid target architecture '{target_arch}'"
    target = importlib.import_module(f".{target_arch}", "src.arch")
    TYPE.reset_type_sizes()
    global_.LOCAL_ALIGN = 1
    global_.NULL_STRING_ASM = "\nld hl, 0\n"
    global_.ASM_RETURNS_VALUE = False
    if hasattr(target, "setup"):  # Arch dependent sizes, alignments...
        target.setup()
    __DEBUG__(f"Target architecture set to {target_arch}")


set_target_arch(AVAILABLE_ARCHITECTURES[0])
