from pathlib import Path
import re
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
drivers = host.parents[1] / '5_ModuleDrivers'
source = (drivers / 'bsp_spi_flash.c').read_bytes().decode('gb18030').replace('\r\n', '\n')
header = (drivers / 'bsp_spi_flash.h').read_text(encoding='utf8')

def function(signature):
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

definitions = '\n'.join(line for line in header.splitlines()
                        if re.match(r'#define\s+(W25X_|WIP_Flag|FLASH_PageSize)', line))
implementations = '\n\n'.join(function(signature) for signature in [
    'void FLASH_Erase_Sectors(', 'void FLASH_Read_Data(',
    'void FLASH_Write_Page_v1(', 'void FLASH_Write_Page_v2(',
    'void FLASH_Write_Page_v3(', 'void FLASH_Write_Data(',
    'static void Flash_Write_Enable(void)\n{',
    'static uint8_t Flash_Wait_For_Standby(uint32_t timeout_ms)\n{',
])
with tempfile.TemporaryDirectory(prefix='remoter-spi-flash-') as folder:
    tmp = Path(folder)
    (tmp / 'flash_write_impl.inc').write_text(definitions + '\n' + implementations, encoding='utf8')
    exe = tmp / 'flash-test.exe'
    subprocess.run([sys.argv[1] if len(sys.argv) > 1 else 'gcc', '-std=c99', '-O2',
                    '-Wall', '-Wextra', '-Werror', '-I', str(tmp), '-I', str(drivers),
                    str(host / 'spi_flash_write_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
