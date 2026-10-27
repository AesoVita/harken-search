from pathlib import Path
import inspect

import pytest

from tools.verify_release_assembly import verify_release_assembly


def _source(root: Path, *, show_welcome: bool) -> Path:
    src = root / "src"
    src.mkdir(parents=True)
    (src / "BetaFeatureFlags.h").write_text(
        f"inline constexpr bool kShowStartupLogin = {'true' if show_welcome else 'false'};\n"
        "inline constexpr bool kUseBetaSplashLoading = false;\n",
        encoding="utf-8",
    )
    (src / "WindowsMain.cpp").write_text(
        "else if (!kShowStartupLogin && kUseBetaSplash) { }\n"
        "RunWebView2LoginScreen(hwnd, settings, &controller, &view, &loader);\n"
        "RunWebView2LoadingScreen(hwnd, state, settings, controller, view, load, finish);\n",
        encoding="utf-8",
    )
    return root


def test_release_gate_rejects_disabled_welcome_source(tmp_path: Path) -> None:
    if "source_root" not in inspect.signature(verify_release_assembly).parameters:
        pytest.fail("release verifier has no source_root gate for the startup source")
    source = _source(tmp_path / "bad-source", show_welcome=False)
    issues = verify_release_assembly(tmp_path / "package", source_root=source)
    assert any("startup welcome disabled" in issue for issue in issues)


def test_release_gate_accepts_welcome_and_web_loading_source(tmp_path: Path) -> None:
    if "source_root" not in inspect.signature(verify_release_assembly).parameters:
        pytest.fail("release verifier has no source_root gate for the startup source")
    source = _source(tmp_path / "good-source", show_welcome=True)
    issues = verify_release_assembly(tmp_path / "package", source_root=source)
    assert not any(issue.startswith("startup ") for issue in issues)
