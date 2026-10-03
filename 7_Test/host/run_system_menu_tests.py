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
interrupts = (root / '6_Core/stm32f4xx_it.c').read_bytes().decode('latin1').replace('\r\n', '\n')


def function(source, name):
    matches = re.findall(r'(?ms)^(?:static )?(?:void|uint8_t) ' + name + r'\([^;]*?\)\s*\{.*?^\}(?=\n|$)', source)
    assert len(matches) == 1, name
    return matches[0]


parts = [re.search(r'(?ms)^typedef enum\s*\{.*?^\} MenuPage;', menu).group(0)]
parts += [re.search(r'(?m)^#define MENU_EVENT_QUEUE_SIZE[^\n]*', menu).group(0)]
for name in ('repeat_key', 'repeat_pending', 'calendar_state', 'calendar_return_page',
             'system_selected', 'system_status', 'event_queue', 'event_read_index',
             'event_write_index', 'page_dirty', 'page_changed', 'current_page'):
    parts.append(re.search(r'(?m)^static [^;\n]+\b' + name + r'\b[^;\n]*;', menu).group(0))
parts += [function(param, 'param_load_defaults')]
parts += [function(menu, name) for name in (
    'menu_calendar_refresh', 'menu_calendar_load', 'menu_handle_calendar_key',
    'menu_system_load', 'menu_handle_system_key', 'menu_draw_system',
    'menu_post_key', 'menu_post_repeat', 'menu_get_key')]
parts += [function(interrupts, 'SysTick_Handler')]
with tempfile.TemporaryDirectory(prefix='remoter-system-menu-') as folder:
    tmp = Path(folder)
    (tmp / 'system_menu_impl.inc').write_text('\n'.join(parts), encoding='latin1')
    (tmp / 'stm32f4xx.h').write_text('#include <stdint.h>\ntypedef uint8_t u8;\ntypedef uint16_t u16;\n')
    exe = tmp / 'system-menu-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(tmp), '-I', str(root / '1_App'),
                    '-I', str(root / '5_ModuleDrivers'), '-I', str(root / '5_SystemDrivers'),
                    str(host / 'system_menu_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
