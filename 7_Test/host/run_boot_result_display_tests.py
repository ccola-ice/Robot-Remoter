"""Check the production boot result layout with captured LCD text calls."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from app_test_paths import app_include_args


root = Path(__file__).resolve().parents[2]
host = Path(__file__).resolve().parent
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
source = (root / '1_App/ui/ui_pages.c').read_bytes().decode('gbk').replace('\r\n', '\n')
declarations = []
for pattern in (r'^char displayBuffer\[\d+\];$',
                r'^static const BootReport \*boot_last_report;$'):
    matches = re.findall(pattern, source, re.M)
    if len(matches) != 1:
        raise RuntimeError('Expected one production declaration: ' + pattern)
    declarations.append(matches[0])
for name in ('gui_boot_color', 'gui_boot_row', 'gui_boot_finish'):
    matches = re.findall(r'^(?:static )?(?:void|uint16_t)\s+' + name +
                         r'\s*\(.*?^\}', source, re.M | re.S)
    if len(matches) != 1:
        raise RuntimeError('Expected one production function: ' + name)
    declarations.append(matches[0])

with tempfile.TemporaryDirectory(prefix='remoter-boot-display-') as folder:
    temporary = Path(folder)
    (temporary / 'boot_result_display_impl.inc').write_text(
        '\n\n'.join(declarations), encoding='utf8')
    executable = temporary / 'boot-result-display-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    *app_include_args(), '-I', folder,
                    str(host / 'boot_result_display_test.c'),
                    str(root / '1_App/system/boot_status.c'),
                    '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
