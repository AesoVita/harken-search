#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// @stage7-contract
// API contract: MiniWaveformScheduler owns queued/visible keys and is confined
// to its caller thread. Scheduling copies/moves paths into the queue, popNext
// transfers an owned key through out, and trim/remove/clear mutate only local
// state. It starts no workers and retains no external references.
struct MiniWaveformTextureKey {
    std::string path;
    int width = 0;
    int height = 0;

    bool operator==(const MiniWaveformTextureKey& other) const noexcept
    {
        return path == other.path && width == other.width && height == other.height;
    }
};

struct MiniWaveformTextureKeyHash {
    std::size_t operator()(const MiniWaveformTextureKey& key) const noexcept
    {
        std::size_t seed = std::hash<std::string>{}(key.path);
        HashCombine(seed, std::hash<int>{}(key.width));
        HashCombine(seed, std::hash<int>{}(key.height));
        return seed;
    }

private:
    static void HashCombine(std::size_t& seed, std::size_t value) noexcept
    {
        seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    }
};

class MiniWaveformScheduler {
public:
    bool schedule(std::string path, int width, int height)
    {
        MiniWaveformTextureKey key{std::move(path), width, height};
        if (pendingSet_.find(key) != pendingSet_.end())
        {
            return false;
        }

        pendingSet_.insert(key);
        incrementPath(key.path);
        pending_.push_back(std::move(key));
        return true;
    }

    bool scheduleFront(std::string path, int width, int height)
    {
        MiniWaveformTextureKey key{std::move(path), width, height};
        const bool inserted = pendingSet_.insert(key).second;
        if (!inserted)
        {
            pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
        }
        else
        {
            incrementPath(key.path);
        }

        pending_.push_front(std::move(key));
        return inserted;
    }

    bool scheduleFrontCoalescedByPath(std::string path, int width, int height)
    {
        MiniWaveformTextureKey key{std::move(path), width, height};
        if (pendingSet_.find(key) != pendingSet_.end())
        {
            pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
            pending_.push_front(std::move(key));
            return false;
        }

        if (containsPath(key.path))
        {
            removePath(key.path);
        }

        pendingSet_.insert(key);
        incrementPath(key.path);
        pending_.push_front(std::move(key));
        return true;
    }

    bool scheduleBackIfPathAbsent(std::string path, int width, int height)
    {
        if (containsPath(path))
        {
            return false;
        }

        return schedule(std::move(path), width, height);
    }

    bool popNext(MiniWaveformTextureKey& out)
    {
        if (pending_.empty())
        {
            return false;
        }

        out = pending_.front();
        pendingSet_.erase(out);
        decrementPath(out.path);
        pending_.pop_front();
        return true;
    }

    void remove(const MiniWaveformTextureKey& key)
    {
        if (pendingSet_.erase(key) == 0)
        {
            return;
        }

        decrementPath(key.path);
        pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
    }

    void clear()
    {
        pending_.clear();
        pendingSet_.clear();
        pathCounts_.clear();
        visibleSet_.clear();
    }

    void setVisibleKeys(std::vector<MiniWaveformTextureKey> keys)
    {
        visibleSet_.clear();
        visibleSet_.reserve(keys.size());
        for (auto& key : keys)
        {
            visibleSet_.insert(std::move(key));
        }
    }

    bool isVisible(const MiniWaveformTextureKey& key) const
    {
        return visibleSet_.find(key) != visibleSet_.end();
    }

    void trimExcess(std::size_t maxPending)
    {
        if (pending_.size() <= maxPending)
        {
            return;
        }

        std::size_t dropsNeeded = pending_.size() - maxPending;
        std::deque<MiniWaveformTextureKey> kept;

        for (auto& key : pending_)
        {
            if (dropsNeeded > 0 && !isVisible(key))
            {
                pendingSet_.erase(key);
                decrementPath(key.path);
                --dropsNeeded;
                continue;
            }

            kept.push_back(std::move(key));
        }

        while (dropsNeeded > 0 && !kept.empty())
        {
            pendingSet_.erase(kept.front());
            decrementPath(kept.front().path);
            kept.pop_front();
            --dropsNeeded;
        }

        pending_.swap(kept);
    }

    void trimBackExcess(std::size_t maxPending)
    {
        while (pending_.size() > maxPending)
        {
            pendingSet_.erase(pending_.back());
            decrementPath(pending_.back().path);
            pending_.pop_back();
        }
    }

    std::size_t pendingCount() const noexcept
    {
        return pending_.size();
    }

    bool contains(const MiniWaveformTextureKey& key) const
    {
        return pendingSet_.find(key) != pendingSet_.end();
    }

    bool containsPath(const std::string& path) const
    {
        return pathCounts_.find(path) != pathCounts_.end();
    }

private:
    void incrementPath(const std::string& path)
    {
        ++pathCounts_[path];
    }

    void decrementPath(const std::string& path)
    {
        auto it = pathCounts_.find(path);
        if (it == pathCounts_.end())
        {
            return;
        }

        if (it->second <= 1)
        {
            pathCounts_.erase(it);
            return;
        }

        --it->second;
    }

    void removePath(const std::string& path)
    {
        if (!containsPath(path))
        {
            return;
        }

        for (auto it = pending_.begin(); it != pending_.end();)
        {
            if (it->path == path)
            {
                pendingSet_.erase(*it);
                decrementPath(it->path);
                it = pending_.erase(it);
                continue;
            }

            ++it;
        }
    }

    std::deque<MiniWaveformTextureKey> pending_;
    std::unordered_set<MiniWaveformTextureKey, MiniWaveformTextureKeyHash> pendingSet_;
    std::unordered_map<std::string, std::size_t> pathCounts_;
    std::unordered_set<MiniWaveformTextureKey, MiniWaveformTextureKeyHash> visibleSet_;
};
