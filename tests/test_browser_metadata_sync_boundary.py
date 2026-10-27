from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_browser_metadata_uses_database_identity_and_complete_record_model():
    database = read("src/Database.cpp")
    app_state = read("src/AppState.h")
    main = read("src/main.cpp")

    exact_folder = database.split(
        "std::vector<AudioRecord> Database::getFilesByExactFolder", 1
    )[1].split("std::vector<AudioRecord> Database::getFilesByIds", 1)[0]
    assert "HarkenSearch::CanonicalFolderPath(folderPath)" in exact_folder
    assert "canonicalFolderPath.c_str()" in exact_folder
    assert "std::unordered_map<std::string, AudioRecord> browserMetaCache" in app_state
    assert "struct BrowserMetaMini" not in app_state
    assert "state.browserMetaCache[r.filePath] = std::move(r);" in main
    assert 'metaIt->second.getMeta("mood")' in main
    assert "metaIt->second.dateAdded > 0" in main


def test_unindexed_browser_metadata_is_async_shared_and_stateful():
    database_h = read("src/Database.h")
    database = read("src/Database.cpp")
    app_state = read("src/AppState.h")
    main = read("src/main.cpp")

    assert "readAudioRecordMetadataSnapshot" in database_h
    assert database.count("ApplyAudioMetadataToRecord(record, metadata)") >= 2
    assert "std::future<BrowserMetadataLoadResult>" in main
    assert "Database::readAudioRecordMetadataSnapshot(path, result.record)" in main
    assert "BrowserMetadataLoadState" in app_state
    assert "BrowserMetadataLoadState::Loading" in main
    assert "BrowserMetadataLoadState::Available" in main
    assert "BrowserMetadataLoadState::Unavailable" in main
    assert "result.cacheKey != cacheKey" in main
    assert "state.browserMetaLoadStates.clear()" in main
    assert 'ImGui::TextUnformatted(loading ? "..." : "-")' in main
