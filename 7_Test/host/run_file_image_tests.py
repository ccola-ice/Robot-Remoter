"""用真实 BMP/JPEG 字节验证缩放、像素边界、损坏数据和取消/读写错误。"""
from pathlib import Path
import base64
import random
import struct
import subprocess
import sys
import tempfile
import zlib

from file_image_fixtures import JPEG_FIXTURES

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"

TYPES = """
#ifndef IMAGE_TEST_TYPES
#define IMAGE_TEST_TYPES
#define _INTEGER
#include <stdint.h>
typedef unsigned char BYTE;
typedef short SHORT;
typedef unsigned short WORD;
typedef int INT;
typedef unsigned int UINT;
typedef int32_t LONG;
typedef uint32_t DWORD;
#endif
"""
FF = """
#ifndef IMAGE_TEST_FF
#define IMAGE_TEST_FF
#include "types.h"
#include <stdio.h>
typedef struct { FILE *stream; DWORD size, offset; } FIL;
typedef enum { FR_OK = 0, FR_DISK_ERR } FRESULT;
#define FA_READ 1U
#define FA_OPEN_EXISTING 0U
#define f_size(f) ((f)->size)
#define f_tell(f) ((f)->offset)
FRESULT f_open(FIL *, const char *, unsigned);
FRESULT f_read(FIL *, void *, UINT, UINT *);
FRESULT f_lseek(FIL *, DWORD);
FRESULT f_close(FIL *);
#endif
"""
LCD = """
#include <stdint.h>
extern uint16_t LCD_X_LENGTH, LCD_Y_LENGTH;
void LCD_BlitRGB565(uint16_t, uint16_t, uint16_t, uint16_t, const uint16_t *);
"""


def bmp(width, height, bpp=24, top_down=False):
    stride = (width * (bpp // 8) + 3) & ~3
    pixels = bytearray()
    rows = range(height) if top_down else range(height - 1, -1, -1)
    for y in rows:
        row = bytearray()
        for x in range(width):
            row += bytes(((x * 37 + y * 19) & 255, (y * 41) & 255, (x * 53) & 255))
            if bpp == 32:
                row += b"\0"
        pixels += row + b"\xAA" * (stride - len(row))
    return (b"BM" + struct.pack("<IHHI", 54 + len(pixels), 0, 0, 54) +
            struct.pack("<IiiHHIIiiII", 40, width, -height if top_down else height,
                        1, bpp, 0, len(pixels), 0, 0, 0, 0) + pixels)


def coefficient_jpeg(quantizer, dc_category=0, dc_bits="", ac_symbol=None,
                     ac_bits="", mcus=1):
    def segment(marker, data):
        return bytes((255, marker)) + struct.pack(">H", len(data) + 2) + data

    dc_table = bytes((1,)) + bytes(15) + bytes((dc_category,))
    ac_table = (bytes((1,)) + bytes(15) + bytes((0,)) if ac_symbol is None else
                bytes((1, 1)) + bytes(14) + bytes((ac_symbol, 0)))
    tables = bytes((0,)) + dc_table + bytes((1,)) + dc_table
    tables += bytes((16,)) + ac_table + bytes((17,)) + ac_table
    frame = bytes((8,)) + struct.pack(">HH", 8, mcus * 8) + bytes((3, 1, 17, 0, 2, 17, 0, 3, 17, 0))
    scan = bytes((3, 1, 0, 2, 17, 3, 17, 0, 63, 0))
    bits = ("0" + dc_bits + ("0" if ac_symbol is None else "0" + ac_bits + "10")) * (3 * mcus)
    bits += "1" * (-len(bits) % 8)
    entropy = bytes(int(bits[at:at + 8], 2) for at in range(0, len(bits), 8)).replace(b"\xff", b"\xff\x00")
    return (b"\xff\xd8" + segment(219, bytes((0,)) + bytes((quantizer,)) * 64) +
            segment(192, frame) + segment(196, tables) + segment(218, scan) + entropy + b"\xff\xd9")


with tempfile.TemporaryDirectory(prefix="remoter-file-image-") as temporary:
    folder = Path(temporary)
    for name, data in (("types.h", TYPES), ("ff.h", FF), ("bsp_fsmc_lcd.h", LCD)):
        (folder / name).write_text(data, encoding="ascii")
    exe = folder / "image-test.exe"
    subprocess.run([compiler, "-std=c99", "-O2", "-ftrapv", "-Wall", "-Wextra", "-Werror",
                    "-include", str(folder / "types.h"), "-I", str(folder),
                    "-I", str(root / "1_App"), "-I", str(root / "5_Middleware/tjpgd/src"),
                    str(host / "file_image_test.c"), str(root / "1_App/file_image.c"),
                    str(root / "5_Middleware/tjpgd/src/tjpgd.c"), "-o", str(exe)], check=True)
    idct_exe = folder / "idct-test.exe"
    subprocess.run([compiler, "-std=c99", "-O2", "-ftrapv", "-Wall", "-Wextra", "-Werror",
                    "-include", str(folder / "types.h"), "-I", str(root / "5_Middleware/tjpgd/src"),
                    str(host / "file_image_idct_test.c"), "-o", str(idct_exe)], check=True)
    subprocess.run([str(idct_exe)], check=True)
    count = 0

    def run(name, data, expected=0, viewport=(24, 136, 752, 280), cancel=0,
            fail_read=0, fail_seek=0, fail_close=0):
        global count
        path = folder / name
        if data is not None:
            path.write_bytes(data)
        output = folder / "frame.bin"
        result = subprocess.run([str(exe), str(path), str(expected), *map(str, viewport),
                                 str(cancel), str(fail_read), str(fail_seek), str(fail_close),
                                 str(output)], check=True, capture_output=True, text=True)
        count += 1
        info = tuple(map(int, result.stdout.split()))
        frame = struct.unpack("<384000H", output.read_bytes())
        return info, frame

    for width, height in ((3, 2), (17, 11), (799, 361), (1, 480), (2048, 1)):
        for bpp in (24, 32):
            for top_down in (False, True):
                info, frame = run("valid.bmp", bmp(width, height, bpp, top_down))
                assert info[:2] == (width, height) and info[4] == 1
                dw, dh = info[2:4]
                dx, dy = 24 + (752 - dw) // 2, 136 + (280 - dh) // 2
                for y in range(dh):
                    for x in range(dw):
                        sx, sy = x * width // dw, y * height // dh
                        r, g, b = (sx * 53) & 255, (sy * 41) & 255, (sx * 37 + sy * 19) & 255
                        expected = ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)
                        assert frame[(dy + y) * 800 + dx + x] == expected

    valid = bmp(17, 11)
    run("missing.bmp", None, 3)
    run("bad.bmp", b"", 2)
    run("bad.bmp", b"hello", 1)
    for length in (2, 12, 53, 54, len(valid) - 1):
        run("truncated.bmp", valid[:length], 2)
    for offset, value, expected in ((10, 53, 2), (14, 12, 1), (18, 0, 2),
                                     (18, 9000, 4), (22, 0, 2), (30, 1, 1), (2, 55, 2)):
        bad = bytearray(valid)
        struct.pack_into("<I", bad, offset, value)
        run("bad.bmp", bad, expected)
    run("cancel.bmp", valid, 5, cancel=1)
    run("cancel.bmp", valid, 5, cancel=7)
    run("read.bmp", valid, 3, fail_read=1)
    run("read.bmp", valid, 3, fail_read=4)
    run("seek.bmp", valid, 3, fail_seek=1)
    run("close.bmp", valid, 3, fail_close=1)
    run("viewport.bmp", valid, 1, viewport=(799, 479, 2, 2))
    run("viewport.bmp", valid, 1, viewport=(0, 0, 0, 2))

    for name, width, height, encoded in JPEG_FIXTURES:
        data = zlib.decompress(base64.b64decode(encoded))
        expected = 1 if name in ("progressive", "grayscale") else 0
        for viewport in ((24, 136, 752, 280), (791, 473, 9, 7)):
            info, frame = run(name + ".jpg", data, expected, viewport)
            if expected == 0:
                assert info[:2] == (width, height) and info[4] == 2
                dw, dh = info[2:4]
                dx, dy = viewport[0] + (viewport[2] - dw) // 2, viewport[1] + (viewport[3] - dh) // 2
                for y in range(dh):
                    for x in range(dw):
                        pixel = frame[(dy + y) * 800 + dx + x]
                        rgb = (80, 160, 224)
                        if name == "quadrants":
                            sx, sy = x * width // dw, y * height // dh
                            if abs(sx - width // 2) < 20 or abs(sy - height // 2) < 20:
                                continue
                            rgb = ((208, 48, 32), (32, 192, 64), (48, 64, 224), (208, 176, 32))[(sy >= height // 2) * 2 + (sx >= width // 2)]
                        assert abs(((pixel >> 11) * 255 // 31) - rgb[0]) < 16
                        assert abs((((pixel >> 5) & 63) * 255 // 63) - rgb[1]) < 16
                        assert abs(((pixel & 31) * 255 // 31) - rgb[2]) < 16
                run("truncated.jpg", data[:-2], 2)
                run("cancel.jpg", data, 5, cancel=3)
                run("read.jpg", data, 3, fail_read=3)
                assert run("decode-cancel.jpg", data, 5, viewport, cancel=info[5] - 1)[0][6] == 1
                assert run("decode-read.jpg", data, 3, viewport, fail_read=info[7])[0][6] == 1
    baseline = zlib.decompress(base64.b64decode(JPEG_FIXTURES[0][3]))
    for marker, length in ((b"\xff\xc0", 3), (b"\xff\xda", 3), (b"\xff\xdb", 3), (b"\xff\xc4", 3)):
        bad = bytearray(baseline)
        at = bad.index(marker)
        struct.pack_into(">H", bad, at + 2, length)
        run("bad-marker.jpg", bad, 2)
    bad = bytearray(baseline)
    at = bad.index(b"\xff\xc0")
    struct.pack_into(">H", bad, at + 7, 9000)
    run("too-large.jpg", bad, 4)
    run("zero-coefficients.jpg", coefficient_jpeg(255), 0)
    run("overflow-positive-dc.jpg", coefficient_jpeg(255, 11, "1" * 11), 2)
    run("overflow-negative-dc.jpg", coefficient_jpeg(255, 11, "0" * 11), 2)
    run("overflow-dc-predictor.jpg", coefficient_jpeg(1, 11, "1" * 11, mcus=17), 2)
    run("overflow-positive-ac.jpg", coefficient_jpeg(255, ac_symbol=0x3A, ac_bits="1" * 10), 2)
    run("overflow-negative-ac.jpg", coefficient_jpeg(255, ac_symbol=0x3A, ac_bits="0" * 10), 2)
    run("overflow-idct.jpg", coefficient_jpeg(255, ac_symbol=8, ac_bits="1" * 8), 2)
    # 固定种子破坏段头、霍夫曼表或熵流；允许解码成功，但必须保持像素边界和资源释放。
    random_source = random.Random(0xF407)
    for index in range(96):
        bad = bytearray(baseline)
        for _ in range(1 + index % 4):
            at = random_source.randrange(2, len(bad))
            bad[at] ^= 1 << random_source.randrange(8)
        info, _ = run("mutated.jpg", bad, 99)
        assert info[6] == 1
    print(f"File image: {count} cases passed; real BMP pixels, real JPEG decoder, bounded drawing, errors and cancellation.")
