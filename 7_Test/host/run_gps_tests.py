"""Compile production NMEA/service and the production DMA queue using host stubs."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
here = Path(__file__).resolve().parent
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
driver = (root/'5_ModuleDrivers/bsp_usart_gps.c').read_bytes().decode('latin1').replace('\r\n', '\n')
queue = driver[driver.index('uint8_t gps_rbuff'):driver.index('\n /**')]
calendar = driver[driver.index('static uint8_t IsLeapYear'):]
with tempfile.TemporaryDirectory(prefix='remoter-gps-') as folder:
    tmp = Path(folder)
    (tmp/'gps_queue.inc').write_text(queue + '\n' + calendar, encoding='utf8')
    (tmp/'nmea_decode_test.c').write_bytes((root/'7_Test/nmea_decode_test.c').read_bytes())
    (tmp/'nmea_decode_test.h').write_bytes((root/'7_Test/nmea_decode_test.h').read_bytes())
    (tmp/'stm32f4xx.h').write_text('#include <stdint.h>\n', encoding='ascii')
    (tmp/'bsp_SysTick.h').write_text('int get_tick_count(unsigned long *count);\n', encoding='ascii')
    (tmp/'bsp_usart_gps.h').write_text('''#include <stdint.h>
#include "nmea/nmea.h"
#define GPS_RBUFF_SIZE 512U
#define HALF_GPS_RBUFF_SIZE 256U
int GPS_DMA_ReadBlock(uint8_t *data, uint32_t *received_ms);
void GMTconvert(nmeaTIME *source, nmeaTIME *dest, uint8_t zone, uint8_t east);
void trace(const char *, int);
void error(const char *, int);
void gps_info(const char *, int);
''', encoding='ascii')
    (tmp/'bsp_rtc.h').write_text('''#include <stdint.h>
uint8_t RTC_SynchronizeCalendar(uint16_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
''', encoding='ascii')
    src = root/'3_Protocol/nmea_decode/src'
    sources = [src/name for name in ['tok.c','parse.c','parser.c','context.c','info.c','time.c','gmath.c']]
    exe = tmp/'gps-tests.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-but-set-variable',  # Existing gmath.c variables.
                    '-I', str(tmp), '-I', str(root/'3_Protocol/nmea_decode/include'),
                    str(here/'gps_test.c'), str(tmp/'nmea_decode_test.c'),
                    *map(str, sources), '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
