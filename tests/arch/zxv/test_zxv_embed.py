# --------------------------------------------------------------------
# SPDX-License-Identifier: AGPL-3.0-or-later
# © Copyright 2008-2024 José Manuel Rodríguez de la Rosa and contributors.
# See the file CONTRIBUTORS.md for copyright details.
# See https://www.gnu.org/licenses/agpl-3.0.html for details.
# --------------------------------------------------------------------

"""zxbc --embed for the zxv target: the option and the table of files (tests/zxv/embed.bas runs it)"""

import pytest

from src.api import global_ as gl
from src.arch.zxv import driver
from src.zxbc import args_parser


@pytest.fixture(autouse=True)
def no_errors():
    gl.has_errors = 0
    yield
    gl.has_errors = 0


def test_option_only_for_zxv():
    options = args_parser.parser("zxv").parse_args(
        ["--arch", "zxv", "--embed", "a.pt3", "--embed", "b.afb", "x.bas"]
    )
    assert options.embed == ["a.pt3", "b.afb"]
    assert args_parser.parser("zxv").parse_args(["--arch", "zxv", "x.bas"]).embed == []
    with pytest.raises(SystemExit):
        args_parser.parser("zx48k").parse_args(["--embed", "a.pt3", "x.bas"])


def test_table(tmp_path):
    song = tmp_path / "Song.pt3"
    song.write_bytes(b"\1\2\3")
    level = tmp_path / "sub" / "level.bin"
    level.parent.mkdir()
    level.write_bytes(b"")
    asm = driver.embed_table([str(song), str(level)])
    assert ".globl zxb_embed_table" in asm
    assert "\t.word 2" in asm
    assert '\t.asciz "Song.pt3"' in asm  # without the folder, as it was written
    assert '\t.asciz "level.bin"' in asm
    assert f'\t.incbin "{str(level).replace(chr(92), "/")}"' in asm


@pytest.mark.parametrize("name", ["a:b.bin", "song.", "trailing ", "ñ.pt3", "x" * 64])
def test_bad_names(tmp_path, name):
    try:
        f = tmp_path / name
        f.write_bytes(b"x")
    except OSError:
        pytest.skip("this file system does not allow that name")
    assert driver.embed_table([str(f)]) is None
    assert gl.has_errors


def test_missing_and_repeated(tmp_path):
    assert driver.embed_table([str(tmp_path / "none.bin")]) is None
    a = tmp_path / "a" / "music.pt3"
    b = tmp_path / "b" / "MUSIC.PT3"
    for f in (a, b):
        f.parent.mkdir()
        f.write_bytes(b"x")
    assert driver.embed_table([str(a), str(b)]) is None
