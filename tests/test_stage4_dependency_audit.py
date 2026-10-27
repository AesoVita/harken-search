import json
from pathlib import Path

from tools import stage4_dependency_audit as audit


def write_fixture(root: Path, relative_path: str, content: str) -> None:
    path = root / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def rules_for_fixture() -> dict[str, object]:
    return {
        "modules": {
            "composition": {
                "files": ["src/app/Main.cpp"],
                "may_depend_on": ["ui", "workflow", "domain"],
            },
            "ui": {
                "files": ["src/ui/**"],
                "may_depend_on": ["workflow", "domain"],
            },
            "workflow": {
                "files": ["src/workflow/**"],
                "may_depend_on": ["domain"],
            },
            "domain": {
                "files": ["src/domain/**"],
                "may_depend_on": [],
            },
        }
    }


def test_forbidden_upward_edge_is_reported(tmp_path: Path) -> None:
    write_fixture(tmp_path, "src/domain/Model.h", '#include "ui/View.h"\n')
    write_fixture(tmp_path, "src/ui/View.h", "#pragma once\n")

    result = audit.audit_repository(tmp_path, rules_for_fixture())

    assert result["forbidden_edges"] == [
        {
            "source": "domain",
            "target": "ui",
            "source_file": "src/domain/Model.h",
            "target_file": "src/ui/View.h",
            "include": "ui/View.h",
        }
    ]


def test_module_cycle_is_reported(tmp_path: Path) -> None:
    write_fixture(tmp_path, "src/ui/View.h", '#include "workflow/Run.h"\n')
    write_fixture(tmp_path, "src/workflow/Run.h", '#include "ui/View.h"\n')

    result = audit.audit_repository(tmp_path, rules_for_fixture())

    assert result["cycles"] == [["ui", "workflow", "ui"]]


def test_declared_file_must_match_exactly_one_module(tmp_path: Path) -> None:
    write_fixture(tmp_path, "src/shared/Overlap.h", "#pragma once\n")
    rules = {
        "modules": {
            "one": {"files": ["src/shared/**"], "may_depend_on": []},
            "two": {"files": ["src/**/Overlap.h"], "may_depend_on": []},
        }
    }

    result = audit.audit_repository(tmp_path, rules)

    assert result["membership_errors"] == [
        {
            "file": "src/shared/Overlap.h",
            "matched_modules": ["one", "two"],
        }
    ]


def test_real_priority_scope_has_no_direction_violation() -> None:
    root = Path(__file__).resolve().parents[1]
    rules_path = root / "docs/architecture/stage-4-dependency-rules.json"
    rules = json.loads(rules_path.read_text(encoding="utf-8"))

    result = audit.audit_repository(root, rules)

    assert result["forbidden_edges"] == []
    assert result["cycles"] == []
    assert result["membership_errors"] == []


def test_module_ui_uses_narrow_host_capability() -> None:
    root = Path(__file__).resolve().parents[1]
    module_ui = (root / "src/ModuleUI.cpp").read_text(encoding="utf-8")
    host_header = root / "src/UiHostServices.h"

    assert '#include "WindowsMain.h"' not in module_ui
    assert '#include "UiHostServices.h"' in module_ui
    assert "OpenFolderDialog(g_hwnd)" not in module_ui
    assert host_header.is_file()
    assert "windows.h" not in host_header.read_text(encoding="utf-8")


def test_render_ui_consumes_deferred_requests_through_policy() -> None:
    root = Path(__file__).resolve().parents[1]
    main_source = (root / "src/main.cpp").read_text(encoding="utf-8")
    render_opening = main_source.split("void RenderUI(AppState& state) {", 1)[1].split(
        "PumpAITranscriptionResults(state);", 1
    )[0]

    assert '#include "RenderFrameRequestPolicy.h"' in main_source
    assert render_opening.count("ConsumeDeferredRenderFrameRequests(") == 1
    assert "state.qaRescanRow = -1" not in render_opening
    assert "state.qaRequestScanBpm = false" not in render_opening
    assert "state.qaRequestWaveformRebuild = false" not in render_opening
    assert "state.qaRequestWaveformRebuildFull = false" not in render_opening


def test_stage4_focused_gates_are_registered_with_cmake() -> None:
    root = Path(__file__).resolve().parents[1]
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (root / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    app_sources = cmake.split("set(APP_SOURCES", 1)[1].split("# Platform abstraction", 1)[0]

    assert "src/RenderFrameRequestPolicy.cpp" in app_sources
    assert "add_executable(TestRenderFrameRequestPolicy EXCLUDE_FROM_ALL" in behavior
    policy_target = behavior.split(
        "add_executable(TestRenderFrameRequestPolicy EXCLUDE_FROM_ALL", 1
    )[1].split("add_test(NAME TestRenderFrameRequestPolicy", 1)[0]
    assert "tests/test_render_frame_request_policy.cpp" in policy_target
    assert "src/RenderFrameRequestPolicy.cpp" in policy_target
    assert "NAME TestStage4DependencyAudit" in behavior
    assert "tests/test_stage4_dependency_audit.py" in behavior
    assert "src/SimilarSoundsWindow.cpp" in app_sources
    assert "src/RelinkWorkflow.cpp" in app_sources
    assert "tests/test_relink_workflow_boundary.py" in behavior


def test_similar_sounds_window_is_governed_as_ui_orchestration() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    ui_files = set(rules["modules"]["ui_orchestration"]["files"])

    assert "src/SimilarSoundsWindow.h" in ui_files
    assert "src/SimilarSoundsWindow.cpp" in ui_files
    assert "src/UIRenderer.cpp" in ui_files


def test_relink_workflow_uses_governed_ui_and_host_capabilities() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    ui_files = set(rules["modules"]["ui_orchestration"]["files"])
    host_files = set(rules["modules"]["host_capability"]["files"])

    assert "src/RelinkWorkflow.h" in ui_files
    assert "src/RelinkWorkflow.cpp" in ui_files
    assert "src/platform/audio_file_dialog.h" in host_files
    assert "src/platform/database_file_dialog.h" in host_files
    assert "src/platform/executable_file_dialog.h" in host_files
    assert "src/platform/avatar_image_file_dialog.h" in host_files
    assert "src/platform/reaper_file_list_dialog.h" in host_files
    assert "src/platform/thesaurus_json_dialog.h" in host_files
    assert "src/platform/settings_import_dialog.h" in host_files
    assert "src/platform/cover_image_import_dialog.h" in host_files
    assert "src/platform/region_wav_export_dialog.h" in host_files


def test_record_removal_workflow_uses_governed_domain_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition_dependencies = set(
        rules["modules"]["composition"]["may_depend_on"]
    )
    workflow_files = set(rules["modules"]["domain_workflow"]["files"])

    assert "domain_workflow" in composition_dependencies
    assert "src/RecordRemovalWorkflow.h" in workflow_files
    assert "src/RecordRemovalWorkflow.cpp" in workflow_files
    assert "src/RecordRenameWorkflow.h" in workflow_files
    assert "src/RecordRenameWorkflow.cpp" in workflow_files
    assert rules["modules"]["domain_workflow"]["may_depend_on"] == [
        "domain_service",
        "domain_policy",
        "platform_api",
    ]


def test_batch_rename_policy_uses_governed_domain_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition_dependencies = set(
        rules["modules"]["composition"]["may_depend_on"]
    )
    state_dependencies = set(rules["modules"]["domain_state"]["may_depend_on"])
    policy_files = set(rules["modules"]["domain_policy"]["files"])

    assert "domain_policy" in composition_dependencies
    assert "domain_policy" in state_dependencies
    assert "src/BatchRenamePolicy.h" in policy_files
    assert "src/BatchRenamePolicy.cpp" in policy_files
    assert rules["modules"]["domain_policy"]["may_depend_on"] == [
        "platform_api"
    ]


def test_batch_rename_execution_uses_governed_workflow_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])

    assert "src/BatchRenameExecutionWorkflow.h" in workflow_files
    assert "src/BatchRenameExecutionWorkflow.cpp" in workflow_files
    assert workflow["may_depend_on"] == [
        "domain_service",
        "domain_policy",
        "platform_api",
    ]


def test_browser_system_folder_search_uses_governed_policy_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition_dependencies = set(
        rules["modules"]["composition"]["may_depend_on"]
    )
    policy = rules["modules"]["domain_policy"]
    policy_files = set(policy["files"])

    assert "domain_policy" in composition_dependencies
    assert "src/BrowserSystemFolderSearchPolicy.h" in policy_files
    assert "src/BrowserSystemFolderSearchPolicy.cpp" in policy_files
    assert policy["may_depend_on"] == ["platform_api"]


def test_browser_system_folder_search_runtime_uses_governed_service_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition_dependencies = set(
        rules["modules"]["composition"]["may_depend_on"]
    )
    service = rules["modules"]["domain_service"]
    service_files = set(service["files"])
    platform_files = set(rules["modules"]["platform_api"]["files"])

    assert "domain_service" in composition_dependencies
    assert "src/BrowserSystemFolderSearchRuntime.h" in service_files
    assert "src/BrowserSystemFolderSearchRuntime.cpp" in service_files
    assert service["may_depend_on"] == ["domain_policy", "platform_api"]
    assert "src/platform/filesystem.h" in platform_files


def test_file_player_deferred_db_warmup_uses_governed_service_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    service = rules["modules"]["domain_service"]
    service_files = set(service["files"])
    platform_files = set(rules["modules"]["platform_api"]["files"])

    assert "src/FilePlayerDeferredDbWarmupRuntime.h" in service_files
    assert "src/FilePlayerDeferredDbWarmupRuntime.cpp" in service_files
    assert "src/FilePlayerDeferredDbWarmupOperation.cpp" in service_files
    assert service["may_depend_on"] == ["domain_policy", "platform_api"]
    assert "src/platform/filesystem.h" in platform_files
    assert "src/platform/system.h" in platform_files


def test_file_player_database_open_uses_governed_workflow_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])

    assert "src/FilePlayerDatabaseOpenWorkflow.h" in workflow_files
    assert "src/FilePlayerDatabaseOpenWorkflow.cpp" in workflow_files
    assert "src/FilePlayerDatabaseOpenAdapter.cpp" in workflow_files
    assert workflow["may_depend_on"] == [
        "domain_service",
        "domain_policy",
        "platform_api",
    ]


def test_database_switch_worker_runtime_uses_governed_service_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition_dependencies = set(
        rules["modules"]["composition"]["may_depend_on"]
    )
    service = rules["modules"]["domain_service"]
    service_files = set(service["files"])

    assert "domain_service" in composition_dependencies
    assert "src/DatabaseSwitchWorkerRuntime.h" in service_files
    assert "src/DatabaseSwitchWorkerRuntime.cpp" in service_files
    assert service["may_depend_on"] == ["domain_policy", "platform_api"]


def test_same_folder_scan_runtime_uses_governed_service_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition_dependencies = set(
        rules["modules"]["composition"]["may_depend_on"]
    )
    service = rules["modules"]["domain_service"]
    service_files = set(service["files"])

    assert "domain_service" in composition_dependencies
    assert "src/SameFolderScanRuntime.h" in service_files
    assert "src/SameFolderScanRuntime.cpp" in service_files
    assert service["may_depend_on"] == ["domain_policy", "platform_api"]


def test_database_switch_blocking_operation_uses_governed_workflow_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])

    assert "src/DatabaseSwitchBlockingOperation.h" in workflow_files
    assert "src/DatabaseSwitchBlockingOperation.cpp" in workflow_files
    assert "src/DatabaseSwitchBlockingOperationAdapter.cpp" in workflow_files
    assert workflow["may_depend_on"] == [
        "domain_service",
        "domain_policy",
        "platform_api",
    ]


def test_database_session_view_state_reset_uses_governed_ui_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    ui = rules["modules"]["ui_orchestration"]
    ui_files = set(ui["files"])

    assert "ui_orchestration" in composition["may_depend_on"]
    assert "src/DatabaseSessionViewStateReset.h" in ui_files
    assert "src/DatabaseSessionViewStateReset.cpp" in ui_files
    assert "domain_state" in ui["may_depend_on"]
    assert "src/DatabaseSessionViewStateReset.h" not in composition["files"]
    assert "src/DatabaseSessionViewStateReset.cpp" not in composition["files"]


def test_database_session_scan_state_reset_uses_governed_ui_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    ui = rules["modules"]["ui_orchestration"]
    ui_files = set(ui["files"])

    assert "ui_orchestration" in composition["may_depend_on"]
    assert "src/DatabaseSessionScanStateReset.h" in ui_files
    assert "src/DatabaseSessionScanStateReset.cpp" in ui_files
    assert "domain_state" in ui["may_depend_on"]
    assert "src/DatabaseSessionScanStateReset.h" not in composition["files"]
    assert "src/DatabaseSessionScanStateReset.cpp" not in composition["files"]


def test_database_session_media_cache_reset_uses_governed_ui_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    ui = rules["modules"]["ui_orchestration"]
    ui_files = set(ui["files"])

    assert "ui_orchestration" in composition["may_depend_on"]
    assert "src/DatabaseSessionMediaCacheReset.h" in ui_files
    assert "src/DatabaseSessionMediaCacheReset.cpp" in ui_files
    assert "domain_state" in ui["may_depend_on"]
    assert "src/DatabaseSessionMediaCacheReset.h" not in composition["files"]
    assert "src/DatabaseSessionMediaCacheReset.cpp" not in composition["files"]


def test_waveform_rebuild_runtime_state_uses_governed_ui_boundary() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    ui = rules["modules"]["ui_orchestration"]
    ui_files = set(ui["files"])

    assert "ui_orchestration" in composition["may_depend_on"]
    assert "src/WaveformRebuildRuntimeState.h" in ui_files
    assert "src/WaveformRebuildRuntimeState.cpp" in ui_files
    assert "domain_state" in ui["may_depend_on"]
    assert "src/WaveformRebuildRuntimeState.h" not in composition["files"]
    assert "src/WaveformRebuildRuntimeState.cpp" not in composition["files"]


def test_waveform_rebuild_batch_selector_uses_governed_domain_service() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    service = rules["modules"]["domain_service"]
    service_files = set(service["files"])

    assert "domain_service" in composition["may_depend_on"]
    assert "src/WaveformRebuildBatchSelector.h" in service_files
    assert "src/WaveformRebuildBatchSelector.cpp" in service_files
    assert "src/WaveformRebuildBatchSelector.h" not in composition["files"]
    assert "src/WaveformRebuildBatchSelector.cpp" not in composition["files"]


def test_waveform_rebuild_failure_persistence_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/WaveformRebuildFailurePersistence.h" in workflow_files
    assert "src/WaveformRebuildFailurePersistence.cpp" in workflow_files
    assert "src/WaveformRebuildFailurePersistence.h" not in composition["files"]
    assert "src/WaveformRebuildFailurePersistence.cpp" not in composition["files"]


def test_waveform_rebuild_request_submission_uses_governed_domain_service() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    service = rules["modules"]["domain_service"]
    service_files = set(service["files"])

    assert "domain_service" in composition["may_depend_on"]
    assert "src/WaveformRebuildRequestSubmission.h" in service_files
    assert "src/WaveformRebuildRequestSubmission.cpp" in service_files
    assert "src/WaveformRebuildRequestSubmission.h" not in composition["files"]
    assert "src/WaveformRebuildRequestSubmission.cpp" not in composition["files"]


def test_adaptive_mini_format_finalization_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/AdaptiveMiniWaveformFormatFinalization.h" in workflow_files
    assert "src/AdaptiveMiniWaveformFormatFinalization.cpp" in workflow_files
    assert "src/AdaptiveMiniWaveformFormatFinalization.h" not in composition["files"]
    assert "src/AdaptiveMiniWaveformFormatFinalization.cpp" not in composition["files"]


def test_adaptive_mini_format_preparation_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/AdaptiveMiniWaveformFormatPreparation.h" in scope_files
    assert "src/AdaptiveMiniWaveformFormatPreparation.cpp" in scope_files
    assert "src/AdaptiveMiniWaveformFormatPreparation.h" in workflow_files
    assert "src/AdaptiveMiniWaveformFormatPreparation.cpp" in workflow_files
    assert "src/AdaptiveMiniWaveformFormatPreparation.h" not in composition["files"]
    assert "src/AdaptiveMiniWaveformFormatPreparation.cpp" not in composition["files"]


def test_adaptive_mini_rebuild_activation_uses_governed_domain_policy() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    policy = rules["modules"]["domain_policy"]
    policy_files = set(policy["files"])
    scope_files = set(rules["scope"])

    assert "domain_policy" in composition["may_depend_on"]
    assert "src/AdaptiveMiniWaveformRebuildActivation.h" in scope_files
    assert "src/AdaptiveMiniWaveformRebuildActivation.cpp" in scope_files
    assert "src/AdaptiveMiniWaveformRebuildActivation.h" in policy_files
    assert "src/AdaptiveMiniWaveformRebuildActivation.cpp" in policy_files
    assert "src/AdaptiveMiniWaveformRebuildActivation.h" not in composition["files"]
    assert "src/AdaptiveMiniWaveformRebuildActivation.cpp" not in composition["files"]


def test_waveform_rebuild_pump_gate_uses_governed_domain_policy() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    policy = rules["modules"]["domain_policy"]
    policy_files = set(policy["files"])
    scope_files = set(rules["scope"])

    assert "domain_policy" in composition["may_depend_on"]
    assert "src/WaveformRebuildPumpGate.h" in scope_files
    assert "src/WaveformRebuildPumpGate.cpp" in scope_files
    assert "src/WaveformRebuildPumpGate.h" in policy_files
    assert "src/WaveformRebuildPumpGate.cpp" in policy_files
    assert "src/WaveformRebuildPumpGate.h" not in composition["files"]
    assert "src/WaveformRebuildPumpGate.cpp" not in composition["files"]


def test_waveform_rebuild_record_workflow_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "domain_service" in workflow["may_depend_on"]
    assert "src/WaveformRebuildRecordWorkflow.h" in scope_files
    assert "src/WaveformRebuildRecordWorkflow.cpp" in scope_files
    assert "src/WaveformRebuildRecordWorkflow.h" in workflow_files
    assert "src/WaveformRebuildRecordWorkflow.cpp" in workflow_files
    assert "src/WaveformRebuildRecordWorkflow.h" not in composition["files"]
    assert "src/WaveformRebuildRecordWorkflow.cpp" not in composition["files"]


def test_waveform_rebuild_batch_lifecycle_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "domain_service" in workflow["may_depend_on"]
    assert "src/WaveformRebuildBatchLifecycle.h" in scope_files
    assert "src/WaveformRebuildBatchLifecycle.cpp" in scope_files
    assert "src/WaveformRebuildBatchLifecycle.h" in workflow_files
    assert "src/WaveformRebuildBatchLifecycle.cpp" in workflow_files
    assert "src/WaveformRebuildBatchLifecycle.h" not in composition["files"]
    assert "src/WaveformRebuildBatchLifecycle.cpp" not in composition["files"]


def test_waveform_rebuild_cancellation_recovery_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/WaveformRebuildCancellationRecovery.h" in scope_files
    assert "src/WaveformRebuildCancellationRecovery.cpp" in scope_files
    assert "src/WaveformRebuildCancellationRecovery.h" in workflow_files
    assert "src/WaveformRebuildCancellationRecovery.cpp" in workflow_files
    assert "src/WaveformRebuildCancellationRecovery.h" not in composition["files"]
    assert "src/WaveformRebuildCancellationRecovery.cpp" not in composition["files"]


def test_scan_waveform_backfill_drain_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/ScanWaveformBackfillDrainWorkflow.h" in scope_files
    assert "src/ScanWaveformBackfillDrainWorkflow.cpp" in scope_files
    assert "src/ScanWaveformBackfillDrainWorkflow.h" in workflow_files
    assert "src/ScanWaveformBackfillDrainWorkflow.cpp" in workflow_files
    assert "src/ScanWaveformBackfillDrainWorkflow.h" not in composition["files"]
    assert "src/ScanWaveformBackfillDrainWorkflow.cpp" not in composition["files"]


def test_selected_waveform_rebuild_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/SelectedWaveformRebuildWorkflow.h" in scope_files
    assert "src/SelectedWaveformRebuildWorkflow.cpp" in scope_files
    assert "src/SelectedWaveformRebuildWorkflow.h" in workflow_files
    assert "src/SelectedWaveformRebuildWorkflow.cpp" in workflow_files
    assert "src/SelectedWaveformRebuildWorkflow.h" not in composition["files"]
    assert "src/SelectedWaveformRebuildWorkflow.cpp" not in composition["files"]


def test_selected_record_rescan_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/SelectedRecordRescanWorkflow.h" in scope_files
    assert "src/SelectedRecordRescanWorkflow.cpp" in scope_files
    assert "src/SelectedRecordRescanWorkflow.h" in workflow_files
    assert "src/SelectedRecordRescanWorkflow.cpp" in workflow_files
    assert "src/SelectedRecordRescanWorkflow.h" not in composition["files"]
    assert "src/SelectedRecordRescanWorkflow.cpp" not in composition["files"]


def test_bpm_batch_scan_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/BpmBatchScanWorkflow.h" in scope_files
    assert "src/BpmBatchScanWorkflow.cpp" in scope_files
    assert "src/BpmBatchScanWorkflow.h" in workflow_files
    assert "src/BpmBatchScanWorkflow.cpp" in workflow_files
    assert "src/BpmBatchScanWorkflow.h" not in composition["files"]
    assert "src/BpmBatchScanWorkflow.cpp" not in composition["files"]


def test_bpm_batch_result_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/BpmBatchResultWorkflow.h" in scope_files
    assert "src/BpmBatchResultWorkflow.cpp" in scope_files
    assert "src/BpmBatchResultWorkflow.h" in workflow_files
    assert "src/BpmBatchResultWorkflow.cpp" in workflow_files
    assert "src/BpmBatchResultWorkflow.h" not in composition["files"]
    assert "src/BpmBatchResultWorkflow.cpp" not in composition["files"]


def test_single_music_result_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/SingleMusicAnalysisResultWorkflow.h" in scope_files
    assert "src/SingleMusicAnalysisResultWorkflow.cpp" in scope_files
    assert "src/SingleMusicAnalysisResultWorkflow.h" in workflow_files
    assert "src/SingleMusicAnalysisResultWorkflow.cpp" in workflow_files
    assert "src/SingleMusicAnalysisResultWorkflow.h" not in composition["files"]
    assert "src/SingleMusicAnalysisResultWorkflow.cpp" not in composition["files"]


def test_bpm_batch_completion_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/BpmBatchCompletionWorkflow.h" in scope_files
    assert "src/BpmBatchCompletionWorkflow.cpp" in scope_files
    assert "src/BpmBatchCompletionWorkflow.h" in workflow_files
    assert "src/BpmBatchCompletionWorkflow.cpp" in workflow_files
    assert "src/BpmBatchCompletionWorkflow.h" not in composition["files"]
    assert "src/BpmBatchCompletionWorkflow.cpp" not in composition["files"]


def test_cover_batch_operation_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/CoverBatchOperationWorkflow.h" in scope_files
    assert "src/CoverBatchOperationWorkflow.cpp" in scope_files
    assert "src/CoverBatchOperationWorkflow.h" in workflow_files
    assert "src/CoverBatchOperationWorkflow.cpp" in workflow_files
    assert "src/CoverBatchOperationWorkflow.h" not in composition["files"]
    assert "src/CoverBatchOperationWorkflow.cpp" not in composition["files"]


def test_embed_metadata_batch_runtime_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])

    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/EmbedMetadataBatchRuntime.h" in scope_files
    assert "src/EmbedMetadataBatchRuntime.cpp" in scope_files
    assert "src/EmbedMetadataBatchRuntime.h" in workflow_files
    assert "src/EmbedMetadataBatchRuntime.cpp" in workflow_files
    assert "src/EmbedMetadataBatchRuntime.h" not in composition["files"]
    assert "src/EmbedMetadataBatchRuntime.cpp" not in composition["files"]


def test_cover_rebuild_record_processor_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])
    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/CoverRebuildRecordProcessor.h" in scope_files
    assert "src/CoverRebuildRecordProcessor.cpp" in scope_files
    assert "src/CoverRebuildRecordProcessor.h" in workflow_files
    assert "src/CoverRebuildRecordProcessor.cpp" in workflow_files
    assert "src/CoverRebuildRecordProcessor.h" not in composition["files"]
    assert "src/CoverRebuildRecordProcessor.cpp" not in composition["files"]


def test_cover_rebuild_lifecycle_workflow_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])
    assert "domain_workflow" in composition["may_depend_on"]
    assert "src/CoverRebuildLifecycleWorkflow.h" in scope_files
    assert "src/CoverRebuildLifecycleWorkflow.cpp" in scope_files
    assert "src/CoverRebuildLifecycleWorkflow.h" in workflow_files
    assert "src/CoverRebuildLifecycleWorkflow.cpp" in workflow_files
    assert "src/CoverRebuildLifecycleWorkflow.h" not in composition["files"]
    assert "src/CoverRebuildLifecycleWorkflow.cpp" not in composition["files"]


def test_cover_rebuild_batch_commit_workflow_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])
    assert "domain_workflow" in composition["may_depend_on"]
    for path in (
        "src/CoverRebuildBatchCommitWorkflow.h",
        "src/CoverRebuildBatchCommitWorkflow.cpp",
    ):
        assert path in scope_files
        assert path in workflow_files
        assert path not in composition["files"]


def test_load_all_batch_runtime_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    workflow_files = set(workflow["files"])
    scope_files = set(rules["scope"])
    for path in ("src/LoadAllBatchRuntime.h", "src/LoadAllBatchRuntime.cpp"):
        assert path in scope_files
        assert path in workflow_files
        assert path not in composition["files"]


def test_same_folder_scan_workflow_uses_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    for path in ("src/SameFolderScanWorkflow.h", "src/SameFolderScanWorkflow.cpp"):
        assert path in rules["scope"]
        assert path in workflow["files"]
        assert path not in composition["files"]


def test_async_search_result_workflows_use_governed_domain_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    rules = json.loads(
        (root / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    composition = rules["modules"]["composition"]
    workflow = rules["modules"]["domain_workflow"]
    for path in (
        "src/AsyncSearchFacetResultWorkflow.h",
        "src/AsyncSearchFacetResultWorkflow.cpp",
        "src/RandomShufflePlaybackWorkflow.h",
        "src/RandomShufflePlaybackWorkflow.cpp",
        "src/AsyncSearchRefreshFacetsWorkflow.h",
        "src/AsyncSearchRefreshFacetsWorkflow.cpp",
        "src/AsyncSearchAppendResultWorkflow.h",
        "src/AsyncSearchAppendResultWorkflow.cpp",
        "src/AsyncSearchRefreshCountRepairWorkflow.h",
        "src/AsyncSearchRefreshCountRepairWorkflow.cpp",
        "src/AsyncSearchRefreshCompletionWorkflow.h",
        "src/AsyncSearchRefreshCompletionWorkflow.cpp",
        "src/AsyncSearchRefreshMemoryWorkflow.h",
        "src/AsyncSearchRefreshMemoryWorkflow.cpp",
        "src/AsyncSearchWaveformPreloadWorkflow.h",
        "src/AsyncSearchWaveformPreloadWorkflow.cpp",
        "src/AsyncSearchCoverPreloadWorkflow.h",
        "src/AsyncSearchCoverPreloadWorkflow.cpp",
        "src/AsyncSearchEmptyFallbackWorkflow.h",
        "src/AsyncSearchEmptyFallbackWorkflow.cpp",
        "src/AsyncSearchRefreshDiagnosticsWorkflow.h",
        "src/AsyncSearchRefreshDiagnosticsWorkflow.cpp",
        "src/AsyncSearchRefreshResultsWorkflow.h",
        "src/AsyncSearchRefreshResultsWorkflow.cpp",
        "src/CoverKeyFilterWorkflow.h",
        "src/CoverKeyFilterWorkflow.cpp",
        "src/AsyncSearchBrowsePreviewWorkflow.h",
        "src/AsyncSearchBrowsePreviewWorkflow.cpp",
        "src/AsyncSearchResultLifecycleWorkflow.h",
        "src/AsyncSearchResultLifecycleWorkflow.cpp",
        "src/PlaylistResultFilterWorkflow.h",
        "src/PlaylistResultFilterWorkflow.cpp",
        "src/AsyncSearchPlaylistContextWorkflow.h",
        "src/AsyncSearchPlaylistContextWorkflow.cpp",
    ):
        assert path in rules["scope"]
        assert path in workflow["files"]
        assert path not in composition["files"]
    assert "src/CoverBrowseSurfaceState.h" in rules["scope"]
    assert "src/CoverBrowseSurfaceState.h" in rules["modules"]["domain_policy"]["files"]


def test_database_test_stubs_yield_to_external_platform_providers() -> None:
    root = Path(__file__).resolve().parents[1]
    stubs = (root / "tests/test_database_link_stubs.cpp").read_text(encoding="utf-8")
    behavior = (root / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")

    assert "#ifndef AURO_TEST_EXTERNAL_EXE_DIR" in stubs
    assert "#ifndef AURO_TEST_EXTERNAL_PATH_ATTRIBUTES" in stubs
    assert "AURO_TEST_EXTERNAL_EXE_DIR=1" in behavior
    assert "AURO_TEST_EXTERNAL_PATH_ATTRIBUTES=1" in behavior


def test_appstate_test_stub_yields_to_real_similarity_workflow() -> None:
    root = Path(__file__).resolve().parents[1]
    stubs = (root / "tests/test_appstate_link_stubs.cpp").read_text(encoding="utf-8")
    behavior = (root / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    real_workflow_target = behavior.split(
        "add_executable(TestSimilarityQueryService EXCLUDE_FROM_ALL", 1
    )[1].split("add_executable(TestLocalization", 1)[0]

    assert "#ifndef AURO_TEST_REAL_SIMILARITY_WORKFLOW" in stubs
    assert "SimilarityWorkflow::~SimilarityWorkflow()" in stubs
    assert "AURO_TEST_REAL_SIMILARITY_WORKFLOW=1" in real_workflow_target


def test_cmake_targets_choose_one_miniaudio_implementation() -> None:
    root = Path(__file__).resolve().parents[1]
    behavior = (root / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    offenders = []

    for target_block in behavior.split("add_executable(")[1:]:
        declaration = target_block.split(")", 1)[0]
        target = declaration.split(None, 1)[0]
        if (
            "src/AudioEngine.cpp" in declaration
            and "tests/miniaudio_impl.cpp" in declaration
        ):
            offenders.append(target)

    assert offenders == []
