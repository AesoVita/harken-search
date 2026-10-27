from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/bootstrap/HarkenSearchBootstrapPlan.h"


def test_shared_bootstrap_contract_exposes_canonical_installation_guid():
    source = HEADER.read_text(encoding="utf-8")
    match = re.search(
        r"kArisInstalledInstallationGuid\s*=\s*\"([0-9A-F-]{36})\"",
        source,
    )
    assert match
    assert match.group(1) == "A8B3C9D1-E4F5-4A6B-8C9D-0E1F2A3B4C5D"
