from pathlib import Path
import re
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]

def read(path):
    return (root / path).read_bytes().decode('latin1').replace('\r\n', '\n')

def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    position = opening + 1
    depth = 1
    while depth:
        if source[position] == '{':
            depth += 1
        elif source[position] == '}':
            depth -= 1
        position += 1
    return source[start:position]

timer = read('6_Core/stm32f4xx_it.c')
systick = read('5_SystemDrivers/bsp_SysTick.c')
main = read('1_App/main.c')
menu = read('1_App/menu.c')
gui = read('1_App/gui.c')
mpu = read('5_Middleware/MPU6050/eMPL/inv_mpu.c')
pieces = ['\n'.join(re.findall(r'^#define (?:PARAM_|ROBOT_)\w+[^\n]*', menu + '\n' + gui, re.M))]
pieces += [systick[systick.index('static volatile uint32_t g_ul_ms_ticks;'):]]
pieces += [function(mpu, 'void mget_ms(unsigned long *time)')]
pieces += [function(timer, name) for name in ['void SysTick_Handler(void)', 'void GENERAL_TIM5_IRQHandler(void)']]
pieces += [function(main, 'static uint8_t take_tick(')]
pieces += [menu[menu.index('static GuiRobotTelemetry robot_telemetry;'):menu.index('static uint8_t menu_nrf_power_index(')]]
pieces += [function(menu, name) for name in ['static uint8_t menu_nrf_power_index(',
           'static uint8_t menu_param_supported(', 'static void menu_param_format_item(']]
pieces += [function(gui, 'static void gui_robot_format_value('), function(gui, 'void robot_control_page(')]
snapshot_end = gui.index('} gui_gps_snapshot_t;') + len('} gui_gps_snapshot_t;')
snapshot_start = gui.rfind('typedef struct', 0, snapshot_end)
pieces += [gui[snapshot_start:snapshot_end], function(gui, 'void system_data_read_and_set(void)')]
with tempfile.TemporaryDirectory(prefix='remoter-runtime-') as folder:
    tmp = Path(folder)
    (tmp / 'runtime_safety_impl.inc').write_text('\n'.join(pieces), encoding='utf8')
    (tmp / 'stm32f4xx.h').write_text('#include <stdint.h>\ntypedef uint8_t u8;\ntypedef uint16_t u16;\n')
    exe = tmp / 'runtime-test.exe'
    subprocess.run([sys.argv[1] if len(sys.argv) > 1 else 'gcc', '-std=gnu99', '-O2',
                    '-Wall', '-Wextra', '-Werror', '-Wno-format-truncation',
                    '-I', str(tmp), '-I', str(root / '1_App'), '-I', str(root / '5_ModuleDrivers'),
                    str(host / 'runtime_safety_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
