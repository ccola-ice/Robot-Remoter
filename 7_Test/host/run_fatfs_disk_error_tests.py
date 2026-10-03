"""Run real FatFs and diskio against a RAM card, including first-write faults."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]


def source(path):
    # 旧驱动的 GBK 注释按字节保留，避免测试准备阶段破坏源文件。
    return (ROOT / path).read_bytes().decode("latin1").replace("\r\n", "\n")


with tempfile.TemporaryDirectory(prefix="remoter-fatfs-disk-error-") as directory:
    temp = Path(directory)
    for name in ("ff.c", "ff.h", "ffconf.h", "diskio.h", "cc936.c"):
        (temp / name).write_text(source("3_Protocol/FatFs/" + name), encoding="utf8")

    # 主机类型固定到固件位宽；生产 FatFs 配置和完整函数保持不变。
    (temp / "integer.h").write_text(
        "#ifndef _FF_INTEGER\n#define _FF_INTEGER\n#include <stdint.h>\n"
        "typedef uint8_t BYTE; typedef int16_t SHORT; typedef uint16_t WORD;\n"
        "typedef uint16_t WCHAR; typedef int INT; typedef unsigned int UINT;\n"
        "typedef int32_t LONG; typedef uint32_t DWORD;\n#endif\n",
        encoding="ascii",
    )
    sd_header = source("5_ModuleDrivers/bsp_sdio_sd.h").replace(
        '#include "stm32f4xx.h"', ""
    )
    (temp / "production_sd_types.h").write_text(sd_header, encoding="utf8")
    disk = re.sub(
        r"^#include[^\n]*\n", "", source("3_Protocol/FatFs/diskio.c"), flags=re.M
    )
    (temp / "production_diskio.inc").write_text(disk, encoding="utf8")
    compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"
    common = [compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-I", str(temp),
              "-I", str(ROOT / "5_ModuleDrivers")]
    # FatFs 的历史第三方告警不阻止编译；测试代码与生产 diskio 则严格检查。
    objects = []
    for name in ("ff", "cc936"):
        obj = temp / (name + ".o")
        subprocess.run(common + ["-c", str(temp / (name + ".c")), "-o", str(obj)],
                       check=True)
        objects.append(str(obj))
    executable = temp / "fatfs-disk-error-test.exe"
    subprocess.run(common + ["-Werror", str(Path(__file__).with_name("fatfs_disk_error_test.c")),
                            *objects, "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=15)
