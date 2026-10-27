from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_load_all_batch_runtime_is_the_only_load_all_state_owner() -> None:
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    pump = main.split("static void PumpLoadAllInBatches(AppState& state)", 1)[1].split(
        "// Helper: Load ALL results", 1
    )[0]
    assert main.count("aura::load_all::LoadAllBatchRuntime g_loadAllBatchRuntime") == 1
    assert "g_loadAllBatchRuntime.onFrame(" in pump
    assert "g_loadAllJob" not in main
    assert "SubmitAsyncSearch(state, filter, AsyncSearchKind::LoadAll" in main
