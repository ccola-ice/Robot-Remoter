from pathlib import Path
import re
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
menu = (root / '1_App/menu.c').read_bytes().decode('latin1').replace('\r\n', '\n')
param = (root / '1_App/param.c').read_bytes().decode('latin1').replace('\r\n', '\n')

def function(source, name):
    matches = re.findall(r'(?ms)^(?:static )?(?:void|uint8_t) ' + name + r'\(.*?^\}(?=\n|$)', source)
    assert len(matches) == 1, name
    return matches[0]

pieces = ['\n'.join(re.findall(r'^#define PARAM_\w+[^\n]*', menu, re.M))]
pieces += [function(param, name) for name in ('param_load_defaults', 'param_sanitize')]
pieces += [function(menu, name) for name in (
    'menu_nrf_power_index', 'menu_param_set_status', 'menu_param_copy_from_runtime',
    'menu_param_load', 'menu_param_adjust_window', 'menu_param_supported',
    'menu_param_format_item', 'menu_param_adjust_float', 'menu_param_adjust', 'menu_handle_param_key')]
with tempfile.TemporaryDirectory(prefix='remoter-param-menu-') as directory:
    tmp = Path(directory)
    (tmp / 'param_menu_impl.inc').write_text('\n'.join(pieces), encoding='utf8')
    (tmp / 'stm32f4xx.h').write_text('#include <stdint.h>\ntypedef uint8_t u8;\ntypedef uint16_t u16;\n')
    exe = tmp / 'param-menu-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
        '-I', str(tmp), '-I', str(root / '1_App'), '-I', str(root / '5_ModuleDrivers'),
        '-I', str(root / '5_SystemDrivers'),
        str(host / 'param_menu_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
