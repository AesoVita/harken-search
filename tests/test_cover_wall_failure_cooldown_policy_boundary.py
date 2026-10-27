from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/CoverWallFailureCooldownPolicy.h"
SOURCE = ROOT / "src/CoverWallFailureCooldownPolicy.cpp"


def test_failure_cooldown_policy_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    for contract in (
        "FailureCoolingDown",
        "RecordFailure",
        "RecordSuccess",
        "ShouldRecordConfirmedFailure",
        "ReadyToWorkCandidateSatisfied",
    ):
        assert contract in source
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert '"platform/' not in source


def test_main_consumes_failure_cooldown_policy():
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "CoverWallFailureCooldownPolicy.h"' in main
    for old_owner in (
        "static bool CoverWallFailureCoolingDown(",
        "static void CoverWallRecordFailure(",
        "static void CoverWallRecordSuccess(",
        "static bool CoverWallShouldRecordConfirmedFailure(",
        "static bool CoverWallReadyToWorkCandidateSatisfied(",
    ):
        assert old_owner not in main
