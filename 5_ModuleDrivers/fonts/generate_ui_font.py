"""Generate Remoter UI native 2bpp cells from licensed local outline fonts.

Requires Pillow 11.3.0. No font binary or host rasterizer is linked into firmware.
Each size is rasterized independently; no bitmap resize, dilation or resampling.
"""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import re
import struct
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
DEFAULT_PILLOW = ROOT / "VSCode EIDE Project/build/ui-clarity/python"
SIZES = (16, 20, 32, 48)


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pillow-path", type=Path, default=DEFAULT_PILLOW)
    parser.add_argument("--cjk-font", type=Path, default=Path("C:/Windows/Fonts/NotoSansSC-VF.ttf"))
    parser.add_argument("--ascii-font", type=Path, default=Path("C:/Windows/Fonts/DejaVuSansMono_0.ttf"))
    parser.add_argument("--ascii-bold-font", type=Path, default=Path("C:/Windows/Fonts/DejaVuSansMono-Bold_0.ttf"))
    parser.add_argument("--output", type=Path, default=HERE)
    parser.add_argument("--preview", type=Path)
    parser.add_argument("--check", action="store_true", help="Compare generated products; do not rewrite them")
    return parser.parse_args()


def source_codes():
    codes = set()
    for path in sorted((ROOT / "1_App").glob("*.[ch]")):
        text = path.read_bytes().decode("latin1")
        for match in re.finditer(r'"(?:[^"\\]|\\.)*"', text):
            try:
                value = ast.literal_eval(match[0]).encode("latin1").decode("gb2312")
            except (UnicodeError, SyntaxError, ValueError):
                continue
            for char in value:
                if ord(char) > 127:
                    encoded = char.encode("gb2312")
                    if len(encoded) == 2:
                        codes.add(int.from_bytes(encoded, "big"))
    # Service pages and calendar weekday labels may be assembled dynamically.
    for char in "星期一二三四五六日未测试\u3000":
        codes.add(int.from_bytes(char.encode("gb2312"), "big"))
    return sorted(codes)


def font_metadata(path):
    data = path.read_bytes()
    count = struct.unpack_from(">H", data, 4)[0]
    tables = {}
    for i in range(count):
        tag, _, offset, length = struct.unpack_from(">4sIII", data, 12 + 16*i)
        tables[tag] = (offset, length)
    offset = tables[b"name"][0]
    _, count, strings = struct.unpack_from(">HHH", data, offset)
    names = {}
    for i in range(count):
        platform, _, _, name_id, length, position = struct.unpack_from(">HHHHHH", data, offset+6+12*i)
        if name_id not in (0, 1, 2, 5, 13, 14):
            continue
        raw = data[offset+strings+position:offset+strings+position+length]
        value = raw.decode("utf-16-be" if platform in (0, 3) else "mac_roman")
        names.setdefault(name_id, set()).add(value)
    result = {str(k): sorted(values) for k, values in names.items()}
    result["file"] = path.name
    result["sha256"] = hashlib.sha256(data).hexdigest()
    result["bytes"] = len(data)
    return result


def cjk_coverage(path, codes):
    """Reject .notdef substitutions before rasterizing the interface subset."""
    data = path.read_bytes()
    for i in range(struct.unpack_from(">H", data, 4)[0]):
        tag, _, offset, _ = struct.unpack_from(">4sIII", data, 12+16*i)
        if tag == b"cmap":
            break
    else:
        raise ValueError("Source font has no cmap")
    groups = []
    for i in range(struct.unpack_from(">H", data, offset+2)[0]):
        _, _, relative = struct.unpack_from(">HHI", data, offset+4+8*i)
        start = offset+relative
        if struct.unpack_from(">H", data, start)[0] == 12:
            groups.extend(struct.unpack_from(">III", data, start+16+12*j)
                          for j in range(struct.unpack_from(">I", data, start+12)[0]))
    for code in codes:
        char = bytes((code >> 8, code & 255)).decode("gb2312")
        if not any(first <= ord(char) <= last and glyph+ord(char)-first
                   for first, last, glyph in groups):
            raise ValueError("Source font lacks U+%04X" % ord(char))


def glyph_bounds(font, chars):
    boxes = [font.getbbox(char, anchor="ls") for char in chars]
    return (min(b[0] for b in boxes), min(b[1] for b in boxes),
            max(b[2] for b in boxes), max(b[3] for b in boxes))


def load_face(path, pixels, weight=None):
    font = ImageFont.truetype(str(path), pixels, layout_engine=ImageFont.Layout.BASIC)
    if weight is not None:
        font.set_variation_by_axes([weight])
    return font


def native_faces(size, cjk_chars, args):
    """Fit a whole face, never individual characters; all share one baseline.

    Outline em size and fixed cell height are different metrics. DejaVu Mono's
    advance is approximately 0.60em, so an 8px cell needs a 13px outline em.
    Fit all printable ASCII and all Chinese labels together, including the
    32px bold faces, before rasterization. This preserves digits' baselines,
    punctuation/descenders, and every cell's original layout dimensions.
    """
    ascii_chars = [chr(i) for i in range(32, 127)]
    cjk_px = ascii_px = size
    while True:
        faces = {
            "cjk": load_face(args.cjk_font, cjk_px, 500),
            "ascii": load_face(args.ascii_font, ascii_px),
        }
        if size == 32:
            faces["cjk_bold"] = load_face(args.cjk_font, cjk_px, 650)
            faces["ascii_bold"] = load_face(args.ascii_bold_font, ascii_px)
        bounds = {name: glyph_bounds(face, cjk_chars if name.startswith("cjk") else ascii_chars)
                  for name, face in faces.items()}
        bad_cjk = any(b[2]-b[0] > size for name, b in bounds.items() if name.startswith("cjk"))
        bad_ascii = any(b[2]-b[0] > size//2 for name, b in bounds.items() if name.startswith("ascii"))
        top = min(b[1] for b in bounds.values())
        bottom = max(b[3] for b in bounds.values())
        if not bad_cjk and not bad_ascii and bottom-top <= size:
            break
        if bad_cjk:
            cjk_px -= 1
        if bad_ascii:
            ascii_px -= 1
        if not bad_cjk and not bad_ascii:
            # The wider CJK line generally determines the total vertical span.
            if min(b[1] for n, b in bounds.items() if n.startswith("cjk")) <= \
                    min(b[1] for n, b in bounds.items() if n.startswith("ascii")):
                cjk_px -= 1
            else:
                ascii_px -= 1
        if min(cjk_px, ascii_px) < 1:
            raise ValueError("Could not fit face into fixed cell")
    baseline = (size-(bottom-top))//2-top
    origins = {}
    # Common left origin for regular/bold within each alphabet.
    for alphabet, width in (("cjk", size), ("ascii", size//2)):
        boxes = [b for name, b in bounds.items() if name.startswith(alphabet)]
        left, right = min(b[0] for b in boxes), max(b[2] for b in boxes)
        origin = (width-(right-left))//2-left
        for name in faces:
            if name.startswith(alphabet):
                origins[name] = (origin, baseline)
    metrics = {"cell": size, "baseline": baseline, "cjk_outline_px": cjk_px,
               "ascii_outline_px": ascii_px, "origins": origins}
    return faces, metrics


def bitmap(font, char, width, height, origin):
    # Draw on an expanded temporary canvas, then prove no ink is being clipped.
    margin = height
    canvas = Image.new("L", (width+2*margin, height+2*margin))
    ImageDraw.Draw(canvas).text((origin[0]+margin, origin[1]+margin), char,
                               font=font, fill=255, anchor="ls")
    bounds = canvas.getbbox()
    if bounds and not (bounds[0] >= margin and bounds[1] >= margin and
                       bounds[2] <= margin+width and bounds[3] <= margin+height):
        raise ValueError("Clipped glyph U+%04X: %r" % (ord(char), bounds))
    cell = canvas.crop((margin, margin, margin+width, margin+height))
    pixels = [(value+42)//85 for value in cell.getdata()]
    stride = (width+3)//4
    packed = bytearray(stride*height)
    for y in range(height):
        for x in range(width):
            packed[y*stride+x//4] |= pixels[y*width+x] << (6-2*(x%4))
    return bytes(packed)


def c_array(name, data):
    lines = ["static const uint8_t %s[%u] = {" % (name, len(data))]
    lines.extend("    " + ",".join("0x%02x" % b for b in data[i:i+24]) + ","
                 for i in range(0, len(data), 24))
    lines.append("};")
    return "\n".join(lines)


def implementation(codes, blobs):
    parts = ["/* Generated Remoter UI derived font assets. Do not edit by hand.",
             " * Regenerate with generate_ui_font.py; see ui_font_manifest.json",
             " * for source hashes and metrics; UI_FONT_LICENSES.txt for licenses.",
             " * Include in fonts.c exactly once. Native coverage: 0,1,2,3.",
             " */", '#include "ui_font_data.h"', "#include <stddef.h>",
             "static const uint16_t ui_font_codes[%u] = {" % len(codes)]
    parts.extend("    " + ",".join("0x%04xU" % code for code in codes[i:i+12]) + ","
                 for i in range(0, len(codes), 12))
    parts.append("};")
    for name, data in blobs.items():
        parts.append(c_array(name, data))
    parts.extend([
        "const uint8_t *UI_FontBitmap(uint16_t code, uint16_t size, uint8_t bold)",
        "{", "    const uint8_t *data;", "    uint32_t glyph, width, stride;",
        "    uint16_t low, high, middle;", "    uint8_t ascii;",
        "    if(size != 16U && size != 20U && size != 32U && size != 48U) return NULL;",
        "    ascii = code >= 32U && code <= 126U;",
        "    if(ascii) glyph = (uint32_t)code - 32U;",
        "    else {",
        "        if((code >> 8) < 0xa1U || (code >> 8) > 0xf7U ||",
        "           (code & 255U) < 0xa1U || (code & 255U) > 0xfeU) return NULL;",
        "        low = 0U; high = (uint16_t)(sizeof(ui_font_codes)/sizeof(ui_font_codes[0]));",
        "        while(low < high) {",
        "            middle = (uint16_t)(low + (high-low)/2U);",
        "            if(ui_font_codes[middle] < code) low = (uint16_t)(middle+1U);",
        "            else high = middle;", "        }",
        "        if(low >= sizeof(ui_font_codes)/sizeof(ui_font_codes[0]) || ui_font_codes[low] != code) return NULL;",
        "        glyph = low;", "    }",
        "    switch(size) {",
        "    case 16U: data = ascii ? ui_font_ascii_16 : ui_font_cjk_16; break;",
        "    case 20U: data = ascii ? ui_font_ascii_20 : ui_font_cjk_20; break;",
        "    case 48U: data = ascii ? ui_font_ascii_48 : ui_font_cjk_48; break;",
        "    default:",
        "        data = ascii ? (bold ? ui_font_ascii_bold_32 : ui_font_ascii_32) :",
        "                       (bold ? ui_font_cjk_bold_32 : ui_font_cjk_32);",
        "        break;", "    }", "    width = ascii ? size/2U : size;",
        "    stride = (width+3U)/4U;", "    return data + glyph*stride*size;", "}", ""])
    return "\n".join(parts)


def write_or_check(path, contents, check):
    data = contents.encode("utf8")
    if check:
        if not path.exists() or path.read_bytes() != data:
            raise ValueError("Generated file is stale: %s" % path)
    else:
        path.write_bytes(data)


def preview(path, codes, blobs):
    canvas = Image.new("RGB", (800, 492), (240,244,248))
    lines = [(16,0,"\u673a\u5668\u4eba\u9065\u63a7\u5668  \u65e5\u5386\u4e0e\u65f6\u949f  2026-09-21 12:34:56"),
             (20,0,"\u672a\u6d4b\u8bd5  \u901a\u8fc7  \u5931\u8d25  \u7535\u538b 3.70V  100%"),
             (32,0,"\u53c2\u6570\u8bbe\u7f6e  CH1  -1000  4095"),
             (32,1,"\u53c2\u6570\u8bbe\u7f6e  CH1  -1000  4095"),
             (48,0,"\u65e5\u5386\u4e0e\u65f6\u949f  012345"),
             (16,0,"0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz"),
             (20,0,"0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz"),
             (32,0,"0123456789 AaBbWwGgjpqy @_[]{}"),
             (32,1,"0123456789 AaBbWwGgjpqy @_[]{}")]
    y = 16
    for size, bold, text in lines:
        x = 20
        for char in text:
            ascii = ord(char) < 128
            width = size//2 if ascii else size
            if x+width > 780: break
            code = ord(char) if ascii else int.from_bytes(char.encode("gb2312"),"big")
            index = code-32 if ascii else codes.index(code)
            name = "ui_font_%s%s_%u" % ("ascii" if ascii else "cjk", "_bold" if bold and size==32 else "", size)
            stride = (width+3)//4
            data = blobs[name][index*stride*size:(index+1)*stride*size]
            for row in range(size):
                for col in range(width):
                    alpha = (data[row*stride+col//4] >> (6-2*(col%4))) & 3
                    canvas.putpixel((x+col,y+row), tuple((fg*alpha+bg*(3-alpha)+1)//3 for fg,bg in zip((28,44,64),(240,244,248))))
            x += width
        y += size+16
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def main():
    args = arguments()
    sys.path.insert(0, str(args.pillow_path))
    global Image, ImageDraw, ImageFont
    from PIL import Image, ImageDraw, ImageFont, __version__
    if __version__ != "11.3.0":
        raise ValueError("Use pinned Pillow 11.3.0 for reproducible assets")
    codes = source_codes()
    cjk_coverage(args.cjk_font, codes)
    chars = [bytes((c >> 8,c & 255)).decode("gb2312") for c in codes]
    metadata = [font_metadata(p) for p in (args.cjk_font,args.ascii_font,args.ascii_bold_font)]
    if "SIL Open Font License" not in " ".join(metadata[0]["13"]):
        raise ValueError("Expected licensed Noto source")
    if any("Bitstream" not in " ".join(m["13"]) for m in metadata[1:]):
        raise ValueError("Expected licensed DejaVu sources")
    blobs, metrics = {}, []
    for size in SIZES:
        faces, metric = native_faces(size, chars, args)
        metrics.append(metric)
        for name, font in faces.items():
            cjk = name.startswith("cjk")
            glyphs = chars if cjk else [chr(i) for i in range(32,127)]
            width = size if cjk else size//2
            blobs["ui_font_%s_%u" % (name,size)] = b"".join(
                bitmap(font, char, width, size, metric["origins"][name]) for char in glyphs)
    amount = sum(map(len, blobs.values()))
    if amount >= 600*1024:
        raise ValueError("Bitmap budget exceeded: %u" % amount)
    manifest = {"derived_name":"Remoter UI", "format":"row-major 2bpp MSB-first; zero row padding",
                "sizes":SIZES,"cjk_regular_weight":500,"cjk_bold_weight":650,
                "pillow":__version__,"sources":metadata,"codes":codes,"metrics":metrics,
                "bitmap_bytes":amount,"code_index_bytes":2*len(codes),
                "blobs":{name:{"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()} for name,data in blobs.items()}}
    licenses = "Remoter UI derived font assets\n=============================\n\n" \
        "Only generated bitmap subsets are distributed, not the source TTF files.\n" \
        "The fonts retain their licenses independently of firmware source code.\n" \
        "Source files, versions and SHA256: ui_font_manifest.json.\n\n" \
        "Noto Sans SC source: https://github.com/google/fonts/tree/main/ofl/notosanssc\n" \
        + "\n".join(metadata[0]["0"]) + "\n\n" + (HERE/"ui_font_OFL.txt").read_text(encoding="utf8") \
        + "\n\nDejaVu Sans Mono / DejaVu Sans Mono Bold\n" \
        "Source: https://dejavu-fonts.github.io/License.html\n\n" \
        + "\n".join(metadata[1]["13"]) + "\n"
    if not args.check: args.output.mkdir(parents=True, exist_ok=True)
    write_or_check(args.output/"ui_font_data.inc",implementation(codes,blobs),args.check)
    write_or_check(args.output/"ui_font_manifest.json",json.dumps(manifest,ensure_ascii=True,indent=2)+"\n",args.check)
    write_or_check(args.output/"UI_FONT_LICENSES.txt",licenses,args.check)
    if args.preview and not args.check: preview(args.preview,codes,blobs)
    print("Remoter UI: %u Chinese + 95 ASCII; %u bitmap bytes + %u index bytes" % (len(codes),amount,2*len(codes)))
    for metric in metrics: print(metric)


if __name__ == "__main__":
    main()
