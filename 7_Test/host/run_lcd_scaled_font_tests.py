"""Compare production RGB565 row-cache scaling with the prior two-buffer algorithm."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
source = (root / '5_ModuleDrivers/bsp_fsmc_lcd.c').read_bytes().decode('gbk').replace('\r\n', '\n')
names = ['lcd_draw_scaled_glyph', 'ILI9806G_DisplayStringEx', 'ILI9806G_DisplayStringEx_YDir']
functions = []
for name in names:
    matches = list(re.finditer(r'^(?:static )?void\s+' + name + r'\s*\(.*?^\}', source, re.M | re.S))
    if len(matches) != 1:
        raise RuntimeError('Expected one production function: ' + name)
    functions.append(matches[0].group())
assert 'zoomBuff' not in source and 'zoomTempBuff' not in source
with tempfile.TemporaryDirectory(prefix='remoter-scaled-font-') as directory:
    temporary = Path(directory)
    (temporary / 'lcd_scaled_font_impl.inc').write_text('\n\n'.join(functions), encoding='utf8')
    executable = temporary / 'lcd-scaled-font-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', directory, str(Path(__file__).with_name('lcd_scaled_font_test.c')),
                    '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
