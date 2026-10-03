"""用真实 FatFs 字符映射验证文本分页，文件读写由内存模拟。"""
from app_test_paths import app_include_args
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"
with tempfile.TemporaryDirectory(prefix="remoter-file-text-") as directory:
    executable = Path(directory) / "file-text-test.exe"
    subprocess.run([
        compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
        *app_include_args(), "-I", str(root / "3_Protocol/FatFs"),
        str(root / "1_App/files/text_reader.c"),
        str(root / "3_Protocol/FatFs/unicode.c"),
        str(root / "7_Test/host/file_text_test.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True, timeout=30)
