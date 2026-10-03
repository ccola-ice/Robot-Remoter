"""Compile and execute the production ui_theme.h with a deterministic glyph source."""
from app_test_paths import app_include_args
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
        "-Wno-unused-function", *app_include_args(),
        str(Path(__file__).with_name("ui_chinese_test.c")), "-o", str(executable)
    ], check=True)
    subprocess.run([str(executable)], check=True)
