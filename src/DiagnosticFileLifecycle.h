#pragma once

#include <cstddef>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace HarkenSearch {

class DiagnosticFileLifecycle {
public:
    DiagnosticFileLifecycle(std::string pathUtf8,
                            std::size_t maxBytes,
                            std::size_t alignmentBytes = 1);

    void setEnabled(bool enabled);
    bool isEnabled() const;
    bool append(std::string_view bytes);
    std::uint64_t generation() const;
    bool appendAtGeneration(std::string_view bytes, std::uint64_t generation);
    std::size_t sizeBytes() const;
    std::size_t maxBytes() const { return maxBytes_; }
    bool clear();

private:
    std::string pathUtf8_;
    std::size_t maxBytes_;
    std::size_t alignmentBytes_;
    mutable std::mutex stateMutex_;
    mutable std::mutex ioMutex_;
    std::atomic<std::uint64_t> clearGeneration_{0};
    std::atomic<std::size_t> cachedSizeBytes_{0};
    bool enabled_ = false;
};

} // namespace HarkenSearch
