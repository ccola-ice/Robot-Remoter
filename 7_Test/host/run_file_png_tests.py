"""逐像素核对 PNG 色型/位深/滤波/Adam7/压缩，并检查损坏输入及取消边界。"""
from app_test_paths import app_include_args
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import zlib

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"
TYPES = """
#include <stdint.h>
typedef unsigned int UINT;
typedef uint32_t DWORD;
"""
FF = """
#ifndef PNG_TEST_FF
#define PNG_TEST_FF
#include <stdio.h>
#include "types.h"
typedef struct { FILE *stream; DWORD size, offset; } FIL;
typedef enum { FR_OK = 0, FR_DISK_ERR } FRESULT;
#define FA_READ 1U
#define FA_OPEN_EXISTING 0U
#define f_size(f) ((f)->size)
FRESULT f_open(FIL *, const char *, unsigned);
FRESULT f_read(FIL *, void *, UINT, UINT *);
FRESULT f_close(FIL *);
#endif
"""
LCD = """
#include <stdint.h>
extern uint16_t LCD_X_LENGTH, LCD_Y_LENGTH;
void LCD_BlitRGB565(uint16_t, uint16_t, uint16_t, uint16_t, const uint16_t *);
"""
SIGNATURE = b"\x89PNG\r\n\x1a\n"
PASSES = ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4),
          (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2))


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def paeth(a, b, c):
    distances = (abs(b - c), abs(a - c), abs(a + b - 2 * c))
    return (a, b, c)[distances.index(min(distances))]


def png(width, height, color=2, depth=8, interlace=0, filter_kind=None,
        compression="dynamic", transparency=False, split=71, invalid_index=False):
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    max_value = (1 << depth) - 1
    palette = [(i * 53 & 255, i * 79 & 255, i * 113 & 255) for i in range(min(16, max_value + 1))]
    alphas = [0, 128, 255] + [255] * 253
    transparent = (max_value // 3,) * channels

    def sample(x, y):
        if color == 3:
            return ((x + y * 3) % len(palette) if not invalid_index else max_value,)
        if transparency and x % 3 == 0:
            return transparent
        return tuple((x * 43 + y * 59 + c * 71) % (max_value + 1) for c in range(channels))

    def expected(x, y):
        raw = sample(x, y)
        if color == 3:
            rgb = palette[raw[0]]
            alpha = alphas[raw[0]] if transparency else 255
        else:
            scaled = tuple(v * 255 // max_value for v in raw)
            rgb = (scaled[0],) * 3 if color in (0, 4) else scaled[:3]
            alpha = scaled[-1] if color in (4, 6) else 255
            if color in (0, 2) and transparency and raw == transparent:
                alpha = 0
        r, g, b = [(v * alpha + 255 * (255 - alpha) + 127) // 255 for v in rgb]
        return (r & 248) << 8 | (g & 252) << 3 | b >> 3

    raw = bytearray()
    for start_x, start_y, step_x, step_y in PASSES if interlace else ((0, 0, 1, 1),):
        if start_x >= width or start_y >= height:
            continue
        previous = None
        for y in range(start_y, height, step_y):
            samples = [v for x in range(start_x, width, step_x) for v in sample(x, y)]
            if depth == 16:
                row = struct.pack(">" + "H" * len(samples), *samples)
            elif depth == 8:
                row = bytes(samples)
            else:
                row = bytearray((len(samples) * depth + 7) // 8)
                for index, value in enumerate(samples):
                    row[index * depth // 8] |= value << (8 - depth - index * depth % 8)
            if previous is None:
                previous = bytes(len(row))
            kind = y % 5 if filter_kind is None else filter_kind
            pixel_bytes = (channels * depth + 7) // 8
            raw.append(kind)
            for at, value in enumerate(row):
                a = row[at - pixel_bytes] if at >= pixel_bytes else 0
                b = previous[at]
                c = previous[at - pixel_bytes] if at >= pixel_bytes else 0
                predictors = (0, a, b, (a + b) // 2, paeth(a, b, c))
                raw.append((value - predictors[kind]) & 255)
            previous = row
    compressor = zlib.compressobj(0 if compression == "stored" else 6,
                                  strategy=zlib.Z_FIXED if compression == "fixed" else zlib.Z_DEFAULT_STRATEGY)
    compressed = compressor.compress(raw) + compressor.flush()
    data = SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, depth, color, 0, 0, interlace))
    if color == 3:
        data += chunk(b"PLTE", bytes(v for rgb in palette for v in rgb))
    if transparency:
        if color == 3:
            data += chunk(b"tRNS", bytes(alphas[:len(palette)]))
        elif color in (0, 2):
            data += chunk(b"tRNS", struct.pack(">" + "H" * channels, *transparent))
    data += chunk(b"tEXt", b"fixture\0tests")
    for at in range(0, len(compressed), split):
        data += chunk(b"IDAT", compressed[at:at + split])
    data += chunk(b"IDAT", b"") + chunk(b"IEND", b"")
    return data, expected


def replace_chunk(data, kind, change):
    at = 8
    while at < len(data):
        size = struct.unpack_from(">I", data, at)[0]
        if data[at + 4:at + 8] == kind:
            return data[:at] + chunk(kind, change(data[at + 8:at + 8 + size])) + data[at + 12 + size:]
        at += size + 12
    raise AssertionError(kind)


with tempfile.TemporaryDirectory(prefix="remoter-png-") as temporary:
    folder = Path(temporary)
    for name, value in (("types.h", TYPES), ("ff.h", FF), ("bsp_fsmc_lcd.h", LCD)):
        (folder / name).write_text(value, encoding="ascii")
    exe = folder / "png-test.exe"
    subprocess.run([compiler, "-std=c99", "-O2", "-ftrapv", "-Wall", "-Wextra", "-Werror",
                    "-I", str(folder), *app_include_args(),
                    str(host / "file_png_test.c"), str(root / "1_App/files/image_png.c"), "-o", str(exe)], check=True)
    count = 0

    def run(data, expected=0, viewport=(24, 136, 752, 280), cancel=0, fail_read=0,
            fail_close=0, capacity=280576):
        global count
        path = folder / ("missing.png" if data is None else "fixture.png")
        if data is not None:
            path.write_bytes(data)
        output = folder / "pixels.bin"
        result = subprocess.run([str(exe), str(path), str(expected), *map(str, viewport),
                                 str(cancel), str(fail_read), str(fail_close), str(capacity),
                                 str(output)], check=True, capture_output=True, text=True)
        count += 1
        return tuple(map(int, result.stdout.split())), struct.unpack("<384000H", output.read_bytes())

    def verify(data, pixel, width, height, viewport=(24, 136, 752, 280)):
        info, frame = run(data, viewport=viewport)
        assert info[:2] == (width, height)
        dw, dh = info[2:4]
        dx, dy = viewport[0] + (viewport[2] - dw) // 2, viewport[1] + (viewport[3] - dh) // 2
        for y in range(dh):
            for x in range(dw):
                expected = pixel(x * width // dw, y * height // dh)
                assert frame[(dy + y) * 800 + dx + x] == expected, (info, x, y, expected)
        return info

    for color, depths in ((0, (1, 2, 4, 8, 16)), (2, (8, 16)), (3, (1, 2, 4, 8)),
                          (4, (8, 16)), (6, (8, 16))):
        for depth in depths:
            for interlace in (0, 1):
                for transparency in (False, True) if color in (0, 2, 3) else (False,):
                    for compression in ("stored", "fixed", "dynamic"):
                        data, pixel = png(37, 19, color, depth, interlace, transparency=transparency,
                                          compression=compression)
                        verify(data, pixel, 37, 19)
                        verify(data, pixel, 37, 19, (791, 473, 9, 7))
    for width, height in ((1, 1), (1, 33), (39, 1), (2, 2), (3, 5), (900, 411), (8192, 1)):
        for interlace in (0, 1):
            data, pixel = png(width, height, 6, 16, interlace)
            verify(data, pixel, width, height)
    for filter_kind in range(5):
        data, pixel = png(211, 127, filter_kind=filter_kind)
        verify(data, pixel, 211, 127)
    # 长流触发 32 KiB 字典回绕；每字节一个 IDAT 校验跨块字节与 CRC。
    data, pixel = png(256, 256, 2, 8, split=1)
    info = verify(data, pixel, 256, 256)
    run(data, 5, cancel=1)
    run(data, 5, cancel=20)
    run(data, 5, cancel=info[5] - 1)
    run(data, 3, fail_read=1)
    run(data, 3, fail_read=info[7])
    run(data, 3, fail_close=1)
    run(data, 6, capacity=100)
    run(data, 1, viewport=(799, 479, 2, 2))
    run(data, 1, viewport=(0, 0, 0, 2))
    run(None, 3)
    for length in (0, 1, 7, 8, 20, 32, len(data) - 1):
        run(data[:length], 2)
    run(b"not PNG data", 1)
    run(data + b"trailing", 2)
    run(replace_chunk(data, b"IHDR", lambda h: struct.pack(">I", 0) + h[4:]), 2)
    run(replace_chunk(data, b"IHDR", lambda h: struct.pack(">I", 9000) + h[4:]), 4)
    run(replace_chunk(data, b"IHDR", lambda h: h[:10] + b"\x01" + h[11:]), 1)
    run(replace_chunk(data, b"IHDR", lambda h: h[:8] + b"\x03" + h[9:]), 2)
    run(data[:33] + chunk(b"ABCD", b"") + data[33:], 1)
    run(data[:33] + chunk(b"IHDR", b"" ) + data[33:], 2)
    run(replace_chunk(data, b"IEND", lambda h: b"x"), 2)
    indexed, _ = png(17, 13, 3, 8, invalid_index=True)
    run(indexed, 2)
    valid, _ = png(17, 13, split=100000)
    # CRC 正确但 zlib 校验和错误，以及无效流头、输出过长/过短、非法滤波。
    run(replace_chunk(valid, b"IDAT", lambda d: d[:-1] + bytes((d[-1] ^ 1,))), 2)
    run(replace_chunk(valid, b"IDAT", lambda d: b"\0" + d[1:]), 2)
    run(replace_chunk(valid, b"IDAT", lambda d: zlib.compress(zlib.decompress(d) + b"x")), 2)
    run(replace_chunk(valid, b"IDAT", lambda d: zlib.compress(zlib.decompress(d)[:-1])), 2)
    run(replace_chunk(valid, b"IDAT", lambda d: zlib.compress(b"\x05" + zlib.decompress(d)[1:])), 2)
    run(replace_chunk(valid, b"IDAT", lambda d: d + b"x"), 2)
    # 文件内容 CRC 仍正确，强制走损坏的 DEFLATE 解码路径。
    for index in range(96):
        def mutate_stream(stream):
            changed = bytearray(stream)
            at = 2 + index % (len(changed) - 6)
            changed[at] ^= 1 << (index % 8)
            return changed
        run(replace_chunk(valid, b"IDAT", mutate_stream), 99)
    # 不连续的 IDAT、重复调色板、缺失调色板、非法透明键均应拒绝。
    split_data, _ = png(17, 13, split=1)
    at = split_data.index(b"IDAT") - 4
    run(split_data[:at + 13] + chunk(b"tEXt", b"gap\0x") + split_data[at + 13:], 2)
    indexed, _ = png(17, 13, 3, 4, transparency=True)
    at = indexed.index(b"PLTE") - 4
    length = struct.unpack_from(">I", indexed, at)[0] + 12
    run(indexed[:at] + indexed[at + length:], 2)
    run(indexed[:at] + indexed[at:at + length] + indexed[at:], 2)
    gray, _ = png(17, 13, 0, 1, transparency=True)
    run(replace_chunk(gray, b"tRNS", lambda _: b"\0\x02"), 2)
    random_source = random.Random(0x504E47)
    for index in range(96):
        bad = bytearray(valid)
        at = random_source.randrange(8, len(bad))
        bad[at] ^= 1 << random_source.randrange(8)
        run(bad, 99)
    print(f"PNG: {count} cases passed; all standard color/depth modes, Adam7, filters, DEFLATE, CRC/Adler, bounds and cancellation.")
