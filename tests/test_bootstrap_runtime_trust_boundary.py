from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WIN32 = ROOT / "src/bootstrap/WindowsBootstrapRuntimeWin32.cpp"


def test_updater_trust_is_optional_additive_authenticode() -> None:
    """Path A: Authenticode is optional-additive over a project-key trust floor.

    When both the parent updater and this Bootstrap carry a valid Authenticode
    signer, an identical signer is still required (strong publisher binding for
    signed releases). When Authenticode is absent (unsigned open-source builds),
    the updater's authority rests on the surrounding project-key floor. This must
    not become a blanket "skip verification": the Ed25519 package plan, the
    compiled-in Bootstrap hash pin, path ownership and the pipe/user checks stay.
    """
    assert WIN32.is_file(), "Windows Bootstrap runtime is required"
    source = WIN32.read_text(encoding="utf-8")

    assert "bool isTrustedUpdaterImage(" in source

    # Optional-additive guard: only enforce same-signer when a signer is present.
    assert (
        "if (updaterSigner.has_value() || bootstrapSigner.has_value())" in source
    ), "trust must be optional-additive, not an unconditional require-both"

    # Strong binding retained for signed releases.
    assert "*updaterSigner == *bootstrapSigner" in source

    # The project-key trust floor and its anchors must be documented in place so
    # the auditor can see the anchor is relocated, not lost.
    assert "Ed25519 package-plan verification" in source
    assert "updater-owned base" in source

    # The pre-Path-A behavior (unconditional require-both as the sole return) must
    # be gone: a neither-signed unsigned pair must be able to reach acceptance.
    assert (
        "return updaterSigner.has_value() && bootstrapSigner.has_value() &&\n"
        "           *updaterSigner == *bootstrapSigner;\n#endif"
        not in source
    ), "unsigned open-source pair must no longer be rejected outright"
