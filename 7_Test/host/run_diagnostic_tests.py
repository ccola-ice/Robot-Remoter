"""Compile production diagnostic functions with fault-injected hardware mocks."""
from pathlib import Path
import re, subprocess, tempfile, sys

root = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
with tempfile.TemporaryDirectory(prefix='remoter-diag-') as folder:
    tmp = Path(folder)
    # Stubs are supplied by the test translation unit, before production code.
    for name in ['bsp_spi_flash.h', 'bsp_i2c_eeprom.h', 'bsp_Systick.h', 'ff.h']:
        (tmp/name).write_text('', encoding='ascii')
    (tmp/'diag_hardware.c').write_bytes((root/'1_App/diagnostics/diag_hardware.c').read_bytes())
    (tmp/'diag_hardware.h').write_bytes((root/'1_App/diagnostics/diag_hardware.h').read_bytes())
    (tmp/'spi_flash_layout.h').write_bytes((root/'5_ModuleDrivers/spi_flash_layout.h').read_bytes())
    exe = tmp/'diagnostic-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(tmp), str(Path(__file__).with_name('diagnostic_test.c')),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    # Exercise the production polling path with timed GPIO levels, then replay
    # long holds through the real EEPROM editor to verify write confirmation.
    (tmp/'stm32f4xx.h').write_text('', encoding='ascii')
    (tmp/'diag_menu.h').write_text('', encoding='ascii')
    (tmp/'ui_menu.h').write_bytes((root/'1_App/ui/ui_menu.h').read_bytes())
    source = (root/'1_App/diagnostics/diag_menu.c').read_bytes().decode('gbk')
    source = source.replace('\r\n', '\n')
    declarations = re.search(
        r'static uint8_t diag_key_levels\[4\].*?static DiagNavigationRepeat diag_repeat\[2\];',
        source, re.S)
    assert declarations, 'Missing diagnostic input state'
    functions = []
    for name in ['diag_present', 'key_down', 'diag_reset_keys', 'diag_release',
                 'diag_key', 'diag_screen']:
        match = re.search(r'(?m)^(?:static )?(?:void|uint8_t|int) ' + name +
                          r'\([^\n]*\)\n\{.*?^\}', source, re.S | re.M)
        assert match, 'Missing production function: ' + name
        functions.append(match.group(0))
    (tmp/'diag_key.inc').write_text(declarations.group(0) + '\n' +
                                    '\n'.join(functions), encoding='utf-8')
    (tmp/'diag_eeprom.c').write_text(
        (root/'1_App/diagnostics/diag_eeprom.c').read_bytes().decode('gbk'), encoding='utf-8')
    exe = tmp/'diag-key-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-finput-charset=UTF-8', '-fexec-charset=GBK',
                    '-I', str(tmp), str(Path(__file__).with_name('diag_key_test.c')),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    # Menu replay: exact key sequences must be required before any EEPROM write.
    (tmp/'stm32f4xx.h').write_text('', encoding='ascii')
    (tmp/'diag_menu.h').write_text('', encoding='ascii')
    (tmp/'ui_menu.h').write_bytes((root/'1_App/ui/ui_menu.h').read_bytes())
    (tmp/'diag_eeprom.c').write_text(
        (root/'1_App/diagnostics/diag_eeprom.c').read_bytes().decode('gbk'), encoding='utf-8')
    exe = tmp/'eeprom-menu-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-finput-charset=UTF-8', '-fexec-charset=GBK',
                    '-I', str(tmp), str(Path(__file__).with_name('eeprom_menu_test.c')),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    (tmp/'stm32f4xx.h').write_text('''#define SPI3 3U
#define GPIOE 5U
#define GPIO_Pin_5 32U
#define GPIO_Pin_6 64U
''', encoding='ascii')
    (tmp/'multi_button_user.h').write_text('', encoding='ascii')
    (tmp/'bsp_spi_nrf.h').write_bytes((root/'5_ModuleDrivers/bsp_spi_nrf.h').read_bytes())
    (tmp/'diag_radio.c').write_bytes((root/'1_App/diagnostics/diag_radio.c').read_bytes())
    exe = tmp/'radio-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(tmp), str(Path(__file__).with_name('radio_diagnostic_test.c')),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
