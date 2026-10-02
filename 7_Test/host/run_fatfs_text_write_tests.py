"""运行 FatFs 示例写入流程，验证保存正文时不会写入缓冲区零填充。"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]


def source(name):
    data = (root / '3_Protocol' / 'FatFs' / name).read_bytes()
    text = data.decode('gbk').replace('\r\n', '\n')
    return re.sub(r'^#include[^\n]*\n', '', text, flags=re.M)


flash = source('FATFS_FLASH_test.c').split('void fatfs_flash_test2(void)')[0]
sdcard = source('FATFS_SDCARD_test.c')
with tempfile.TemporaryDirectory(prefix='remoter-fatfs-text-') as directory:
    temp = Path(directory)
    (temp / 'production_fatfs_text.inc').write_bytes((flash + '\n' + sdcard).encode('gbk'))
    executable = temp / 'fatfs-text-test.exe'
    subprocess.run([
        sys.argv[1] if len(sys.argv) > 1 else 'gcc', '-std=c99', '-O2',
        '-Wall', '-Wextra', '-Werror', '-finput-charset=GBK', '-fexec-charset=GBK',
        '-I', str(temp), str(Path(__file__).with_name('fatfs_text_write_test.c')),
        '-o', str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True, timeout=10)
