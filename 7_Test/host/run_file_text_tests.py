"""用真实 FatFs 字符映射验证文本分页，文件读写由内存模拟。"""
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
        "-I", str(root / "1_App"), "-I", str(root / "3_Protocol/FatFs"),
        str(root / "1_App/file_text.c"),
        str(root / "3_Protocol/FatFs/unicode.c"),
        str(root / "7_Test/host/file_text_test.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True, timeout=30)
