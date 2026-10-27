#pragma once

#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>

struct FileEntry {

// @stage7-contract
// API contract: FileEntry and browser snapshots own path strings and metadata;
// browser workers publish copies and never expose iterator-owned storage.
    std::string name;
    std::string fullPath;
    std::string relativePath;  // relative to browser root (recursive mode only)
    std::string extension;
    bool isDirectory = false;
    uint64_t fileSize = 0;
};

enum class BrowserScope {
    CurrentDir,
    RecursiveAudio
};

class FileBrowser {
public:
    FileBrowser();
    ~FileBrowser();

    // Navigation
    void setRootPath(const std::string& path);
    void navigateTo(const std::string& path);
    void navigateUp();
    void refresh();

    // Getters
    const std::string& getCurrentPath() const { return m_currentPath; }
    const std::vector<FileEntry>& getEntries() const { return m_entries; }

    // Scope (CurrentDir vs RecursiveAudio)
    BrowserScope getScope() const { return m_scope; }
    void setScope(BrowserScope scope);
    const std::vector<FileEntry>& activeEntries() const {
        return (m_scope == BrowserScope::RecursiveAudio) ? m_recursiveEntries : m_entries;
    }
    size_t activeAudioFileCount() const {
        return (m_scope == BrowserScope::RecursiveAudio) ? m_recursiveAudioFileCount : m_entriesAudioFileCount;
    }

    // Async current-dir scan — navigateTo()/refresh() launch a background thread.
    // Call tickDirScanResult() each frame; it swaps results in and returns true when done.
    bool isDirScanInProgress() const { return m_dirScanInProgress.load(); }
    bool tickDirScanResult();

    // Async recursive scan — buildRecursiveEntries() launches a background thread.
    // Call tickScanResult() each frame; it swaps results in and returns true when done.
    // isScanInProgress() is true while the thread is running.
    void buildRecursiveEntries();
    bool isScanInProgress() const { return m_scanInProgress.load(); }
    bool tickScanResult();

    // Selection
    void setSelectedIndex(int index);
    int getSelectedIndex() const { return m_selectedIndex; }
    const FileEntry* getSelectedEntry() const;
    // Find entry by full path, select it, return its index (-1 if not found)
    int selectByPath(const std::string& fullPath);

    // Filtering
    void setAudioFilter(bool enabled) { m_audioFilterEnabled = enabled; refresh(); }
    bool isAudioFilterEnabled() const { return m_audioFilterEnabled; }

    // Callbacks
    using FileSelectedCallback = std::function<void(const FileEntry&)>;
    using DirectoryChangedCallback = std::function<void(const std::string&)>;

    void setOnFileSelected(FileSelectedCallback cb) { m_onFileSelected = cb; }
    void setOnDirectoryChanged(DirectoryChangedCallback cb) { m_onDirectoryChanged = cb; }

    // Actions
    void selectCurrent();
    void activateCurrent();  // Double-click behavior

    // Utility
    static bool isAudioFile(const std::string& extension);
    static std::string formatFileSize(uint64_t bytes);

private:
    // Non-blocking cancel helpers — flag scan, reset shared_ptr, detach thread.
    void cancelDirScan();
    void cancelScan();

    // Launch async current-dir scan for m_currentPath into a background thread.
    void startDirScan();

    std::string m_currentPath;
    std::vector<FileEntry> m_entries;
    std::vector<FileEntry> m_recursiveEntries;
    size_t m_entriesAudioFileCount = 0;
    size_t m_recursiveAudioFileCount = 0;
    BrowserScope m_scope = BrowserScope::CurrentDir;
    int m_selectedIndex = -1;
    bool m_audioFilterEnabled = true;

    FileSelectedCallback m_onFileSelected;
    DirectoryChangedCallback m_onDirectoryChanged;

    // ScanState is owned jointly by FileBrowser and the scan thread via shared_ptr,
    // so the thread can finish safely even if FileBrowser is destroyed or a new scan
    // is started (cancel resets the shared_ptr without joining).
    struct ScanState {
        std::atomic<bool> cancelled{false};
        std::mutex mutex;
        std::vector<FileEntry> entries;
        size_t audioFileCount{0};
        bool ready{false};
    };

    // Async current-dir scan state (m_entries).
    std::shared_ptr<ScanState> m_dirActiveScan;
    std::thread m_dirScanThread;
    std::atomic<bool> m_dirScanInProgress{false};

    // Async recursive scan state (m_recursiveEntries).
    std::shared_ptr<ScanState> m_activeScan;
    std::thread m_scanThread;
    std::atomic<bool> m_scanInProgress{false};
};
