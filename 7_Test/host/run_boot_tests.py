from pathlib import Path
import subprocess, tempfile, sys

host = Path(__file__).resolve().parent
app = host.parents[1] / '1_App'
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
with tempfile.TemporaryDirectory(prefix='remoter-boot-') as folder:
    exe = Path(folder) / 'boot-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(app), '-I', str(host.parent), str(host/'boot_test.c'),
                    str(app/'boot_status.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
subprocess.run([sys.executable, str(host/'run_storage_probe_tests.py'), compiler], check=True)
