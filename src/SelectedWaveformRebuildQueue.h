#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

// @stage7-contract
// API contract: SelectedWaveformRebuildQueue owns a deduplicated copy of its
// item list and is confined to the caller/UI thread. pump invokes the borrowed
// callback synchronously for at most maxItems and retains neither callback nor
// item references; completion clears queue storage.
struct SelectedWaveformRebuildItem {
    int64_t id = 0;
    std::string filePath;
};

class SelectedWaveformRebuildQueue {
public:
    void start(const std::vector<SelectedWaveformRebuildItem>& items)
    {
        items_.clear();
        nextIndex_ = 0;

        std::set<std::string> seenPaths;
        items_.reserve(items.size());
        for (const auto& item : items) {
            if (item.filePath.empty()) continue;
            if (!seenPaths.insert(item.filePath).second) continue;
            items_.push_back(item);
        }
    }

    void clear()
    {
        items_.clear();
        nextIndex_ = 0;
    }

    bool enqueue(const SelectedWaveformRebuildItem& item)
    {
        if (item.filePath.empty()) return false;
        for (const auto& existing : items_) {
            if (existing.filePath == item.filePath) return false;
        }
        items_.push_back(item);
        return true;
    }

    bool active() const
    {
        return nextIndex_ < items_.size();
    }

    size_t remaining() const
    {
        return active() ? (items_.size() - nextIndex_) : 0;
    }

    size_t total() const
    {
        return items_.size();
    }

    template <typename Fn>
    size_t pump(size_t maxItems, Fn&& fn)
    {
        if (maxItems == 0 || !active()) return 0;

        const size_t begin = nextIndex_;
        const size_t end = std::min(items_.size(), nextIndex_ + maxItems);
        for (; nextIndex_ < end; ++nextIndex_) {
            fn(items_[nextIndex_]);
        }
        const size_t processed = end - begin;
        if (!active()) clear();
        return processed;
    }

private:
    std::vector<SelectedWaveformRebuildItem> items_;
    size_t nextIndex_ = 0;
};
