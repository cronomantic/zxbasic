# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""zxv target: a FASTCALL SUB/FUNCTION whose BASIC body reads a parameter gets a warning (its
parameters arrive in registers, not in the frame the BASIC reads); ASM bodies and functions that
do not read them do not"""

import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "..", "..")

PROGRAM = """\
FUNCTION FASTCALL fi(a AS UBYTE) AS UBYTE
    RETURN a + 1
END FUNCTION
SUB FASTCALL ss(s AS STRING)
    PRINT s
END SUB
FUNCTION FASTCALL fasm(a AS UBYTE) AS UBYTE
    ASM
    addi a0, a0, 1
    END ASM
END FUNCTION
FUNCTION FASTCALL fnone() AS UBYTE
    RETURN 5
END FUNCTION
FUNCTION normal(a AS UBYTE) AS UBYTE
    RETURN a + 1
END FUNCTION
FUNCTION FASTCALL fact(n AS UBYTE) AS ULONG
    ASM
    li a0, 1
    END ASM
END FUNCTION
ss("x")
PRINT fi(1); fasm(3); fnone(); normal(4); fact(3)
"""


def compile_warnings(tmp_path, arch: str) -> str:
    src = tmp_path / "fc.bas"
    src.write_text(PROGRAM)
    r = subprocess.run(
        [sys.executable, os.path.join(ROOT, "zxbc.py"), "--arch", arch, "-f", "asm", str(src), "-o",
         str(tmp_path / "fc.asm")],
        capture_output=True, text=True, timeout=120,
    )
    assert r.returncode == 0, r.stdout + r.stderr
    return r.stdout + r.stderr


def test_warns_when_basic_reads_a_fastcall_parameter(tmp_path):
    out = compile_warnings(tmp_path, "zxv")
    assert "FUNCTION 'fi' is FASTCALL" in out
    assert "SUB 'ss' is FASTCALL" in out
    for name in ("fasm", "fnone", "normal", "fact"):
        assert f"'{name}' is FASTCALL" not in out


def test_z80_targets_unchanged(tmp_path):
    assert "is FASTCALL: its parameters arrive in registers" not in compile_warnings(tmp_path, "zx48k")
