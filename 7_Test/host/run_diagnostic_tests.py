"""Compile production diagnostic functions with fault-injected hardware mocks."""
from pathlib import Path
import subprocess, tempfile, sys

root = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
with tempfile.TemporaryDirectory(prefix='remoter-diag-') as folder:
    tmp = Path(folder)
    # Stubs are supplied by the test translation unit, before production code.
    for name in ['bsp_spi_flash.h', 'bsp_i2c_eeprom.h', 'bsp_Systick.h', 'ff.h']:
        (tmp/name).write_text('', encoding='ascii')
    (tmp/'hardware_tests.c').write_bytes((root/'1_App/hardware_tests.c').read_bytes())
    (tmp/'hardware_tests.h').write_bytes((root/'1_App/hardware_tests.h').read_bytes())
    (tmp/'spi_flash_layout.h').write_bytes((root/'5_ModuleDrivers/spi_flash_layout.h').read_bytes())
    exe = tmp/'diagnostic-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(tmp), str(Path(__file__).with_name('diagnostic_test.c')),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    # Menu replay: exact key sequences must be required before any EEPROM write.
    (tmp/'stm32f4xx.h').write_text('', encoding='ascii')
    (tmp/'diagnostics.h').write_text('', encoding='ascii')
    (tmp/'menu.h').write_bytes((root/'1_App/menu.h').read_bytes())
    (tmp/'eeprom_menu.c').write_bytes((root/'1_App/eeprom_menu.c').read_bytes())
    exe = tmp/'eeprom-menu-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
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
    (tmp/'radio_diagnostic.c').write_bytes((root/'1_App/radio_diagnostic.c').read_bytes())
    exe = tmp/'radio-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(tmp), str(Path(__file__).with_name('radio_diagnostic_test.c')),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
