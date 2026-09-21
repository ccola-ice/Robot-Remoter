"""Verify the production font asset, coverage manifest and pure lookup API."""
from pathlib import Path
import ast
import hashlib
import json
import re
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
font_dir = root / "5_ModuleDrivers/fonts"
manifest = json.loads((font_dir / "ui_font_manifest.json").read_text(encoding="utf8"))
codes = manifest["codes"]
assert codes == sorted(set(codes))
assert manifest["bitmap_bytes"] < 600*1024
assert manifest["code_index_bytes"] == len(codes)*2
for path in (root / "1_App").glob("*.[ch]"):
    for token in re.findall(r'"(?:[^"\\]|\\.)*"',path.read_bytes().decode("latin1")):
        try:
            value=ast.literal_eval(token).encode("latin1").decode("gb2312")
        except (UnicodeError,SyntaxError,ValueError):
            continue
        for char in value:
            if ord(char)>127:
                assert int.from_bytes(char.encode("gb2312"),"big") in codes, (path,char)
for char in "星期一二三四五六日未测试\u3000":
    assert int.from_bytes(char.encode("gb2312"),"big") in codes

source=(font_dir/"ui_font_data.inc").read_text(encoding="utf8")
total=0
for name, description in manifest["blobs"].items():
    match=re.search(r'static const uint8_t '+name+r'\[(\d+)\] = \{(.*?)\};',source,re.S)
    assert match, name
    data=bytes(int(value,16) for value in re.findall(r'0x([0-9a-f]{2})',match[2]))
    size=int(name.rsplit('_',1)[1]); width=size//2 if "ascii" in name else size
    glyph_count=95 if "ascii" in name else len(codes)
    assert len(data)==int(match[1])==description["bytes"]==glyph_count*((width+3)//4)*size
    assert hashlib.sha256(data).hexdigest()==description["sha256"]
    total+=len(data)
assert total==manifest["bitmap_bytes"]
assert all(metric["origins"][face][1]==metric["baseline"]
           for metric in manifest["metrics"] for face in metric["origins"])
licenses=(font_dir/"UI_FONT_LICENSES.txt").read_text(encoding="utf8")
assert "SIL OPEN FONT LICENSE Version 1.1" in licenses and "Bitstream Vera Fonts Copyright" in licenses
assert all(source["sha256"] and source["13"] for source in manifest["sources"])

with tempfile.TemporaryDirectory(prefix="remoter-native-font-") as folder:
    temporary=Path(folder)
    (temporary/"ui_font_expected.inc").write_text("static const uint16_t expected_codes[] = {"+
        ",".join("0x%04xU"%code for code in codes)+"};\n",encoding="ascii")
    exe=temporary/"native-font-test.exe"
    subprocess.run([sys.argv[1] if len(sys.argv)>1 else "gcc","-std=c99","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(font_dir),"-I",str(temporary),str(host/"ui_font_data_test.c"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("Native asset manifest: %u bitmap + %u index bytes; source coverage, hashes, common baselines and licenses passed"%
      (total,manifest["code_index_bytes"]))
