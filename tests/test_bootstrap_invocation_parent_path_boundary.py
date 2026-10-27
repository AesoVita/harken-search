from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ADMISSION = ROOT / "src/bootstrap/HarkenSearchBootstrapAdmission.cpp"
FILESYSTEM = ROOT / "src/bootstrap/WindowsBootstrapFilesystemWin32.cpp"


def test_capability_parent_match_normalizes_image_path() -> None:
    """The one-shot capability must match the authenticated parent using the
    same normalized-path semantics as the Win32 channel's sameIdentity.

    Root cause of the demo AdmissionRejected(14): the updater's self-identity
    stores the image path with '\\' separators while the Bootstrap's handle
    capture stores '/', so a raw operator== rejected a genuine same-process
    parent. The capability check must normalize the image path.
    """
    assert ADMISSION.is_file(), "Bootstrap admission core is required"
    source = ADMISSION.read_text(encoding="utf-8")

    # The parent comparison in the capability constructor must go through the
    # normalized-path helper, not a raw identity operator==.
    assert "sameAuthenticatedParent(authenticatedParent, invocation.parentProcess)" in source
    assert "!(authenticatedParent == invocation.parentProcess)" not in source

    # The helper must normalize the image path (mirroring sameIdentity) while
    # still comparing the exact process and file identity fields.
    assert "bool sameAuthenticatedParent(" in source
    assert "normalizePathText(left.imagePath) == normalizePathText(right.imagePath)" in source
    for field in (
        "left.processId == right.processId",
        "left.creationTime == right.creationTime",
        "left.imageVolumeSerial == right.imageVolumeSerial",
        "left.imageFileId == right.imageFileId",
    ):
        assert field in source, field


def test_filesystem_parent_match_normalizes_image_path() -> None:
    source = FILESYSTEM.read_text(encoding="utf-8")

    assert (
        "SameAuthenticatedParentProcess(\n"
        "                  trustedRoots_.authenticatedParentProcess,\n"
        "                  plan.invocation.parentProcess)"
    ) in source
    assert (
        "!(trustedRoots_.authenticatedParentProcess ==\n"
        "              plan.invocation.parentProcess)"
    ) not in source
    assert "bool SameAuthenticatedParentProcess(" in source
    assert "ComparableFinalPath" in source
    for field in (
        "left.processId == right.processId",
        "left.creationTime == right.creationTime",
        "left.imageVolumeSerial == right.imageVolumeSerial",
        "left.imageFileId == right.imageFileId",
    ):
        assert field in source, field
