from pathlib import Path
import subprocess, tempfile, re, sys

root = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
source = (root/'1_App/control_link.c').read_text()
source = re.sub(r'^#include "bsp_[^"]+"\n', '', source, flags=re.M)
with tempfile.TemporaryDirectory(prefix='remoter-control-') as temp:
    temp = Path(temp)
    (temp/'stm32f4xx.h').write_text('#include <stdint.h>\ntypedef uint8_t u8; typedef uint16_t u16;\n')
    (temp/'control_test.c').write_text((root/'7_Test/host/control_test.c').read_text().replace('/* PRODUCTION_SOURCE */', source))
    exe = temp/'test.exe'
    subprocess.run([compiler,'-std=c99','-O2','-Wall','-Wextra','-Werror',
                    '-I'+str(temp),'-I'+str(root/'1_App'),'-I'+str(root/'5_ModuleDrivers'),
                    str(temp/'control_test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
