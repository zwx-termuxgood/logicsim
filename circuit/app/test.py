#!/usr/bin/env python3
"""
删除文件中包含 "// 原样" 内容的所有行。

用法:
    python remove_lines.py 目标文件.txt
    python remove_lines.py 目标文件.txt -o 输出文件.txt
    python remove_lines.py 目标文件.txt --inplace   # 直接修改原文件
"""

import argparse
import sys
from pathlib import Path

MARKER = "// 原样"


def process(src: Path, dst: Path | None = None, inplace: bool = False) -> None:
    # 读取原文件全部行（保留换行符，便于原样写回）
    lines = src.read_text(encoding="utf-8").splitlines(keepends=True)

    # 过滤掉包含标记的行
    kept = [ln for ln in lines if MARKER not in ln]

    removed = len(lines) - len(kept)

    # 决定输出目标
    if inplace:
        out = src
    elif dst is not None:
        out = dst
    else:
        out = src.with_name(src.stem + "_cleaned" + src.suffix)

    # 写回结果
    out.write_text("".join(kept), encoding="utf-8")

    print(f"共 {len(lines)} 行，删除 {removed} 行，保留 {len(kept)} 行 -> {out}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description='删除文件中包含 "// 原样" 的所有行'
    )
    parser.add_argument("file", help="待处理文件路径")
    parser.add_argument("-o", "--output", help="输出文件路径（默认: 原文件名_cleaned.扩展名）")
    parser.add_argument("--inplace", action="store_true", help="直接修改原文件")
    args = parser.parse_args()

    src = Path(args.file)
    if not src.is_file():
        print(f"错误: 文件不存在 -> {src}", file=sys.stderr)
        return 1

    dst = Path(args.output) if args.output else None
    try:
        process(src, dst, args.inplace)
    except OSError as e:
        print(f"错误: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
