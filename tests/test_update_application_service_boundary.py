from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/update/UpdateApplicationService.h"
SOURCE = ROOT / "src/update/UpdateApplicationService.cpp"


def test_application_service_stays_update_owned_and_platform_free():
    text = HEADER.read_text(encoding="utf-8") + SOURCE.read_text(encoding="utf-8")
    forbidden = (
        "#include <windows.h>",
        "#include <wininet.h>",
        "src/bootstrap",
        "bootstrap/ArisBootstrapPlan.h",
        "std::thread",
        "sleep_for",
        "main.cpp",
        "WindowsMain",
        "webui",
        "sqlite",
    )
    for token in forbidden:
        assert token not in text


def test_application_service_composes_existing_boundaries():
    header = HEADER.read_text(encoding="utf-8")
    assert '#include "update/UpdateCheckService.h"' in header
    assert "UpdateRuntime runtime_" in header
    assert "UpdateCheckService checkService_" in header
