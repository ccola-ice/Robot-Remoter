"""Exercise the production menu event/state functions with peripheral pages mocked."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
source = (root / '1_App/menu.c').read_text(encoding='latin1')

def match(pattern):
    found = re.findall(pattern, source, re.M | re.S)
    if len(found) != 1:
        raise RuntimeError('Expected one production match: ' + pattern)
    return found[0]

state = '\n'.join([
    match(r'(^typedef enum\s*\{.*?\} MenuPage;)'),
    match(r'(^static const MenuPage menu_items\[MENU_ITEM_COUNT\] =.*?^\};)'),
    match(r'(^static MenuKey event_queue.*?^static MenuPage current_page;)'),
    'static MenuKey repeat_key; static uint8_t repeat_pending, param_editing, nrf_editing; static GuiCalendarState calendar_state;',
])
constants = '\n'.join(re.findall(r'^#define (?:MENU_ITEM_COUNT|MENU_EVENT_QUEUE_SIZE|MENU_REFRESH_TICKS|CLOCK_REFRESH_TICKS)[^\r\n]*', source, re.M))
names = ['menu_get_key', 'menu_handle_home_key', 'menu_handle_category_key',
         'menu_handle_page_key', 'menu_draw_monitor', 'menu_init', 'menu_post_key',
         'menu_tick_10ms', 'menu_post_repeat', 'menu_key_context', 'menu_dispatch_key', 'menu_process', 'menu_control_active']
functions = '\n'.join(match(r'(^' + r'(?:static )?(?:void|uint8_t|uint32_t) ' + name + r'\([^;]*?\)\s*\{.*?^\})') for name in names)

with tempfile.TemporaryDirectory(prefix='remoter-menu-navigation-') as folder:
    temp = Path(folder)
    (temp / 'stm32f4xx.h').write_text('#include <stdint.h>\n')
    (temp / 'menu_state.inc').write_text(constants + '\n' + state)
    (temp / 'menu_functions.inc').write_text(functions)
    exe = temp / 'test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(temp), '-I' + str(root / '1_App'),
                    '-I' + str(root / '5_SystemDrivers'),
                    str(host / 'menu_navigation_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
