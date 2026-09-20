"""Compile and execute the production gui_theme.h with a deterministic glyph source."""
from pathlib import Path
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"
with tempfile.TemporaryDirectory(prefix="remoter-ui-chinese-") as temporary:
    executable = Path(temporary) / "ui-chinese-test.exe"
    subprocess.run([
        compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
        "-Wno-unused-function", "-I", str(repo / "1_App"),
        str(Path(__file__).with_name("ui_chinese_test.c")), "-o", str(executable)
    ], check=True)
    subprocess.run([str(executable)], check=True)
