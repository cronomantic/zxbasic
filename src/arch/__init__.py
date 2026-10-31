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


def add_arch_arguments(target_arch: str, parser) -> None:
    """Command line options of an arch: the add_arguments(parser) function of its package, if it has one"""
    module = importlib.import_module(f".{target_arch}", "src.arch")
    if hasattr(module, "add_arguments"):
        module.add_arguments(parser)


def set_arch_options(target_arch: str, options) -> None:
    """Lets an arch take its command line options (the parsed ones): the set_options(options) function of its
    package, if it has one"""
    module = importlib.import_module(f".{target_arch}", "src.arch")
    if hasattr(module, "set_options"):
        module.set_options(options)


set_target_arch(AVAILABLE_ARCHITECTURES[0])
