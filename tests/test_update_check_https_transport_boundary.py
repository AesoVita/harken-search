from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/update/UpdateCheckHttpsTransport.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "src/update/UpdateCheckHttpsTransport.cpp").read_text(encoding="utf-8")
PLATFORM = (
    ROOT / "src/platform/update_check_https_transport_win32.cpp"
).read_text(encoding="utf-8") if (
    ROOT / "src/platform/update_check_https_transport_win32.cpp"
).is_file() else ""


def test_native_https_details_stay_out_of_public_header():
    assert "windows.h" not in HEADER
    assert "wininet.h" not in HEADER
    assert "HINTERNET" not in HEADER
    assert '#include "update/UpdateCheckTransport.h"' in HEADER
    assert '#include "update/UpdateCheckService.h"' not in HEADER


def test_check_adapter_has_no_filesystem_scheduler_or_ui_coupling():
    for token in ("filesystem", "std::thread", "sleep_for", "main.cpp", "WindowsMain", "webui", "sqlite"):
        assert token not in SOURCE


def test_wininet_implementation_stays_in_platform_adapter():
    forbidden = (
        "windows.h",
        "wininet.h",
        "HINTERNET",
        "InternetOpenW",
        "InternetOpenUrlW",
        "InternetReadFile",
        "InternetCloseHandle",
        "MultiByteToWideChar",
        "WideCharToMultiByte",
        "GetLastError",
    )
    violations = [token for token in forbidden if token in SOURCE]
    assert not violations, (
        "UpdateCheckHttpsTransport.cpp must remain neutral; "
        f"WinINet details found: {violations}"
    )
    assert PLATFORM, "Win32 HTTPS transport adapter is required"
    for token in forbidden[1:]:
        assert token in PLATFORM


def test_https_platform_sources_are_registered_without_duplicate_product_source():
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    assert "src/platform/update_check_https_transport_win32.cpp" in cmake
    assert "src/platform/update_check_https_transport_unsupported.cpp" in cmake
    assert behavior.count("src/platform/update_check_https_transport_win32.cpp") == 1
