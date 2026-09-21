"""Exercise the production native LCD renderer and legacy entry points with 2bpp fixtures."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
source = (root / '5_ModuleDrivers/bsp_fsmc_lcd.c').read_bytes().decode('latin1').replace('\r\n', '\n')
names = ['LCD_DrawFontGlyph', 'ILI9806G_DispChar_EN', 'LCD_DispString_EN_Bold',
         'ILI9806G_DispChar_CH', 'ILI9806G_DispStringLine_EN_CH',
         'ILI9806G_DispString_EN_CH', 'ILI9806G_DispString_EN_CH_YDir',
         'ILI9806G_DisplayStringEx', 'ILI9806G_DisplayStringEx_YDir']
functions = []
for name in names:
    matches = list(re.finditer(r'^(?:void|uint8_t)\s+' + name + r'\s*\(.*?^\}', source, re.M | re.S))
    if len(matches) != 1:
        raise RuntimeError('Expected one production function: ' + name)
    functions.append(matches[0].group())
with tempfile.TemporaryDirectory(prefix='remoter-native-font-') as directory:
    temporary = Path(directory)
    (temporary / 'lcd_native_font_impl.inc').write_text('\n\n'.join(functions), encoding='utf8')
    executable = temporary / 'lcd-native-font-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', directory, str(Path(__file__).with_name('lcd_native_font_test.c')),
                    '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
