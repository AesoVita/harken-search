# -*- coding: utf-8 -*-
"""RQ-20260915-01: 启动页/欢迎页品牌字与 macOS 窗口标题必须显示 Harken Search。"""

from __future__ import annotations

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
WINDOWS_MAIN = ROOT / "src" / "WindowsMain.cpp"
MACOS_ENTRY = ROOT / "src" / "platform" / "app_entry_macos.mm"

EXPECTED_BRAND = "Harken Search"
EXPECTED_CLAMP = "clamp(22px,1.8vw,32px)"
EXPECTED_WEIGHT = "700"
EXPECTED_TRACKING = "-0.01em"
EXPECTED_MOBILE_SIZE = "20px"

BRAND_WORD_CSS_RE = re.compile(
    r"\.brand-word\{[^}]*font-size:clamp\([^}]+\}"
)
MOBILE_BRAND_RE = re.compile(
    r"@media \(max-width:640px\)\{\.brand-word\{([^}]+)\}"
)
BRAND_HTML_RE = re.compile(
    r'<div class="brand-word">([^<]*)</div>'
)
TITLE_RE = re.compile(r"<title>([^<]*)</title>")
MACOS_TITLE_RE = re.compile(r'setTitle:\s*@"([^"]*)"')


def _read(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8")


def _css_field(rule: str, name: str) -> str | None:
    match = re.search(rf"{re.escape(name)}:([^;}}]+)", rule)
    if not match:
        return None
    return match.group(1).strip()


def _function_definition(source: str, name: str) -> str:
    marker = "static std::wstring %s(" % name
    start = 0
    while True:
        idx = source.find(marker, start)
        if idx < 0:
            return ""
        rest = source[idx:]
        brace = rest.find("{")
        semi = rest.find(";")
        if brace >= 0 and (semi < 0 or brace < semi):
            return rest
        start = idx + len(marker)


def _split_loading_and_welcome(source: str) -> tuple[str, str]:
    loading = _function_definition(source, "BuildFlashUiLoadingHtml")
    welcome = _function_definition(source, "BuildFlashUiWelcomeHtml")
    if loading and welcome:
        loading_end = source.find(welcome) if welcome in source else len(source)
        loading_start = source.find(loading)
        if 0 <= loading_start < loading_end:
            loading = source[loading_start:loading_end]
    return loading, welcome


class BrandWordHarkenSearchContractTests(unittest.TestCase):
    def test_loading_page_brand_word_css_matches_frozen_design(self) -> None:
        loading, _ = _split_loading_and_welcome(_read(WINDOWS_MAIN))
        rules = BRAND_WORD_CSS_RE.findall(loading)
        self.assertEqual(1, len(rules), "expected one loading .brand-word clamp rule")
        rule = rules[0]
        self.assertEqual(EXPECTED_CLAMP, _css_field(rule, "font-size"))
        self.assertEqual(EXPECTED_WEIGHT, _css_field(rule, "font-weight"))
        self.assertEqual(EXPECTED_TRACKING, _css_field(rule, "letter-spacing"))
        self.assertNotIn("text-transform:lowercase", rule)

    def test_loading_page_brand_word_and_title_are_harken_search(self) -> None:
        loading, _ = _split_loading_and_welcome(_read(WINDOWS_MAIN))
        html = BRAND_HTML_RE.findall(loading)
        self.assertEqual([EXPECTED_BRAND], html)
        titles = TITLE_RE.findall(loading)
        self.assertIn(EXPECTED_BRAND, titles)
        self.assertNotIn("ARIS Loading", titles)
        self.assertNotIn("aris", html)

    def test_welcome_page_brand_word_css_matches_frozen_design(self) -> None:
        _, welcome = _split_loading_and_welcome(_read(WINDOWS_MAIN))
        rules = BRAND_WORD_CSS_RE.findall(welcome)
        self.assertEqual(1, len(rules), "expected one welcome .brand-word clamp rule")
        rule = rules[0]
        self.assertEqual(EXPECTED_CLAMP, _css_field(rule, "font-size"))
        self.assertEqual(EXPECTED_WEIGHT, _css_field(rule, "font-weight"))
        self.assertEqual(EXPECTED_TRACKING, _css_field(rule, "letter-spacing"))
        self.assertNotIn("text-transform:lowercase", rule)

    def test_welcome_page_brand_word_and_title_are_harken_search(self) -> None:
        _, welcome = _split_loading_and_welcome(_read(WINDOWS_MAIN))
        html = BRAND_HTML_RE.findall(welcome)
        self.assertEqual([EXPECTED_BRAND], html)
        titles = TITLE_RE.findall(welcome)
        self.assertIn(EXPECTED_BRAND, titles)
        self.assertNotIn("ARIS Welcome", titles)
        self.assertNotIn("aris", html)

    def test_welcome_mobile_breakpoint_brand_word_is_20px(self) -> None:
        _, welcome = _split_loading_and_welcome(_read(WINDOWS_MAIN))
        matches = MOBILE_BRAND_RE.findall(welcome)
        self.assertEqual(1, len(matches), "expected one welcome mobile .brand-word rule")
        self.assertEqual(EXPECTED_MOBILE_SIZE, _css_field("{" + matches[0] + "}", "font-size"))

    def test_macos_window_title_is_harken_search(self) -> None:
        text = _read(MACOS_ENTRY)
        titles = MACOS_TITLE_RE.findall(text)
        self.assertEqual([EXPECTED_BRAND], titles)


if __name__ == "__main__":
    unittest.main()
