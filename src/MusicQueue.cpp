#include "MusicQueue.h"
#include "../thirdparty/sqlite/sqlite3.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <string>

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void MusicQueue::init(sqlite3* db) {
    m_db = db;
    loadState();
}

// ---------------------------------------------------------------------------
// Queue management
// ---------------------------------------------------------------------------
void MusicQueue::add(int64_t fileId) {
    if (!m_db) return;

    // Compute next position = MAX(position) + 1, or 0 if empty
    int nextPos = 0;
    {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db,
            "SELECT COALESCE(MAX(position)+1, 0) FROM play_queue;",
            -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW)
                nextPos = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
    }

    int64_t now = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
        "INSERT INTO play_queue(position, file_id, added_at) VALUES(?,?,?);",
        -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, nextPos);
        sqlite3_bind_int64(stmt, 2, fileId);
        sqlite3_bind_int64(stmt, 3, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // If this is the first item, set current to it
    if (m_currentIndex == -1 && size() == 1)
        m_currentIndex = 0;

    // Rebuild shuffle order if in shuffle mode
    if (m_mode == QueueMode::Shuffle)
        rebuildShuffleOrder();
}

void MusicQueue::remove(int64_t fileId) {
    if (!m_db) return;

    int removedPos = positionOf(fileId);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
        "DELETE FROM play_queue WHERE file_id = ?;",
        -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, fileId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Adjust m_currentIndex
    if (removedPos >= 0 && m_currentIndex >= 0) {
        if (removedPos < m_currentIndex)
            m_currentIndex--;
        else if (removedPos == m_currentIndex) {
            int s = size();
            if (s == 0)
                m_currentIndex = -1;
            else if (m_currentIndex >= s)
                m_currentIndex = s - 1;
        }
    }

    if (m_mode == QueueMode::Shuffle)
        rebuildShuffleOrder();
}

void MusicQueue::clear() {
    if (!m_db) return;

    sqlite3_exec(m_db, "DELETE FROM play_queue;", nullptr, nullptr, nullptr);
    m_currentIndex = -1;
    m_shuffleOrder.clear();
    m_shuffleIndex = -1;
}

void MusicQueue::insertAt(int64_t fileId, int position) {
    if (!m_db) return;

    // Shift all existing entries at >= position up by 1
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
        "UPDATE play_queue SET position = position + 1 WHERE position >= ?;",
        -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, position);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    int64_t now = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    if (sqlite3_prepare_v2(m_db,
        "INSERT INTO play_queue(position, file_id, added_at) VALUES(?,?,?);",
        -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, position);
        sqlite3_bind_int64(stmt, 2, fileId);
        sqlite3_bind_int64(stmt, 3, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Adjust m_currentIndex if the insertion shifted it
    if (m_currentIndex >= position)
        m_currentIndex++;

    if (m_currentIndex == -1 && size() == 1)
        m_currentIndex = 0;

    if (m_mode == QueueMode::Shuffle)
        rebuildShuffleOrder();
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
int64_t MusicQueue::getCurrent() const {
    return fileIdAt(m_currentIndex);
}

int64_t MusicQueue::getNext() {
    int s = size();
    if (s == 0) return -1;

    if (m_mode == QueueMode::RepeatOne) {
        return getCurrent();
    }

    if (m_mode == QueueMode::Shuffle) {
        if (m_shuffleOrder.empty()) {
            rebuildShuffleOrder();
        }
        int sSize = static_cast<int>(m_shuffleOrder.size());
        if (sSize == 0) return -1;

        // Shuffle always cycles (wrap-around)
        m_shuffleIndex = (m_shuffleIndex + 1) % sSize;
        // Sync m_currentIndex to match
        int64_t fid = m_shuffleOrder[m_shuffleIndex];
        m_currentIndex = positionOf(fid);
        return fid;
    }

    // Sequential or RepeatAll
    m_currentIndex++;
    if (m_currentIndex >= s) {
        if (m_mode == QueueMode::RepeatAll) {
            m_currentIndex = 0;
        } else {
            m_currentIndex = s - 1;
            return -1;
        }
    }
    return fileIdAt(m_currentIndex);
}

int64_t MusicQueue::getPrev() {
    int s = size();
    if (s == 0) return -1;

    if (m_mode == QueueMode::RepeatOne) {
        return getCurrent();
    }

    if (m_mode == QueueMode::Shuffle) {
        if (m_shuffleOrder.empty()) {
            rebuildShuffleOrder();
        }
        int sSize = static_cast<int>(m_shuffleOrder.size());
        if (sSize == 0) return -1;

        // Shuffle always cycles (wrap-around backwards)
        m_shuffleIndex = (m_shuffleIndex - 1 + sSize) % sSize;
        int64_t fid = m_shuffleOrder[m_shuffleIndex];
        m_currentIndex = positionOf(fid);
        return fid;
    }

    // Sequential or RepeatAll
    m_currentIndex--;
    if (m_currentIndex < 0) {
        if (m_mode == QueueMode::RepeatAll) {
            m_currentIndex = s - 1;
        } else {
            m_currentIndex = 0;
            return -1;
        }
    }
    return fileIdAt(m_currentIndex);
}

bool MusicQueue::moveTo(int64_t fileId) {
    int idx = positionOf(fileId);
    if (idx < 0) return false;
    m_currentIndex = idx;

    if (m_mode == QueueMode::Shuffle) {
        // Update shuffle index to match
        auto it = std::find(m_shuffleOrder.begin(), m_shuffleOrder.end(), fileId);
        if (it != m_shuffleOrder.end())
            m_shuffleIndex = static_cast<int>(it - m_shuffleOrder.begin());
    }
    return true;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
void MusicQueue::setMode(QueueMode m) {
    m_mode = m;
    if (m == QueueMode::Shuffle)
        rebuildShuffleOrder();
}

int MusicQueue::size() const {
    if (!m_db) return 0;
    int count = 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
        "SELECT COUNT(*) FROM play_queue;",
        -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return count;
}

// ---------------------------------------------------------------------------
// toVector
// ---------------------------------------------------------------------------
std::vector<int64_t> MusicQueue::toVector() const {
    std::vector<int64_t> result;
    if (!m_db) return result;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
        "SELECT file_id FROM play_queue ORDER BY position ASC;",
        -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW)
            result.push_back(sqlite3_column_int64(stmt, 0));
        sqlite3_finalize(stmt);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Persist / restore
// ---------------------------------------------------------------------------
void MusicQueue::saveState() {
    if (!m_db) return;

    const char* modeStr = "sequential";
    switch (m_mode) {
        case QueueMode::RepeatAll: modeStr = "repeat_all"; break;
        case QueueMode::RepeatOne: modeStr = "repeat_one"; break;
        case QueueMode::Shuffle:   modeStr = "shuffle";    break;
        default:                   modeStr = "sequential"; break;
    }

    std::string idxStr = std::to_string(m_currentIndex);

    sqlite3_stmt* stmt = nullptr;
    // queue_mode
    if (sqlite3_prepare_v2(m_db,
        "INSERT INTO play_queue_meta(key, value) VALUES(?,?)"
        " ON CONFLICT(key) DO UPDATE SET value=excluded.value;",
        -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, "queue_mode", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, modeStr, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    // current_index
    if (sqlite3_prepare_v2(m_db,
        "INSERT INTO play_queue_meta(key, value) VALUES(?,?)"
        " ON CONFLICT(key) DO UPDATE SET value=excluded.value;",
        -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, "current_index", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, idxStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void MusicQueue::loadState() {
    if (!m_db) return;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
        "SELECT key, value FROM play_queue_meta WHERE key IN ('queue_mode','current_index');",
        -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (!key || !val) continue;

            std::string k(key), v(val);
            if (k == "queue_mode") {
                if (v == "repeat_all") m_mode = QueueMode::RepeatAll;
                else if (v == "repeat_one") m_mode = QueueMode::RepeatOne;
                else if (v == "shuffle")    m_mode = QueueMode::Shuffle;
                else                        m_mode = QueueMode::Sequential;
            } else if (k == "current_index") {
                try { m_currentIndex = std::stoi(v); } catch (...) { m_currentIndex = -1; }
            }
        }
        sqlite3_finalize(stmt);
    }

    if (m_mode == QueueMode::Shuffle)
        rebuildShuffleOrder();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void MusicQueue::rebuildShuffleOrder() {
    m_shuffleOrder = toVector();
    std::mt19937 rng(std::random_device{}());
    std::shuffle(m_shuffleOrder.begin(), m_shuffleOrder.end(), rng);

    // Try to keep current track at the front of the shuffle order
    if (m_currentIndex >= 0) {
        int64_t curId = fileIdAt(m_currentIndex);
        auto it = std::find(m_shuffleOrder.begin(), m_shuffleOrder.end(), curId);
        if (it != m_shuffleOrder.end()) {
            std::iter_swap(m_shuffleOrder.begin(), it);
            m_shuffleIndex = 0;
        } else {
            m_shuffleIndex = -1;
        }
    } else {
        m_shuffleIndex = -1;
    }
}

int64_t MusicQueue::fileIdAt(int index) const {
    if (index < 0 || !m_db) return -1;

    sqlite3_stmt* stmt = nullptr;
    int64_t result = -1;
    if (sqlite3_prepare_v2(m_db,
        "SELECT file_id FROM play_queue ORDER BY position ASC LIMIT 1 OFFSET ?;",
        -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, index);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return result;
}

int MusicQueue::positionOf(int64_t fileId) const {
    if (!m_db) return -1;

    // Get the list in order and find the index
    std::vector<int64_t> v = toVector();
    for (int i = 0; i < static_cast<int>(v.size()); i++) {
        if (v[i] == fileId) return i;
    }
    return -1;
}
