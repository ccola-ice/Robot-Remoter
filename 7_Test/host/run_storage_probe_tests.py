from pathlib import Path
import subprocess, tempfile, sys

root = Path(__file__).resolve().parents[2]
eeprom = (root/'5_ModuleDrivers/bsp_i2c_eeprom.c').read_bytes().decode('gb18030')
flash = (root/'5_ModuleDrivers/bsp_spi_flash.c').read_bytes().decode('gb18030')
# Compile the checked-in implementations, not copies maintained in the test.
eeprom_part = eeprom[eeprom.index('static uint8_t EEPROM_WaitReadFlag'):]
eeprom_part = eeprom_part[:eeprom_part.index('//addr:')]
flash_part = flash[flash.index('uint8_t FLASH_BootProbe'):]
with tempfile.TemporaryDirectory(prefix='remoter-boot-probe-') as folder:
    tmp = Path(folder)
    (tmp/'storage_probes.inc').write_text(eeprom_part+'\n'+flash_part, encoding='utf8')
    exe = tmp/'probe-test.exe'
    subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'gcc', '-std=c99', '-O2',
                    '-Wall', '-Wextra', '-Werror', '-I', str(tmp),
                    str(Path(__file__).with_name('storage_probe_test.c')), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
