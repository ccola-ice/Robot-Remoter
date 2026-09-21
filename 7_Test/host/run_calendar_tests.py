from pathlib import Path
import re
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
source = (root / '1_App/menu.c').read_bytes().decode('latin1').replace('\r\n', '\n')
parts = []
for name in ('menu_calendar_refresh', 'menu_calendar_load', 'menu_handle_calendar_key'):
    matches = re.findall(r'(?ms)^static void ' + name + r'\(.*?^\}(?=\n|$)', source)
    assert len(matches) == 1, name
    parts.append(matches[0])
with tempfile.TemporaryDirectory(prefix='remoter-calendar-') as directory:
    tmp = Path(directory)
    (tmp / 'calendar_menu_impl.inc').write_text('\n'.join(parts), encoding='utf8')
    exe = tmp / 'calendar-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
        '-I', str(tmp), '-I', str(root / '1_App'), '-I', str(root / '5_SystemDrivers'),
        str(host / 'calendar_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
