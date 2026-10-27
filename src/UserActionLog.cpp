#include "UserActionLog.h"
#include "platform/filesystem.h"
#include <cstring>
#include <cstdarg>
#include <algorithm>
#include <sstream>
#include <iomanip>

// Singleton

UserActionLog& UserActionLog::instance() {
    static UserActionLog inst;
    return inst;
}

UserActionLog::UserActionLog()
    : m_sessionStart(std::chrono::steady_clock::now())
    , m_lastAction(m_sessionStart)
{
    m_ring.resize(kMaxEntries);
}

UserActionLog::~UserActionLog() {
    m_file.Close();
}

// Control

void UserActionLog::setEnabled(bool on) {
    m_enabled.store(on, std::memory_order_relaxed);
    if (on) {
        log(UACategory::System, "UserActionLog enabled");
    }
}

void UserActionLog::setOutputPath(const std::string& pathUtf8) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_outputPath = pathUtf8;

    m_file.Close();
    if (!pathUtf8.empty() && m_file.Open(pathUtf8, false, false)) {
        m_file.Write(
            "# Aris User Action Log\n"
            "# Format: [session_time] +elapsed  CATEGORY   detail\n"
            "# --------------------------------------------------------\n");
        m_file.Flush();
    }
}

// Category names

const char* UserActionLog::categoryName(UACategory cat) {
    switch (cat) {
        case UACategory::Search:   return "SEARCH";
        case UACategory::Filter:   return "FILTER";
        case UACategory::Sort:     return "SORT";
        case UACategory::Select:   return "SELECT";
        case UACategory::Play:     return "PLAY";
        case UACategory::Seek:     return "SEEK";
        case UACategory::Navigate: return "NAV";
        case UACategory::Edit:     return "EDIT";
        case UACategory::Export:   return "EXPORT";
        case UACategory::Shortcut: return "KEY";
        case UACategory::UI:       return "UI";
        case UACategory::System:   return "SYSTEM";
    }
    return "?";
}

// Logging

int64_t UserActionLog::nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void UserActionLog::log(UACategory cat, const char* detail) {
    if (!m_enabled.load(std::memory_order_relaxed)) return;

    auto now = std::chrono::steady_clock::now();

    UAEntry entry{};
    entry.category = cat;
    entry.timestampMs = nowMs();

    // Safe string copy with truncation
    if (detail) {
        size_t len = strlen(detail);
        if (len >= sizeof(entry.detail)) len = sizeof(entry.detail) - 1;
        memcpy(entry.detail, detail, len);
        entry.detail[len] = '\0';
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        double sessionS = std::chrono::duration<double>(now - m_sessionStart).count();
        double elapsedS = std::chrono::duration<double>(now - m_lastAction).count();
        entry.sessionTimeS = sessionS;
        entry.elapsedSinceLastS = elapsedS;
        m_lastAction = now;

        // Ring buffer write
        m_ring[m_head] = entry;
        m_head = (m_head + 1) % kMaxEntries;
        if (m_count < kMaxEntries) m_count++;

        // Flush to file
        flushEntry(entry);
    }
}

void UserActionLog::logFmt(UACategory cat, const char* fmt, ...) {
    if (!m_enabled.load(std::memory_order_relaxed)) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    log(cat, buf);
}

void UserActionLog::flushEntry(const UAEntry& entry) {
    // Must be called under m_mutex
    if (!m_file.IsOpen()) return;

    int minutes = (int)(entry.sessionTimeS / 60.0);
    double secs = entry.sessionTimeS - minutes * 60.0;

    char line[512];
    std::snprintf(line, sizeof(line), "[%02d:%06.3f] +%.3fs  %-8s %s\n",
        minutes, secs,
        entry.elapsedSinceLastS,
        categoryName(entry.category),
        entry.detail);
    m_file.Write(line);
    m_file.Flush();
}

// Export

size_t UserActionLog::entryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_count;
}

void UserActionLog::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_count = 0;
    m_head = 0;
    m_lastAction = std::chrono::steady_clock::now();
}

int UserActionLog::copyRecent(UAEntry* out, int maxOut) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int n = (int)std::min((size_t)maxOut, m_count);
    for (int i = 0; i < n; i++) {
        size_t idx = (m_head + kMaxEntries - n + i) % kMaxEntries;
        out[i] = m_ring[idx];
    }
    return n;
}

std::string UserActionLog::exportTimelineString() const {
    return exportRecentString((int)kMaxEntries);
}

std::string UserActionLog::exportRecentString(int n) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = (int)std::min((size_t)n, m_count);

    std::ostringstream ss;
    ss << "# Aris User Action Timeline (" << count << " actions)\n";
    ss << "# [session_time] +elapsed  CATEGORY   detail\n";
    ss << "# --------------------------------------------------------\n";

    for (int i = 0; i < count; i++) {
        size_t idx = (m_head + kMaxEntries - count + i) % kMaxEntries;
        const auto& e = m_ring[idx];

        int minutes = (int)(e.sessionTimeS / 60.0);
        double secs = e.sessionTimeS - minutes * 60.0;

        char line[512];
        snprintf(line, sizeof(line), "[%02d:%06.3f] +%.3fs  %-8s %s\n",
            minutes, secs,
            e.elapsedSinceLastS,
            categoryName(e.category),
            e.detail);
        ss << line;
    }
    return ss.str();
}

bool UserActionLog::exportTimeline(const std::string& pathUtf8) const {
    std::string content = exportTimelineString();
    return Platform::FileSystem::WriteFileText(pathUtf8, content);
}
