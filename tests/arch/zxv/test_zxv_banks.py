# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""zxbc --code-bank for the zxv target (ZX-V D109): the option, the assembler of the banks and the .zxv
(tests/zxv/bancos.bas and bancosuso.bas run them)"""

import struct
import zlib

import pytest

from src.arch.zxv import banks, zxvfile
from src.zxbc import args_parser


def test_option_only_for_zxv():
    options = args_parser.parser("zxv").parse_args(
        ["--arch", "zxv", "--code-bank", "uno=a.bas", "--code-bank", "dos@1=b.bas", "x.bas"]
    )
    assert options.code_bank == ["uno=a.bas", "dos@1=b.bas"]
    assert args_parser.parser("zxv").parse_args(["--arch", "zxv", "x.bas"]).code_bank == []
    with pytest.raises(SystemExit):
        args_parser.parser("zx48k").parse_args(["--code-bank", "uno=a.bas", "x.bas"])


def test_parse():
    assert banks.parse("uno=a.bas") == ("uno", 0, "a.bas")
    assert banks.parse("dos@7=sub/b.bas") == ("dos", 7, "sub/b.bas")
    for bad in ("uno", "uno=", "uno@8=a.bas", "uno@x=a.bas", "a-b=a.bas", "x" * 16 + "=a.bas", "=a.bas"):
        with pytest.raises(banks.BankError):
            banks.parse(bad)


def test_files(tmp_path):
    a = tmp_path / "a.bas"
    a.write_text("")
    b = tmp_path / "b.bas"
    b.write_text("")
    bk = banks.Banks([f"uno={a}", f"dos@2={b}"])
    assert bk.by_file[banks.norm(str(a))] == ("uno", 0)
    assert bk.by_file[banks.norm(str(b))] == ("dos", 2)
    with pytest.raises(banks.BankError):  # The same name twice
        banks.Banks([f"uno={a}", f"uno={b}"])
    with pytest.raises(banks.BankError):  # The same file in two banks
        banks.Banks([f"uno={a}", f"dos={a}"])
    with pytest.raises(banks.BankError):  # A file that is not there
        banks.Banks([f"uno={tmp_path / 'no.bas'}"])


CODE = [
    "_main_code:",
    "\tj _f",
    "\tj _g",
    banks.begin("uno", 0),
    "_f:",
    "\tj _g",  # The same bank: straight to it
    "\tj _h",  # Another bank: through the bridge
    "\tj _f",  # Itself (recursion)
    "_f__leave:",
    banks.END,
    banks.begin("uno", 0),
    "_g:",
    "\tnop",
    banks.END,
    banks.begin("dos", 1),
    "_h:",
    "\tj _f",
    banks.END,
]


def test_rewrite():
    code, used, funcs = banks.rewrite(CODE)
    assert used == [("uno", 0), ("dos", 1)]
    assert funcs == {"_f": "uno", "_g": "uno", "_h": "dos"}
    assert code[1:3] == ["\tj _f", "\tj _g"]  # The resident part: through the bridges
    assert "__zxvbank_real_f:" in code and "__zxvbank_real_g:" in code and "__zxvbank_real_h:" in code
    i = code.index("__zxvbank_real_f:")
    assert code[i + 1 : i + 4] == ["\tj __zxvbank_real_g", "\tj _h", "\tj __zxvbank_real_f"]
    assert "_f__leave:" in code  # Only the first label of the function is renamed
    j = code.index("__zxvbank_real_h:")
    assert code[j + 1] == "\tj _f"


def test_bridges():
    _, used, funcs = banks.rewrite(CODE)
    asm = banks.bridges(used, funcs)
    for label, bank in (("_f", 0), ("_g", 0), ("_h", 1)):
        assert f"{label}:\n\tli t5, {bank}\n\tla t6, __zxvbank_real{label}\n\tj __zxvbasic_call" in asm
    assert "__zxvbank_nbanks: .word 2" in asm
    assert "__zxvbank_nregions: .word 2" in asm
    assert "_novlys: .word 2" in asm
    assert f"__zxvbasic_bstack: .space {banks.NEST_MAX * banks.SLOT}" in asm


def test_linker_script():
    ld = banks.linker_script("SECTIONS {\n/* ZXVBANKS */\n}", [("uno", 0), ("dos", 0), ("tres", 2)])
    assert "OVERLAY ALIGN(16) : NOCROSSREFS AT (0x80000000)" in ld
    assert "OVERLAY ALIGN(16) : NOCROSSREFS AT (0x82000000)" in ld
    assert "*(.zxvbank.0.uno)" in ld and "*(.zxvbank.0.dos)" in ld and "*(.zxvbank.2.tres)" in ld
    assert "__zxvbank_region0 = ABSOLUTE(ADDR(.zxvbank.0.uno));" in ld
    assert "__zxvbank_lma2 = LOADADDR(.zxvbank.2.tres);" in ld
    with pytest.raises(banks.BankError):
        banks.linker_script("SECTIONS {}", [("uno", 0)])


def test_bank_data():
    body = bytes(range(64)) * 8
    out = zxvfile.bank_data(1000, 0x1234, 900, [("uno", 0, 0x20040000, body), ("tres", 2, 0x20041000, b"\1\2")])
    assert out[-16:-8] == b"ZXVBANK1"
    index, length = struct.unpack("<II", out[-8:])
    index -= 1000  # out starts at 1000 in the file
    count, crc, size, entries_crc = struct.unpack_from("<IIII", out, index)
    assert (count, crc, size) == (2, 0x1234, 900)
    entries = out[index + 16 : index + 16 + 48 * count]
    assert zlib.crc32(entries) & 0xFFFFFFFF == entries_crc
    name, kind, method, region, _, offset, stored, real, bcrc, addr = struct.unpack_from("<16sBBBBIIIII", entries)
    assert (name.rstrip(b"\0"), kind, method, region, real, addr) == (b"uno", 1, 1, 0, len(body), 0x20040000)
    data = out[offset - 1000 : offset - 1000 + stored]
    assert zlib.decompress(data, -15) == body and zlib.crc32(body) & 0xFFFFFFFF == bcrc
    name, kind, method, region, *_ = struct.unpack_from("<16sBBBB", entries, 48)
    assert (name.rstrip(b"\0"), method, region) == (b"tres", 0, 2)  # Two bytes: stored as they are
    assert zxvfile.bank_data(0, 0, 0, []) == b""
