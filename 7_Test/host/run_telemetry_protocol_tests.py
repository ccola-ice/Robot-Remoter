"""Compile the real, standalone RT codec; no MCU or radio stubs are used."""
from app_test_paths import app_include_args
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"
with tempfile.TemporaryDirectory(prefix="remoter-telemetry-") as directory:
    executable = Path(directory) / "telemetry_test.exe"
    subprocess.run(
        [compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
         *app_include_args(),
         str(root / "7_Test/host/telemetry_protocol_test.c"),
         "-o", str(executable)], check=True
    )
    subprocess.run([str(executable)], check=True)
