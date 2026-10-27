from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COMPOSITION = ROOT / "src/update/HarkenSearchUpdateComposition.cpp"
AUTHORITY_PORT = ROOT / "src/platform/update_bootstrap_authority_port.h"


def test_update_composition_contains_no_direct_platform_implementation_details() -> None:
    source = COMPOSITION.read_text(encoding="utf-8")
    forbidden = (
        "<Windows.h>",
        "<Aclapi.h>",
        "<bcrypt.h>",
        "<fileapi.h>",
        "<processthreadsapi.h>",
        "<shlobj.h>",
        "<wintrust.h>",
        "std::filesystem",
        "HANDLE",
        "GetFileType",
        "GetStdHandle",
        "GetTickCount64",
        "PeekNamedPipe",
        "ReadFile",
        "WriteFile",
        "CreateFileW",
        "CreateNamedPipeW",
        "ConnectNamedPipe",
        "CreateJobObjectW",
        "SetInformationJobObject",
        "SetHandleInformation",
        "CreateProcessW",
        "TerminateProcess",
        "CloseHandle",
        "BCryptGenRandom",
        "SetEntriesInAclW",
        "SetNamedSecurityInfoW",
        "std::filesystem::recursive_directory_iterator",
        "mz_zip_reader_init_file",
        "mz_zip_reader_extract_file_to_file",
        "ExitProcess",
    )
    violations = [token for token in forbidden if token in source]
    assert not violations, (
        "HarkenSearchUpdateComposition.cpp must delegate platform implementation details "
        f"through a neutral port; direct tokens found: {violations}"
    )


def test_update_composition_declares_and_uses_neutral_bootstrap_authority_factory() -> None:
    source = COMPOSITION.read_text(encoding="utf-8")
    assert AUTHORITY_PORT.is_file(), (
        "Stage6-C requires a neutral update bootstrap authority factory header at "
        "src/platform/update_bootstrap_authority_port.h"
    )
    assert '#include "platform/update_bootstrap_authority_port.h"' in source
    assert (
        "Platform::UpdateBootstrapAuthority::CreateUpdateBootstrapAuthorityPort()"
        in source
    )
    assert "if (!authority.ok()) return;" in source
    assert "*authority.port, config" in source
