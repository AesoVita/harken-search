#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct sqlite3;

// @stage7-contract
// API contract: MusicQueue borrows the initialized sqlite3 connection and
// never closes or transfers it. Queue mutations/navigation are synchronous
// owner-thread operations; save/load use the same connection and preserve the
// existing queue metadata format. Returned vectors/IDs are owned values and
// callers must initialize before any other method.
enum class QueueMode {
    Sequential,   // play in order, stop at end
    RepeatAll,    // loop through entire queue
    RepeatOne,    // loop current track
    Shuffle,      // randomized order
};

class MusicQueue {
public:
    MusicQueue() = default;
    ~MusicQueue() = default;

    // Must be called before any other method
    void init(sqlite3* db);

    // Queue management
    void add(int64_t fileId);                    // Append to end
    void remove(int64_t fileId);                 // Remove by file_id
    void clear();                                // Remove all entries
    void insertAt(int64_t fileId, int position); // Insert at specific position

    // Navigation
    int64_t getCurrent() const;  // file_id of current track (-1 if empty)
    int64_t getNext();           // Advance + return next file_id (-1 if end with no repeat)
    int64_t getPrev();           // Go back + return previous file_id (-1 if at start)
    bool    moveTo(int64_t fileId); // Set current to specific file_id (for user click)

    // State
    QueueMode mode() const { return m_mode; }
    void      setMode(QueueMode m);
    int       size() const;
    bool      empty() const { return size() == 0; }
    int       currentIndex() const { return m_currentIndex; }

    // Enumerate queue in play order
    std::vector<int64_t> toVector() const;

    // Persist/restore state from play_queue_meta
    void saveState();
    void loadState();

private:
    sqlite3*  m_db           = nullptr;
    QueueMode m_mode         = QueueMode::Sequential;
    int       m_currentIndex = -1;  // index in position-ordered list; -1 = none

    // Shuffle support: pre-built random permutation of positions
    std::vector<int64_t> m_shuffleOrder;  // file_ids in shuffle order
    int                  m_shuffleIndex  = -1;

    void    rebuildShuffleOrder();
    int64_t fileIdAt(int index) const;  // -1 if out of bounds
    int     positionOf(int64_t fileId) const;  // index into ordered list, -1 if not found
};
