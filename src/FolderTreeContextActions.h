#pragma once

#include <string>

// @stage7-contract
// API contract: folder-tree context helpers synchronously mutate only the
// caller-owned state fields required by their template contract. Path/display
// strings are copied; empty/duplicate paths are no-ops and no state references
// are retained after return.

template <typename TState>
inline bool AddFolderShortcutIfMissing(TState& state,
                                       const std::string& displayName,
                                       const std::string& path) {
    if (path.empty()) return false;
    for (const auto& shortcut : state.folderShortcuts) {
        if (shortcut.path == path) return false;
    }
    state.folderShortcuts.push_back({displayName, path});
    return true;
}

template <typename TState>
inline void PrepareQuickScanDialogForPath(TState& state,
                                          const std::string& path,
                                          bool autoStart = false) {
    if (path.empty() || state.isScanning.load()) return;
    state.pendingScanFolders.clear();
    state.pendingScanFolders.push_back(path);
    state.scanDialogAutoStart = autoStart;
    state.showScanDialog = true;
}
