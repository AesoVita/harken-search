from __future__ import annotations

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def test_update_persistence_contract_is_owned_and_platform_details_are_one_way() -> None:
    contract_path = ROOT / "src/update/UpdatePersistencePort.h"
    adapter_path = ROOT / "src/platform/update_persistence_port.h"
    implementation_path = ROOT / "src/platform/update_persistence_port.cpp"
    assert contract_path.is_file()
    assert adapter_path.is_file()
    assert implementation_path.is_file()

    contract = contract_path.read_text(encoding="utf-8")
    adapter = adapter_path.read_text(encoding="utf-8")
    implementation = implementation_path.read_text(encoding="utf-8")

    for forbidden in (
        "windows.h",
        "shlobj.h",
        "std::filesystem",
        "nlohmann",
        "main.cpp",
        "WindowsMain",
        "Database.h",
        "AudioEngine",
        "imgui.h",
    ):
        assert forbidden not in contract
        assert forbidden not in adapter

    assert "class UpdatePersistencePort" in contract
    assert "ExecuteStoreIntents" in contract
    assert "ExecuteDownloadIntents" in contract
    assert "ObserveStagedPatch" in contract
    assert "CreateUpdatePersistencePort()" in adapter
    assert "const std::string&" not in adapter
    assert "SHGetKnownFolderPath" in implementation
    assert "FOLDERID_LocalAppData" in implementation
    assert "FILE_FLAG_OPEN_REPARSE_POINT" in implementation
    assert "FileAttributeTagInfo" in implementation
    assert "SetFileInformationByHandle" in implementation
    assert "NtSetInformationFile" in implementation
    assert "FileRenameInformation" in implementation
    assert "FileDispositionInfo" in implementation
    assert "HashAndSizeFromHandle" in implementation
    assert "NtCreateFile" in implementation
    assert "OBJ_DONT_REPARSE" in implementation
    assert "object.RootDirectory = root" in implementation
    assert "WriteAndRename" in implementation
    execute_store = implementation.split("UpdatePersistenceResult ExecuteStoreIntents", 1)[1].split(
        "UpdatePersistenceResult ExecuteDownloadIntents", 1
    )[0]
    assert "WriteAndRename" in execute_store
    assert "BCryptHashData" in implementation
    assert "GetFileSizeEx" in implementation
    assert "LockFileEx" in implementation
    assert "main.cpp" not in implementation
    assert "WindowsMain" not in implementation


def test_update_persistence_port_is_registered_in_build_and_governance() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    rules = (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
        encoding="utf-8"
    )
    stage7_tool = (ROOT / "tools/stage7_api_contract_audit.py").read_text(
        encoding="utf-8"
    )

    assert "src/platform/update_persistence_port.cpp" in cmake
    assert "TestUpdatePersistencePort" in behavior
    assert "TestUpdatePersistencePortBoundary" in behavior
    persistence_target_start = behavior.index("add_executable(TestUpdatePersistencePort")
    persistence_start = behavior.rfind("if(WIN32)", 0, persistence_target_start)
    boundary_registration = behavior.index("TestUpdatePersistencePortBoundary", persistence_start)
    # Find the matching WIN32 block, independent of unrelated test ordering.
    depth = 0
    persistence_end = None
    for command in re.finditer(r"(?m)^\s*(if|endif)\s*\(", behavior[persistence_start:]):
        depth += 1 if command.group(1) == "if" else -1
        if depth == 0:
            persistence_end = persistence_start + command.start()
            break
    assert persistence_end is not None
    assert persistence_start < boundary_registration < persistence_end
    persistence_target = behavior[persistence_start:persistence_end]
    assert "src/update/UpdateDownloadManager.cpp" in persistence_target
    assert "tests/test_update_persistence_port.cpp" in persistence_target
    for path in (
        "src/update/UpdatePersistencePort.h",
        "src/platform/update_persistence_port.h",
        "src/platform/update_persistence_port.cpp",
    ):
        assert f'"{path}"' in rules
    assert '"src/update/UpdatePersistencePort.h"' in stage7_tool
    assert '"src/platform/update_persistence_port.h"' in stage7_tool


def test_local_demo_injects_safe_persistence_root_without_changing_off_mode() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    implementation = (ROOT / "src/platform/update_persistence_port.cpp").read_text(
        encoding="utf-8"
    )
    assert "ARIS_UPDATE_LOCAL_DEMO_ROOT" in cmake
    assert "ARIS_UPDATE_LOCAL_DEMO_ROOT" in implementation
    assert "CreateFromBaseRoot" in implementation
    assert "SHGetKnownFolderPath" in implementation
    assert "FOLDERID_LocalAppData" in implementation


def test_local_demo_persistence_rejects_reparse_or_outside_root() -> None:
    implementation = (ROOT / "src/platform/update_persistence_port.cpp").read_text(
        encoding="utf-8"
    )
    assert "FILE_ATTRIBUTE_REPARSE_POINT" in implementation
    assert "IsSafeDirectoryHandle" in implementation


def test_persistence_creates_missing_root_before_opening_it() -> None:
    implementation = (ROOT / "src/platform/update_persistence_port.cpp").read_text(
        encoding="utf-8"
    )
    assert "SHCreateDirectoryExW" in implementation
    assert "ERROR_ALREADY_EXISTS" in implementation
    assert "CreateStatus::DirectoryCreateFailed" in implementation
