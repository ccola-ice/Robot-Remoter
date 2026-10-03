"""验证直接寻址表、完整码点映射以及原始 CP936 转换结果的 SHA-256。"""

import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
FATFS = ROOT / "3_Protocol/FatFs"
# 来源：改动前双表 cc936.c；按码点 0..65535，各输出 convert(0)、convert(1)、wtoupper 的 uint16 小端值。
GOLDEN_SHA256 = "d67454da6a00072fa18fde127f5af95ce86b1a33df36fab83ac189657c436530"


def read_revision_file(revision, name):
    return subprocess.check_output([
        "git", "-c", "safe.directory=" + ROOT.as_posix(), "show",
        revision + ":3_Protocol/FatFs/" + name,
    ], cwd=ROOT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", nargs="?", default="gcc")
    parser.add_argument("--reference-revision", help="额外编译指定 Git 版本的独立旧实现，例如改动前的 HEAD")
    args = parser.parse_args()
    subprocess.run([sys.executable, str(FATFS / "generate_cc936_table.py"), "--check"], check=True)
    with tempfile.TemporaryDirectory(prefix="remoter-cc936-") as directory:
        temporary = Path(directory)
        executable = temporary / "cc936-test.exe"
        results = temporary / "cc936-results.bin"
        sources = [str(FATFS / "cc936.c"), str(ROOT / "7_Test/host/cc936_test.c")]
        options = []
        if args.reference_revision:
            original = read_revision_file(args.reference_revision, "cc936.c")
            current = (FATFS / "cc936_mapping.inc").read_bytes()
            pair_table = rb"static\s+const WCHAR uni2oem\[\] = \{.*?\};"
            reference_mapping = original
            if not re.search(pair_table, original, re.S):
                # 新版本将原始配对表独立保存，仍可按任意已提交版本验证。
                reference_mapping = read_revision_file(args.reference_revision, "cc936_mapping.inc")
            assert re.search(pair_table, reference_mapping.replace(b"\r\n", b"\n"), re.S).group(0) == \
                re.search(pair_table, current.replace(b"\r\n", b"\n"), re.S).group(0), \
                "Unicode-to-OEM source table changed"
            original = re.sub(rb"\bff_convert\b", b"old_ff_convert", original)
            original = re.sub(rb"\bff_wtoupper\b", b"old_ff_wtoupper", original)
            reference = temporary / "cc936-reference.c"
            reference.write_bytes(original)
            sources.append(str(reference))
            options.append("-DCC936_COMPARE_REFERENCE")
        subprocess.run([
            args.compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I", str(FATFS), *options, *sources, "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable), str(results)], check=True, timeout=30)
        data = results.read_bytes()
        if len(data) != 65536 * 3 * 2:
            raise AssertionError("转换结果长度错误")
        digest = hashlib.sha256(data).hexdigest()
        if digest != GOLDEN_SHA256:
            raise AssertionError("CP936 golden SHA-256 mismatch: " + digest)
        print("CP936 original output SHA-256 verified: " + digest)


if __name__ == "__main__":
    main()
