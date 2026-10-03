"""验证 GIF 逐帧像素、LZW 字典、合成方式、循环和损坏输入的资源释放。"""
from pathlib import Path
import io
import random
import struct
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
sys.path.insert(0, str(root / "VSCode EIDE Project/build/ui-clarity/python"))
from PIL import Image

compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"
FF = """
#ifndef GIF_TEST_FF
#define GIF_TEST_FF
#include <stdio.h>
#include <stdint.h>
typedef unsigned int UINT;
typedef uint32_t DWORD;
typedef struct { FILE *stream; DWORD size, offset; unsigned char sector[4096]; } FIL;
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


def blocks(data, chunk=255):
    return b"".join(bytes((len(data[i:i + chunk]),)) + data[i:i + chunk]
                    for i in range(0, len(data), chunk)) + b"\0"


def lzw_literals(pixels, minimum=2):
    # 每个像素前发清除码，便于独立构造不依赖被测字典实现的 GIF。
    clear = 1 << minimum
    width = minimum + 1
    codes = [value for pixel in pixels for value in (clear, pixel)] + [clear + 1]
    value = bits = 0
    output = bytearray()
    for code in codes:
        value |= code << bits
        bits += width
        while bits >= 8:
            output.append(value & 255)
            value >>= 8
            bits -= 8
    if bits:
        output.append(value & 255)
    return bytes((minimum,)) + blocks(output, 7)


def packed_codes(codes):
    value = bits = 0
    output = bytearray()
    for code, width in codes:
        value |= code << bits
        bits += width
        while bits >= 8:
            output.append(value & 255)
            value >>= 8
            bits -= 8
    if bits:
        output.append(value & 255)
    return bytes(output)


PALETTE = [(0, 0, 0), (255, 0, 0), (0, 255, 0), (0, 0, 255)]


def header(width, height, global_palette=True):
    return (b"GIF89a" + struct.pack("<HHBBB", width, height, 0x81 if global_palette else 0, 0, 0) +
            (bytes(c for color in PALETTE for c in color) if global_palette else b""))


def frame(left, top, width, height, pixels, disposal=0, delay=10, transparent=None,
          interlaced=False, local=None, control=True):
    gce = (b"\x21\xF9\x04" + bytes(((disposal << 2) | (transparent is not None),)) +
           struct.pack("<H", delay) + bytes((transparent or 0, 0))) if control else b""
    packed = (0x40 if interlaced else 0) | (0x81 if local else 0)
    palette = bytes(c for color in local for c in color) if local else b""
    if interlaced:
        order = list(range(0, height, 8)) + list(range(4, height, 8))
        order += list(range(2, height, 4)) + list(range(1, height, 2))
        pixels = [pixels[y * width + x] for y in order for x in range(width)]
    return (gce + b"," + struct.pack("<HHHHB", left, top, width, height, packed) + palette +
            lzw_literals(pixels))


def loop(count):
    return b"\x21\xFF\x0BNETSCAPE2.0\x03\x01" + struct.pack("<H", count) + b"\0"


def rgb565(rgb):
    r, g, b = rgb
    return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)


with tempfile.TemporaryDirectory(prefix="remoter-file-gif-") as temporary:
    folder = Path(temporary)
    (folder / "ff.h").write_text(FF, encoding="ascii")
    (folder / "bsp_fsmc_lcd.h").write_text(LCD, encoding="ascii")
    exe = folder / "gif-test.exe"
    subprocess.run([compiler, "-std=c99", "-O2", "-ftrapv", "-Wall", "-Wextra", "-Werror",
                    "-I", str(folder), "-I", str(root / "1_App"),
                    str(host / "file_gif_test.c"), str(root / "1_App/file_gif.c"),
                    "-o", str(exe)], check=True)
    count = 0

    def run(data, expected=0, capacity=280576, cancel=0, fail_read=0, fail_seek=0,
            fail_close=0, max_frames=40, viewport=(752, 280), no_memory=False):
        global count
        path = folder / "input.gif"
        if data is None:
            if path.exists():
                path.unlink()
        else:
            path.write_bytes(data)
        output = folder / "frames.bin"
        command = [str(exe), str(path), str(expected), str(capacity), str(cancel),
                   str(fail_read), str(fail_seek), str(fail_close), str(max_frames),
                   *map(str, viewport), str(output), str(int(no_memory))]
        process = subprocess.run(command, capture_output=True, text=True)
        if process.returncode:
            raise AssertionError((command, process.stdout, process.stderr))
        lines = process.stdout.splitlines()
        delays = [int(line.split()[1]) for line in lines if line.startswith("frame ")]
        info = tuple(map(int, lines[-1].split()[1:]))
        pixels = struct.unpack("<%dH" % (len(output.read_bytes()) // 2), output.read_bytes())
        frames = [pixels[i:i + info[3] * info[4]] for i in
                  range(0, len(pixels), info[3] * info[4])] if pixels else []
        count += 1
        return info, frames, delays

    def compare(data, viewport=(752, 280)):
        info, actual, delays = run(data, viewport=viewport)
        expected = []
        with Image.open(io.BytesIO(data)) as reference:
            for number in range(reference.n_frames):
                reference.seek(number)
                rgba = reference.convert("RGBA")
                background = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
                background.alpha_composite(rgba)
                rgb = background.convert("RGB")
                expected.append(tuple(rgb565(rgb.getpixel((x * rgb.width // info[3],
                                                          y * rgb.height // info[4])))
                                      for y in range(info[4]) for x in range(info[3])))
        assert actual == expected, (info, actual[:2], expected[:2])
        assert info[0] == 0 and info[5] == 4 and info[7] == 1
        return info, actual, delays

    valid = header(3, 2) + frame(0, 0, 3, 2, [0, 1, 2, 3, 1, 2]) + b";"
    compare(valid)
    compare(valid.replace(b"GIF89a", b"GIF87a"))
    compare(header(1, 1) + frame(0, 0, 1, 1, [1], control=False) + b";")
    for height in range(1, 18):
        data = header(13, height) + frame(0, 0, 13, height,
                [(x + y) % 4 for y in range(height) for x in range(13)], interlaced=True) + b";"
        compare(data)
    compare(header(640, 400) + frame(0, 0, 640, 400,
            [(x + y) % 4 for y in range(400) for x in range(640)]) + b";")
    compare(header(17, 15, False) + frame(0, 0, 17, 15,
            [i % 4 for i in range(17 * 15)], local=PALETTE) + b";")

    for disposal in (0, 1, 2, 3):
        data = (header(8, 7) + frame(0, 0, 8, 7, [1] * 56) +
                frame(2, 1, 4, 5, [2] * 20, disposal=disposal) +
                frame(1, 2, 5, 3, [3, 0, 3, 0, 3] * 3, transparent=0) + b";")
        compare(data)
        compare(data, (5, 5))
    data = (header(5, 4) + frame(0, 0, 5, 4, [0, 1, 0, 1, 0] * 4,
            transparent=0, disposal=2, delay=1) +
            frame(1, 1, 3, 2, [0, 3, 0] * 2, transparent=0, disposal=3, delay=0) +
            frame(0, 0, 1, 1, [2], delay=65535) + b";")
    _, _, delays = compare(data)
    assert delays == [20, 20, 655350]
    palette = [PALETTE[0], PALETTE[3], PALETTE[1], PALETTE[2]]
    compare(header(3, 2) + frame(0, 0, 3, 2, [1] * 6, local=palette) +
            frame(1, 0, 1, 2, [2, 3]) + b";")

    two = frame(0, 0, 1, 1, [1], delay=7) + frame(0, 0, 1, 1, [2], delay=9)
    for repeat in (0, 1, 2, 5):
        info, actual, delays = run(header(1, 1) + loop(repeat) + two + b";", max_frames=25)
        wanted = 25 if repeat == 0 else 2 * (repeat + 1)
        assert len(actual) == wanted and info[7] == (repeat != 0)
        assert delays == ([70, 90] * 13)[:wanted]
    info, actual, _ = run(header(1, 1) + two + b";")
    assert len(actual) == 2 and info[7] == 1

    # Pillow 编码器生成长字典流，验证位宽递增、字典满以及清除码。
    randomizer = random.Random(7219)
    for size in ((2, 2), (31, 29), (257, 193), (640, 400), (320, 201)):
        image = Image.new("P", size)
        image.putpalette([component for value in range(256)
                          for component in (value, (value * 7) & 255, (value * 19) & 255)])
        image.putdata([randomizer.randrange(256) for _ in range(size[0] * size[1])])
        encoded = io.BytesIO()
        image.save(encoded, format="GIF", interlace=True)
        compare(encoded.getvalue())

    # KwKwK 特例：第二个词条恰好是本轮尚待创建的字典项。
    descriptor = b"," + struct.pack("<HHHHB", 0, 0, 3, 1, 0)
    special = (header(3, 1) + descriptor + b"\x02" +
               blocks(packed_codes([(4, 3), (1, 3), (6, 3), (5, 3)]), 1) + b";")
    compare(special)
    # 字典达到 4096 项后继续使用原字典，不能强制要求清除码。
    size = (101, 101)
    palette256 = bytes(component for value in range(256)
                       for component in (value, value, value))
    full_header = b"GIF89a" + struct.pack("<HHBBB", *size, 0xF7, 0, 0) + palette256
    codes = [(256, 9)]
    width, next_code = 9, 258
    source_pixels = [i & 255 for i in range(size[0] * size[1])]
    for index, pixel in enumerate(source_pixels):
        codes.append((pixel, width))
        if index and next_code < 4096:
            next_code += 1
            if next_code == 1 << width and width < 12:
                width += 1
    codes.append((257, width))
    full_dictionary = (full_header + b"," + struct.pack("<HHHHB", 0, 0, *size, 0) +
                       b"\x08" + blocks(packed_codes(codes), 1) + b";")
    compare(full_dictionary)
    # 帧解码失败不得把部分画布提交到 LCD，上一完整帧仍保持显示。
    first = frame(0, 0, 3, 2, [1] * 6)
    second = frame(0, 0, 3, 2, [2] * 6)
    info, actual, _ = run(header(3, 2) + first + second[:-2], 2)
    assert len(actual) == 1 and actual[0] == (rgb565(PALETTE[1]),) * 6
    for at in (3, 7, 15, 31):
        info, actual, _ = run(full_dictionary, 3, fail_read=at)
        assert not actual
    for at in (4, 7, 15, 40):
        info, actual, _ = run(full_dictionary, 5, cancel=at)
        assert not actual
    run(None, 3)
    run(valid, 6, capacity=100)
    run(valid, 6, no_memory=True)
    run(valid, 5, cancel=1)
    run(valid, 5, cancel=3)
    run(valid, 3, fail_read=1)
    run(valid, 3, fail_close=1)
    run(header(1, 1) + loop(0) + two + b";", 3, fail_seek=1)
    for length in range(len(valid)):
        info, frames, _ = run(valid[:length], 2)
        if length < len(valid) - 1:
            assert not frames
    for offset, replacement, result in (
        (0, b"BAD", 1), (6, b"\0\0", 2), (6, struct.pack("<H", 8193), 4),
        (11, b"\xFF", 2), (27, b"\x05", 2), (28, b"\x1C", 1),
        (32, b"\x01", 2), (34, b"\x03\0", 2), (42, b"\x18", 2),
        (43, b"\x01", 2), (44, b"\0", 2)):
        bad = valid[:offset] + replacement + valid[offset + len(replacement):]
        run(bad, result)
    run(header(1, 1) + b";", 2)
    run(header(1, 1) + b"\x21\x01\x0C" + bytes(12) + b"\0;", 1)
    # 未知扩展及注释可跳过；屏幕外区域和不足工作区不会触发绘制。
    compare(header(3, 2) + b"\x21\xFE" + blocks(b"hello" * 300) + valid[25:])
    for _ in range(150):
        bad = bytearray(valid)
        for __ in range(randomizer.randrange(1, 5)):
            bad[randomizer.randrange(len(bad))] = randomizer.randrange(256)
        run(bad, 99, max_frames=8)
    print("GIF decoder tests passed: %d cases" % count)
