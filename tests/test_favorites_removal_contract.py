"""RQ-20260911-13: 彻底取消音效收藏夹，保留文件夹快捷路径。"""

from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
APP_STATE_H = (ROOT / "src/AppState.h").read_text(encoding="utf-8")
APP_STATE_CPP = (ROOT / "src/AppState.cpp").read_text(encoding="utf-8")
DATABASE_H = (ROOT / "src/Database.h").read_text(encoding="utf-8")
DATABASE_CPP = (ROOT / "src/Database.cpp").read_text(encoding="utf-8")
VERSION_H = (ROOT / "src/Version.h").read_text(encoding="utf-8")
FILTER_H = (ROOT / "src/SearchPresetFilterBuilders.h").read_text(encoding="utf-8")
FILTER_CPP = (ROOT / "src/SearchPresetFilterBuilders.cpp").read_text(encoding="utf-8")
LEDGER = json.loads((ROOT / "locale/harken.ui.strings.json").read_text(encoding="utf-8"))
LEDGER_IDS = {entry["id"] for entry in LEDGER["entries"]}
CMAKE = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
WEBUI_SIDEBAR = (ROOT / "src/webui/src/components/sidebar/SidebarPanel.tsx").read_text(encoding="utf-8")
WEBUI_CHROME = (ROOT / "src/webui/src/components/runtime-twin/RuntimeChrome.tsx").read_text(encoding="utf-8")
WEBUI_FILELIST = (ROOT / "src/webui/src/components/filelist/FileListPanel.tsx").read_text(encoding="utf-8")


def test_contract_is_registered() -> None:
    assert "TestFavoritesRemovalContract" in CMAKE
    assert "tests/test_favorites_removal_contract.py" in CMAKE


def test_three_view_entries_are_gone() -> None:
    assert "View_Favorites" not in MAIN
    assert "View_Favorites" not in LEDGER_IDS
    assert "toolbar.favorites" not in MAIN
    assert "ViewMode::Favorites, true" not in MAIN
    assert "Browser_BookmarkFolder" in LEDGER_IDS
    assert "AddBrowserFolderToQuickPaths" in MAIN


def test_favorite_marks_shortcuts_and_undo_are_gone() -> None:
    assert "ShortcutAction::ToggleFavorite" not in MAIN
    assert "ToggleFavorite occupies the slot immediately after Refresh" in MAIN
    assert "Ctx_AddFavorite" not in MAIN
    assert "Info_Favorited" not in MAIN
    assert "Info_FavoriteOff" not in MAIN
    assert "FavoriteOp" not in (ROOT / "src/UndoSystem.h").read_text(encoding="utf-8")
    assert "UndoOpType::FavoriteToggle" not in (ROOT / "src/UndoSystem.h").read_text(encoding="utf-8")
    assert "IsShortcutPressedSnapshot(ShortcutAction::FocusSearch, ks)" in MAIN
    assert "state.focusSearchBar = true;" in MAIN


def test_view_mode_keeps_numeric_holes_and_maps_legacy_favorites() -> None:
    assert "Browser = 0" in APP_STATE_H
    assert "Database = 1" in APP_STATE_H
    assert "Favorites = 2" in APP_STATE_H
    assert "Playlist = 3" in APP_STATE_H
    assert "PlaylistLibrary = 4" in APP_STATE_H
    assert "MapLegacyViewMode" in MAIN
    assert "legacyViewMode == 2" in MAIN or "savedViewMode == 2" in MAIN
    assert "ViewMode::Database" in MAIN
    load = MAIN[MAIN.index("tab.viewMode =") : MAIN.index("tab.viewMode =") + 400]
    assert "MapLegacyViewMode" in load or "legacy" in load.lower()


def test_shortcut_toggle_favorite_slot_is_kept_for_index_stability() -> None:
    assert "ToggleFavorite," in APP_STATE_H
    assert "ShortcutAction::ToggleFavorite" in APP_STATE_CPP
    assert "g_shortcuts[i].customKey" in MAIN
    assert MAIN.count("PersistedShortcutIndex(g_shortcuts[i].action)") == 2


def test_dependents_and_filters_are_cleared() -> None:
    assert "BuildCurrentFavoritesFilter" not in FILTER_H
    assert "BuildCurrentFavoritesFilter" not in FILTER_CPP
    assert "favoritesOnly" not in DATABASE_H
    assert "randomHasFavorite" not in APP_STATE_H
    assert "Status_FavoritesLabel" not in MAIN
    assert "PLQ_DragToFavoriteHint" not in MAIN
    assert "View_Favorites" not in LEDGER_IDS
    assert "Ctx_AddFavorite" not in LEDGER_IDS
    assert "Sc_ToggleFavorite" not in LEDGER_IDS
    cover_copy = next(e for e in LEDGER["entries"] if e["id"] == "CoverClip_CopyNoView")
    assert "收藏" not in cover_copy["zh"]
    assert "Favorites" not in cover_copy["en"]


def test_database_drops_favorite_column_idempotently() -> None:
    assert "is_favorite     INTEGER DEFAULT 0" not in DATABASE_CPP
    assert "CREATE INDEX IF NOT EXISTS idx_files_favorite" not in DATABASE_CPP
    assert "DROP INDEX IF EXISTS idx_files_favorite" in DATABASE_CPP
    assert "DROP COLUMN is_favorite" in DATABASE_CPP
    assert "columnExists(m_db, \"audio_files\", \"is_favorite\")" in DATABASE_CPP
    assert "bool setFavorite(" not in DATABASE_H
    assert "bool toggleFavorite(" not in DATABASE_H
    assert "getFavorites(" not in DATABASE_H
    assert "getFavoriteCount(" not in DATABASE_H
    assert "FEATURE_FAVORITES" not in VERSION_H


def test_webui_favorites_view_is_gone() -> None:
    assert "'favorites'" not in WEBUI_SIDEBAR
    assert "toggleFavorite" not in WEBUI_FILELIST
    assert "['star', 'Favorites'" not in WEBUI_CHROME
    assert "isFavorite" not in (ROOT / "src/webui/src/types.ts").read_text(encoding="utf-8")
