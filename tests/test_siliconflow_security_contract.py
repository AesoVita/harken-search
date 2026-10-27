from pathlib import Path
import json
import sys


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
AI_ENGINE = (ROOT / "src" / "AIEngine.cpp").read_text(encoding="utf-8")
AI_ENGINE_H = (ROOT / "src" / "AIEngine.h").read_text(encoding="utf-8")
DISCLOSURE = (ROOT / "AI_FEATURE_DISCLOSURE.md").read_text(encoding="utf-8")
PRIVACY = (ROOT / "PRIVACY.md").read_text(encoding="utf-8")
LEDGER = (ROOT / "locale" / "harken.ui.strings.json").read_text(encoding="utf-8")
PACK_ENGINE = (ROOT / "tools" / "pack_engine.py").read_text(encoding="utf-8")
PACK_MANIFEST = json.loads((ROOT / "tools" / "pack_manifest.json").read_text(encoding="utf-8"))

sys.path.insert(0, str(ROOT / "tools"))
import pack_engine  # noqa: E402

# Product/release surfaces that must not ship the retired third-party endpoint.
RELEASE_SCAN_PATHS = (
    ROOT / "src",
    ROOT / "tools" / "pack_engine.py",
    ROOT / "tools" / "pack_manifest.json",
    ROOT / "AI_FEATURE_DISCLOSURE.md",
    ROOT / "PRIVACY.md",
    ROOT / "locale" / "harken.ui.strings.json",
)
RELEASE_SCAN_SUFFIXES = {".cpp", ".h", ".hpp", ".py", ".json", ".md"}


def _iter_release_files():
    for path in RELEASE_SCAN_PATHS:
        if path.is_file():
            yield path
            continue
        for child in path.rglob("*"):
            if child.is_file() and child.suffix.lower() in RELEASE_SCAN_SUFFIXES:
                yield child


def _load_block(source: str, start_marker: str, end_marker: str) -> str:
    start = source.index(start_marker)
    end = source.index(end_marker, start + len(start_marker))
    return source[start:end]


def test_release_sources_must_not_contain_siliconflow_domain():
    hits = []
    for path in _iter_release_files():
        text = path.read_text(encoding="utf-8")
        if "siliconflow.cn" in text.lower():
            hits.append(path.relative_to(ROOT).as_posix())
    assert hits == [], f"siliconflow.cn still appears in release sources: {hits}"


def test_cover_generation_must_not_post_to_siliconflow():
    assert "https://api.siliconflow.cn/v1/images/generations" not in AI_ENGINE
    assert "siliconflow.cn" not in AI_ENGINE.lower()
    generate = _load_block(
        AI_ENGINE,
        "int AIEngine::GenerateCoverAsync",
        "std::vector<int> AIEngine::BatchTranscribe",
    )
    assert "SendHttpPost" not in generate
    execute_cover = AI_ENGINE.split("case AITaskType::CoverGeneration", 1)[1].split("break;", 1)[0]
    assert "SendHttpPost" not in execute_cover
    assert "images/generations" not in execute_cover


def test_generate_cover_async_fails_closed_without_http():
    generate = _load_block(
        AI_ENGINE,
        "int AIEngine::GenerateCoverAsync",
        "std::vector<int> AIEngine::BatchTranscribe",
    )
    assert "AITaskStatus::Failed" in generate
    assert "SendHttpPost" not in generate
    assert "siliconFlowApiKey" not in generate


def test_settings_page_has_no_siliconflow_key_input():
    assert "##SiliconFlowApiKey" not in MAIN
    assert "g_siliconFlowDialogKeyBuf" not in MAIN
    assert "HintAwareInputTextWithHint(\"##SiliconFlowApiKey\"" not in MAIN
    assert "L(Set_SiliconFlowHint)" not in MAIN
    assert "SectionHeader(L(Set_AIDrawSettings)" not in MAIN
    assert "sfKeyVisible" not in MAIN
    assert 'u8"ai 绘图 key siliconflow"' not in MAIN


def test_cover_menu_does_not_open_ai_generation():
    assert "ImGui::MenuItem(L(Meta_AIGenCover))" not in MAIN
    assert "state.showAICoverStudio = true" not in MAIN
    assert 'ProductEdition::hasFeature("ai_cover_gen")' not in MAIN


def test_startup_load_always_clears_stored_siliconflow_key():
    load = _load_block(MAIN, "void LoadAppSettings", "// F-034: Populate")
    assert "DeleteSiliconFlowApiKey()" in load
    assert "state.settings.siliconFlowApiKey.clear()" in load
    assert "RemoveKeyValueFileEntry(g_appSettingsPathUtf8, \"siliconFlowApiKey\")" in load
    assert "RemoveKeyValueFileEntry(g_appSettingsPathUtf8, \"siliconFlowApiKeyPendingClear\")" in load
    assert "LoadSiliconFlowApiKey" not in load
    assert "StoreSiliconFlowApiKey" not in load
    assert "hasSecureSiliconFlowKey" not in load


def test_settings_save_never_stores_a_new_siliconflow_key():
    save = _load_block(MAIN, "bool TrySaveAppSettings", "void MarkSettingsDirty")
    assert "StoreSiliconFlowApiKey" not in save
    assert 'items.push_back({"siliconFlowApiKey", state.settings.siliconFlowApiKey})' not in save
    assert 'items.push_back({"siliconFlowApiKey", previousKeyIt->second})' not in save
    assert "siliconFlowApiKeyPendingClear" not in save
    assert "siliconFlowApiKeySaveFailedNotice" not in save


def test_pack_manifest_delists_ai_cover_gen():
    feature_ids = [feature["id"] for feature in PACK_MANIFEST["features"]]
    assert "ai_cover_gen" not in feature_ids
    assert "ai_cover_gen" not in pack_engine.AI_FEATURE_IDS
    assert "ai_engine" in pack_engine.AI_FEATURE_IDS
    assert "ai_tags" in pack_engine.AI_FEATURE_IDS
    assert '"ai_cover_gen"' not in PACK_ENGINE


def test_disclosure_and_privacy_no_longer_send_prompts_to_siliconflow():
    combined = DISCLOSURE + "\n" + PRIVACY
    assert "SiliconFlow" not in combined
    assert "siliconflow" not in combined.lower()
    assert "cover-generation prompts" not in combined
    assert "Cover generation currently sends prompts" not in combined
    assert "Cover-generation requests currently target SiliconFlow" not in combined


def test_user_visible_ledger_does_not_point_to_siliconflow():
    ledger = json.loads(LEDGER)
    leftover = []
    for entry in ledger.get("entries", []):
        for lang in ("zh", "en", "ja"):
            text = str(entry.get(lang, ""))
            lowered = text.lower()
            if "siliconflow" in lowered or "siliconflow.cn" in lowered:
                leftover.append(f"{entry.get('id')}:{lang}")
    assert leftover == [], f"user-visible SiliconFlow copy remains: {leftover}"


def test_settings_encryption_readonly_switch_precedes_credential_io():
    cpp = (ROOT / "tests" / "test_settings_encryption.cpp").read_text(encoding="utf-8")
    main_at = cpp.index("int main(")
    body = cpp[main_at:]
    switch_at = body.index("--source-contract-only")
    io_markers = (
        "EncryptSettingsValue(",
        "ProtectSettingsValue(",
        "CredWriteW(",
        "temp_directory_path(",
    )
    first_io = min(body.index(marker) for marker in io_markers)
    assert switch_at < first_io
    assert 'mainText.find("##SiliconFlowApiKey")' in body
    assert 'mainText.find("g_siliconFlowDialogKeyBuf")' in body
    assert 'StoreSiliconFlowApiKey(state.settings.siliconFlowApiKey)' in body
    assert 'mainText.find("ProtectSettingsValue")' in body
    cmake = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    assert "COMMAND TestSettingsEncryption --source-contract-only" in cmake
