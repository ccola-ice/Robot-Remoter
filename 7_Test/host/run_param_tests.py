from pathlib import Path
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
with tempfile.TemporaryDirectory(prefix='remoter-param-') as folder:
    tmp = Path(folder)
    (tmp / 'stm32f4xx.h').write_text('''#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
''')
    (tmp / 'bsp_spi_flash.h').write_text('''#include <stdint.h>
void FLASH_Read_Data(uint8_t *, uint32_t, uint16_t);
void FLASH_Write_Data(uint8_t *, uint32_t, uint16_t);
void FLASH_Erase_Sectors(uint32_t);
uint8_t FLASH_GetIoError(void);
''')
    exe = tmp / 'param-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(tmp), '-I', str(root / '5_ModuleDrivers'),
                    str(host / 'param_store_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
