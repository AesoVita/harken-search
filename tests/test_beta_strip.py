"""
test_beta_strip.py — 内测版阉割验证（静态扫描）

验证目标：
  用 pack_beta.bat 或等价 cmake flags（-DAURA_INTERNAL_BETA=1）编出的
  build_beta/Release/Auro.exe 里，所有开发者专用的字符串字面量应被剔除。

  这是对 T1-T6 #if 门控是否生效的最后一道保险：如果 #if 漏写或拼错
  (#if AURA_INTERNAL_BETA vs #if !AURA_INTERNAL_BETA)，下面的字符串会
  泄漏到最终 exe 的 .rdata 段，被这个扫描命中。

用法：
  python tests/test_beta_strip.py [path/to/Auro.exe]

  默认扫描：build_beta/Release/Auro.exe

退出码：
  0 = PASS
  1 = FAIL（命中某个禁止字符串）
  2 = 没找到 exe
"""

from __future__ import annotations

import sys
from pathlib import Path

# 这些字符串在 AURA_INTERNAL_BETA=1 的构建里绝不应该出现
FORBIDDEN_STRINGS: list[tuple[str, str]] = [
    ("qa_cmd.json",              "QA Driver 文件轮询（T3）"),
    ("qa_result.json",           "QA Driver 响应文件（T3）"),
    ("dev_mode.txt",             "dev_mode.txt 旁路（T4/T5）"),
    ("theme_path_trace.log",     "游离主题日志（T6）"),
    ("Game Master Debug Panel",  "GM 按钮 tooltip（T2）"),
    ("settings_dev.ini",         "开发模式专用配置（T4 副作用）"),
    ("imgui_dev.ini",            "开发模式专用 ImGui 配置（T4 副作用）"),
]


def scan(exe_path: Path) -> int:
    if not exe_path.exists():
        print(f"[SKIP] 未找到 exe：{exe_path}")
        print("       先运行：pack_beta.bat  或  cmake --build build_beta --config Release")
        return 2

    data = exe_path.read_bytes()
    size_mb = len(data) / (1024 * 1024)
    print(f"扫描文件：{exe_path}  ({size_mb:.1f} MB)")
    print("-" * 60)

    hits: list[tuple[str, str]] = []
    for needle, why in FORBIDDEN_STRINGS:
        # 同时检查 utf-8 和 utf-16 两种编码（Windows exe 两种都可能）
        buf_utf8 = needle.encode("utf-8")
        buf_utf16 = needle.encode("utf-16-le")
        if buf_utf8 in data or buf_utf16 in data:
            hits.append((needle, why))
            print(f"  [FAIL] '{needle}'  —  {why}")
        else:
            print(f"  [ OK ] '{needle}'  (已剔除)")

    print("-" * 60)
    if hits:
        print(f"FAIL: {len(hits)}/{len(FORBIDDEN_STRINGS)} 个开发者字符串仍残留在 exe 中。")
        print("      这说明某处 #if AURA_INTERNAL_BETA 门控失效，或")
        print("      构建时未传入 -DAURA_INTERNAL_BETA=1。")
        return 1

    print(f"PASS: {len(FORBIDDEN_STRINGS)}/{len(FORBIDDEN_STRINGS)} 个开发者字符串已剔除。")
    return 0


def main() -> int:
    if len(sys.argv) >= 2:
        exe = Path(sys.argv[1])
    else:
        # 默认路径（假设从项目根运行）
        exe = Path("build_beta") / "Release" / "Auro.exe"

    return scan(exe)


if __name__ == "__main__":
    sys.exit(main())
