"""Verify the real LCD bold renderer against the project's installed ASCII font."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"
driver = (repo / "5_ModuleDrivers/bsp_fsmc_lcd.c").read_text(encoding="latin1")
function = re.search(r"(?ms)^void LCD_DispString_EN_Bold\([^;]*?\)\s*\{.*?^\}", driver)
if not function:
    raise RuntimeError("Missing production bold renderer")
with tempfile.TemporaryDirectory(prefix="remoter-ui-bold-") as temporary:
    temporary = Path(temporary)
    (temporary / "ui_bold_impl.inc").write_text(function.group(0), encoding="ascii")
    (temporary / "stm32f4xx.h").write_text("#include <stdint.h>\n", encoding="ascii")
    (temporary / "bsp_usart_debug.h").write_text("#include <string.h>\n", encoding="ascii")
    (temporary / "bsp_spi_flash.h").write_text(
        "static void FLASH_SPI_Init(void) {}\n"
        "static void FLASH_Read_Data(uint8_t *p,unsigned address,unsigned size)\n"
        "{ (void)address; while(size--) *p++=0; }\n", encoding="ascii")
    executable = temporary / "ui-bold-test.exe"
    subprocess.run([
        compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
        "-Wno-unused-function", "-finput-charset=GBK", "-fexec-charset=GBK",
        "-I", str(temporary), "-I", str(repo / "5_ModuleDrivers/fonts"),
        str(Path(__file__).with_name("ui_bold_test.c")),
        str(repo / "5_ModuleDrivers/fonts/fonts.c"), "-o", str(executable)
    ], check=True)
    subprocess.run([str(executable)], check=True)
