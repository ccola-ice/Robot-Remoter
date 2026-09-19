from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root/'5_Middleware/MPU6050/eMPL/inv_mpu.c').read_bytes().decode('latin1').replace('\r\n', '\n')
start = source.index('u8 mpu_dmp_get_data(')
end = source.index('\n}', start) + 2
with tempfile.TemporaryDirectory(prefix='remoter-imu-fifo-') as folder:
    tmp = Path(folder)
    (tmp/'imu_fifo.inc').write_text(source[start:end], encoding='utf8')
    exe = tmp/'imu-fifo-tests.exe'
    subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'gcc', '-std=c99', '-O2',
                    '-Wall', '-Wextra', '-Werror', '-I', str(tmp),
                    str(Path(__file__).with_name('imu_fifo_test.c')), '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
