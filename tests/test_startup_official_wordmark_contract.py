"""RQ-20261020-10: both startup pages use the official packaged wordmark."""

from pathlib import Path
import hashlib
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
ASSET = ROOT / "assets" / "logos" / "brand" / "harken-search-menu-wordmark-white.png"
OFFICIAL_SHA256 = "f64332d4e34ed6c7bc1a315c8f4706a86c5556e2887aa7831b1715072e41fb80"


class StartupOfficialWordmarkContract(unittest.TestCase):
    def test_official_asset_is_packaged_and_loaded_locally(self) -> None:
        self.assertTrue(ASSET.is_file())
        self.assertEqual(OFFICIAL_SHA256, hashlib.sha256(ASSET.read_bytes()).hexdigest())
        self.assertIn('copy_directory', CMAKE)
        self.assertIn('assets/logos/brand', CMAKE)
        self.assertTrue('harken-search-menu-wordmark-white.png' in SOURCE, 'startup must read the official logo')
        self.assertTrue('BuildStartupBrandLogoDataUri' in SOURCE, 'startup logo reader is absent')

    def test_both_pages_use_image_with_text_fallback(self) -> None:
        loading = SOURCE.split('static std::wstring BuildFlashUiLoadingHtml(', 1)[1].split('// Beta Splash Screen', 1)[0]
        welcome = SOURCE.split('static std::wstring BuildFlashUiWelcomeHtml(', 2)[-1]
        self.assertTrue('.brand-word{background-image:url(' in SOURCE, 'official logo CSS is absent')
        for page in (loading, welcome):
            self.assertTrue('AppendStartupBrandLogoCss(html, useLightStartupChrome)' in page, 'page does not use official logo')
            self.assertTrue('<div class="brand-word">Harken Search</div>' in page, 'text fallback is absent')


if __name__ == "__main__":
    unittest.main()
