"""验证目录浏览后端，使用真实 FatFs 头文件并模拟只读目录接口。"""
from app_test_paths import app_include_args
from pathlib import Path
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"

with tempfile.TemporaryDirectory(prefix="remoter-file-browser-") as folder:
    executable = Path(folder) / "test.exe"
    subprocess.run([
        compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
        *app_include_args(),
        "-I" + str(root / "3_Protocol/FatFs"),
        "-I" + str(root / "5_SystemDrivers"),
        str(host / "file_browser_test.c"), str(root / "1_App/files/file_browser.c"),
        "-o", str(executable)
    ], check=True)
    subprocess.run([str(executable)], check=True)
