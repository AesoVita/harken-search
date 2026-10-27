from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_http_transport_contract_is_owned_and_platform_free() -> None:
    header = ROOT / "src/update/UpdateHttpTransport.h"
    source = ROOT / "src/update/UpdateHttpTransport.cpp"
    adapter = ROOT / "src/platform/update_http_transport.cpp"
    assert header.is_file()
    assert source.is_file()
    assert adapter.is_file()
    pure = header.read_text(encoding="utf-8") + source.read_text(encoding="utf-8")
    for forbidden in (
        "windows.h", "wininet.h", "HINTERNET", "Sleep(", "std::filesystem",
        "main.cpp", "WindowsMain", "Database", "AudioEngine", "imgui.h",
        "updateRoot", "storageRoot",
    ):
        assert forbidden not in pure
    assert "Range" in pure
    assert "If-Range" in pure
    assert "contentRange" in pure
    assert "TransferAppendIntent" in pure
    assert "VerifyPartIntent" in pure


def test_wininet_adapter_is_explicit_cancellable_and_registered() -> None:
    implementation = (ROOT / "src/platform/update_http_transport.cpp").read_text(
        encoding="utf-8"
    )
    for required in (
        "InternetOpenW", "InternetOpenUrlW", "HttpQueryInfoW",
        "InternetReadFile", "INTERNET_FLAG_SECURE", "IsCancellationRequested",
        "INTERNET_FLAG_NO_AUTO_REDIRECT", "INTERNET_OPTION_CONNECT_TIMEOUT",
        "INTERNET_OPTION_RECEIVE_TIMEOUT", "RegisterInterrupt",
    ):
        assert required in implementation
    assert "Sleep(" not in implementation
    assert "CreateUpdateHttpTransportPort()" in implementation
    hooks = (ROOT / "src/platform/update_http_transport_test_hooks.h").read_text(
        encoding="utf-8"
    )
    assert "NativeHttpApi" in hooks
    assert "CreateWithNativeApi" in hooks

    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    rules = (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
        encoding="utf-8"
    )
    stage7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text(encoding="utf-8")
    assert "src/update/UpdateHttpTransport.cpp" in cmake
    assert "src/platform/update_http_transport.cpp" in cmake
    assert "TestUpdateHttpTransport" in behavior
    assert "TestUpdateHttpTransportPlatform" in behavior
    assert '"src/update/UpdateHttpTransport.h"' in rules
    assert '"src/platform/update_http_transport.cpp"' in rules
    assert '"src/update/UpdateHttpTransport.h"' in stage7
    assert '"src/platform/update_http_transport.h"' in stage7
    assert '"src/platform/update_http_transport_test_hooks.h"' in stage7
