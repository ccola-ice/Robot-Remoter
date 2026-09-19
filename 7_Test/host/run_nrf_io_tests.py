"""Exercise actual NRF driver/platform code with SPI and radio register models."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[2]
def source(name):
    return (root/name).read_bytes().decode('latin1').replace('\r\n','\n')
def strip_includes(text):
    return re.sub(r'^#include[^\n]*\n','',text,flags=re.M)
driver=strip_includes(source('5_ModuleDrivers/bsp_spi_nrf.c'))
# GPIO mux/clock initialization is hardware-specific; retain all transport,
# reset-state, register, transmit, receive and configuration implementations.
start=driver.index('void NRF_SPI_Init(void)\n{')
end=driver.index('\n}\n',start)+3
driver=driver[:start]+driver[end:]
platform=strip_includes(source('2_Platform/platform_nrf.c'))
with tempfile.TemporaryDirectory(prefix='remoter-nrf-io-') as directory:
    tmp=Path(directory)
    (tmp/'bsp_spi_nrf.h').write_text(strip_includes(source('5_ModuleDrivers/bsp_spi_nrf.h')),encoding='utf8')
    (tmp/'production_nrf.inc').write_text(driver+'\n'+platform,encoding='utf8')
    exe=tmp/'nrf-io-test.exe'
    subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'gcc','-std=c99','-O2',
                    '-Wall','-Wextra','-Werror','-I',str(tmp),
                    str(Path(__file__).with_name('nrf_io_test.c')),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=10)
