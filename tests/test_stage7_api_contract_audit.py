from __future__ import annotations

import json
from pathlib import Path

from tools import stage7_api_contract_audit as audit


def test_inventory_is_sorted_classified_and_hash_stable(tmp_path: Path) -> None:
    src = tmp_path / "src"
    (src / "platform").mkdir(parents=True)
    (src / "zeta.h").write_text("#pragma once\n", encoding="utf-8")
    (src / "alpha.hpp").write_text("#pragma once\n", encoding="utf-8")
    (src / "platform" / "filesystem.h").write_text("#pragma once\n", encoding="utf-8")

    first = audit.build_register(tmp_path)
    second = audit.build_register(tmp_path)

    assert first == second
    assert [item["path"] for item in first["headers"]] == [
        "src/alpha.hpp",
        "src/platform/filesystem.h",
        "src/zeta.h",
    ]
    assert first["headers"][1]["classification"] == "platform"
    assert len(first["headers"][0]["sha256"]) == 64


def test_inventory_records_sorted_include_consumers(tmp_path: Path) -> None:
    src = tmp_path / "src"
    src.mkdir()
    (src / "api.h").write_text("#pragma once\n", encoding="utf-8")
    (src / "consumer.cpp").write_text('#include "api.h"\n', encoding="utf-8")

    register = audit.build_register(tmp_path)
    api = next(item for item in register["headers"] if item["path"] == "src/api.h")

    assert api["consumer_count"] == 1
    assert api["consumers"] == ["src/consumer.cpp"]


def test_register_json_is_deterministic(tmp_path: Path) -> None:
    (tmp_path / "src").mkdir()
    (tmp_path / "src" / "api.h").write_text("#pragma once\n", encoding="utf-8")
    output = tmp_path / "register.json"

    audit.write_register(tmp_path, output)
    first = output.read_text(encoding="utf-8")
    audit.write_register(tmp_path, output)
    second = output.read_text(encoding="utf-8")

    assert first == second
    assert json.loads(first)["headers"][0]["path"] == "src/api.h"


def test_real_public_header_graph_is_acyclic() -> None:
    register = audit.build_register(Path.cwd())

    assert audit.find_public_include_cycles(Path.cwd(), register) == []


def test_real_register_has_no_unresolved_header_and_exclusions_have_reasons() -> None:
    register = audit.build_register(Path.cwd())

    assert all(item["contract_state"] != "unreviewed" for item in register["headers"])
    headers = {item["path"] for item in register["headers"]}
    reviewed = set(audit.REVIEWED_CONTRACTS)
    excluded_paths = set(audit.EXCLUDED_HEADERS)
    assert not reviewed & excluded_paths
    assert headers == reviewed | excluded_paths
    assert all((Path.cwd() / evidence).is_file() for evidence in audit.REVIEWED_CONTRACTS.values())
    excluded = [item for item in register["headers"] if item["contract_state"] == "excluded"]
    assert {item["path"] for item in excluded} == excluded_paths
    assert all(item["exclusion_reason"] for item in excluded)


def test_bootstrap_launch_broker_header_has_an_explicit_review_contract() -> None:
    assert audit.REVIEWED_CONTRACTS.get(
        "src/bootstrap/WindowsBootstrapLaunchBroker.h"
    ) == "tests/bootstrap/test_aris_bootstrap_launch_broker.cpp"


def test_renamed_and_new_headers_keep_specific_review_evidence() -> None:
    expected = {
        "src/DatabasePackageReplacement.h": "tests/test_database_package_replacement.cpp",
        "src/HarkenSearchDesignTokens.generated.h": "tests/test_aris_design_tokens.py",
        "src/bootstrap/HarkenSearchBootstrapAdmission.h": "tests/bootstrap/test_aris_bootstrap_admission.cpp",
        "src/bootstrap/HarkenSearchBootstrapExitExplanation.h": "tests/test_bootstrap_exit_explanation.cpp",
        "src/bootstrap/HarkenSearchBootstrapMain.h": "tests/bootstrap/test_aris_bootstrap_entrypoint.cpp",
        "src/bootstrap/HarkenSearchBootstrapRuntime.h": "tests/bootstrap/test_aris_bootstrap_runtime.cpp",
        "src/update/HarkenSearchUpdateComposition.h": "tests/test_update_product_composition.py",
        "src/update/HarkenSearchUpdateNoticeModel.h": "tests/test_update_notice_content.cpp",
        "src/update/HarkenSearchUpdateNoticeUi.h": "tests/test_update_notice_presentation.cpp",
        "src/ImGuiMouseCaptureLoss.h": "tests/test_magic_eq_capture_loss_win32.cpp",
        "src/platform/floating_window_corners.h": "tests/test_floating_window_corners.cpp",
        "src/SettingsMigration.h": "tests/test_settings_migration.cpp",
    }
    for header, evidence in expected.items():
        assert audit.REVIEWED_CONTRACTS.get(header) == evidence
    assert "@stage7-contract" in Path("src/ImGuiMouseCaptureLoss.h").read_text(encoding="utf-8")
