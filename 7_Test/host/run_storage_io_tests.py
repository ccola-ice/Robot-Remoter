"""Compile production FatFs glue and SDIO wait/IRQ routines with fault injection."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]

def source(path):
    # Byte-preserving: this legacy tree mixes GBK and UTF-8 comments.
    return (root / path).read_bytes().decode('latin1').replace('\r\n', '\n')

def function(text, signature):
    start = text.index(signature + '\n{')
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'

driver = source('5_ModuleDrivers/bsp_sdio_sd.c')
helpers = driver[driver.index('/* DWT runs before'):]
helpers = helpers[:helpers.index('/**')]
for signature in ['SD_Error SD_WaitReadOperation(void)',
                  'SD_Error SD_WaitWriteOperation(void)',
                  'SD_Error SD_ProcessIRQSrc(void)',
                  'void SD_ProcessDMAIRQ(void)',
                  'static SD_Error CmdResp3Error(void)']:
    helpers += function(driver, signature)
sd_header = source('5_ModuleDrivers/bsp_sdio_sd.h').replace('#include "stm32f4xx.h"', '')
disk_header = source('3_Protocol/FatFs/diskio.h').replace('#include "integer.h"', '')
disk = re.sub(r'^#include[^\n]*\n', '', source('3_Protocol/FatFs/diskio.c'), flags=re.M)
with tempfile.TemporaryDirectory(prefix='remoter-storage-io-') as directory:
    temp = Path(directory)
    (temp / 'production_storage.inc').write_text(helpers + '\n' + disk, encoding='utf8')
    (temp / 'production_storage_types.h').write_text(sd_header + '\n' + disk_header, encoding='utf8')
    executable = temp / 'storage-io-test.exe'
    subprocess.run([sys.argv[1] if len(sys.argv) > 1 else 'gcc', '-std=c99', '-O2',
                    '-Wall', '-Wextra', '-Werror', '-I', str(temp),
                    '-I', str(root / '5_ModuleDrivers'),
                    str(Path(__file__).with_name('storage_io_test.c')),
                    '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=10)
