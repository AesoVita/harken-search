"""The early splash frames must close ImGui's viewport frame before the next frame."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main() -> int:
    text = (ROOT / "src/WindowsMain.cpp").read_text(encoding="utf-8")
    failures = []
    for marker in ("##BetaSplashPre", "##BetaSplash"):
        start = text.rfind("ImGui::NewFrame();", 0, text.find(marker))
        end = text.find("SwapBuffers(hdc);", start)
        block = text[start:end]
        if "ImGui::NewFrame();" not in block or "ImGui::UpdatePlatformWindows();" not in block:
            failures.append(marker)
    if failures:
        print("FAIL missing viewport update:", ", ".join(failures))
        return 1
    print("PASS startup frames update platform windows")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
