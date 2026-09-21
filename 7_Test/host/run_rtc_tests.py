from pathlib import Path
import re
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + re.escape(name) +
                      r'\([^;]*?\)\s*\{.*?^\}[ \t]*(?=\n|$)', source, re.M | re.S)
    if match is None:
        raise RuntimeError('Production function not found: ' + name)
    return match.group(0)


gps_driver = (root / '5_ModuleDrivers/bsp_usart_gps.c').read_bytes().decode('latin1').replace('\r\n', '\n')
gps_service = (root / '7_Test/nmea_decode_test.c').read_bytes().decode('latin1').replace('\r\n', '\n')
gps_implementation = '\n\n'.join([
    function(gps_driver, 'IsLeapYear'), function(gps_driver, 'GMTconvert'),
    *[function(gps_service, name) for name in
      ['gps_now', 'gps_time_is_fresh', 'gps_date_valid', 'gps_accept_time']]])
with tempfile.TemporaryDirectory(prefix='remoter-rtc-') as directory:
    exe = Path(directory) / 'rtc-service-test.exe'
    (Path(directory) / 'rtc_gps_impl.inc').write_text(gps_implementation, encoding='utf8')
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
        '-I', str(host / 'rtc_mocks'), '-I', str(root / '5_SystemDrivers'),
        '-I', directory, '-I', str(root / '3_Protocol/nmea_decode/include'),
        str(host / 'rtc_service_test.c'), str(root / '5_SystemDrivers/bsp_rtc.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
