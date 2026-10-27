#include "DiagnosticFileLifecycle.h"

#include "platform/filesystem.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace HarkenSearch {

DiagnosticFileLifecycle::DiagnosticFileLifecycle(std::string pathUtf8,
                                                 std::size_t maxBytes,
                                                 std::size_t alignmentBytes)
    : pathUtf8_(std::move(pathUtf8)),
      maxBytes_(maxBytes),
      alignmentBytes_(std::max<std::size_t>(alignmentBytes, 1)) {
    const auto existing = Platform::FileSystem::ProbeRegularFile(pathUtf8_);
    cachedSizeBytes_.store(existing.isRegularFile && existing.sizeAvailable
        ? static_cast<std::size_t>(existing.sizeBytes)
        : 0, std::memory_order_release);
}

void DiagnosticFileLifecycle::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    enabled_ = enabled;
}

bool DiagnosticFileLifecycle::isEnabled() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return enabled_;
}

bool DiagnosticFileLifecycle::append(std::string_view bytes) {
    return appendAtGeneration(bytes, generation());
}

std::uint64_t DiagnosticFileLifecycle::generation() const {
    return clearGeneration_.load(std::memory_order_acquire);
}

bool DiagnosticFileLifecycle::appendAtGeneration(std::string_view bytes,
                                                 std::uint64_t expectedGeneration) {
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        if (!enabled_) return false;
    }
    std::lock_guard<std::mutex> ioLock(ioMutex_);
    if (expectedGeneration != clearGeneration_.load(std::memory_order_relaxed)) return false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        if (!enabled_) return false;
    }
    if (pathUtf8_.empty() || maxBytes_ == 0) return false;

    const std::size_t current = cachedSizeBytes_.load(std::memory_order_relaxed);
    const auto trimToAlignment = [this](std::size_t value) {
        if (alignmentBytes_ <= 1) return value;
        return value - (value % alignmentBytes_);
    };
    const auto skipLeadingUtf16LowSurrogate = [this](const std::uint8_t* data,
                                                      std::size_t start,
                                                      std::size_t size) {
        if (alignmentBytes_ != sizeof(std::uint16_t) || start + 1 >= size) return start;
        const std::uint16_t codeUnit = static_cast<std::uint16_t>(data[start]) |
            static_cast<std::uint16_t>(data[start + 1] << 8);
        return codeUnit >= 0xDC00 && codeUnit <= 0xDFFF ? start + sizeof(std::uint16_t) : start;
    };
    const auto trimTrailingUtf16HighSurrogate = [this](const std::uint8_t* data,
                                                        std::size_t size) {
        if (alignmentBytes_ != sizeof(std::uint16_t) || size < sizeof(std::uint16_t)) {
            return size;
        }
        const std::size_t offset = size - sizeof(std::uint16_t);
        const std::uint16_t codeUnit = static_cast<std::uint16_t>(data[offset]) |
            static_cast<std::uint16_t>(data[offset + 1] << 8);
        return codeUnit >= 0xD800 && codeUnit <= 0xDBFF ? offset : size;
    };
    std::size_t writable = trimToAlignment(std::min(bytes.size(), maxBytes_));
    writable = trimTrailingUtf16HighSurrogate(
        reinterpret_cast<const std::uint8_t*>(bytes.data()), writable);
    const bool currentAligned = alignmentBytes_ <= 1 || current % alignmentBytes_ == 0;
    const bool fits = currentAligned && current <= maxBytes_ && writable <= maxBytes_ - current;
    Platform::FileSystem::StreamWriter writer;
    if (fits) {
        if (!writer.Open(pathUtf8_, true, true)) return false;
        const std::int64_t actualPosition = writer.Tell();
        const bool actualFits = actualPosition >= 0 &&
            static_cast<std::uint64_t>(actualPosition) <= maxBytes_ &&
            writable <= maxBytes_ - static_cast<std::size_t>(actualPosition);
        if (actualFits) {
            const std::size_t actualCurrent = static_cast<std::size_t>(actualPosition);
            const bool written = writable == 0 || writer.Write(bytes.substr(0, writable));
            writer.Close();
            if (written) {
                cachedSizeBytes_.store(actualCurrent + writable, std::memory_order_release);
            }
            return written;
        }
        writer.Close();
    }

    const std::vector<std::uint8_t> existingBytes = Platform::FileSystem::ReadFileBytes(pathUtf8_);
    std::size_t incomingStart = bytes.size() > maxBytes_ ? bytes.size() - maxBytes_ : 0;
    if (alignmentBytes_ > 1 && incomingStart % alignmentBytes_ != 0) {
        incomingStart += alignmentBytes_ - (incomingStart % alignmentBytes_);
    }
    if (incomingStart > bytes.size()) incomingStart = bytes.size();
    incomingStart = skipLeadingUtf16LowSurrogate(
        reinterpret_cast<const std::uint8_t*>(bytes.data()), incomingStart, bytes.size());
    std::size_t incomingBytes = trimToAlignment(bytes.size() - incomingStart);
    incomingBytes = trimTrailingUtf16HighSurrogate(
        reinterpret_cast<const std::uint8_t*>(bytes.data() + incomingStart), incomingBytes);
    // Compact to roughly half capacity instead of rewriting a full max-sized
    // file for every subsequent line.  Large incoming records still get all
    // remaining capacity.
    const std::size_t alignedExistingSize = trimToAlignment(existingBytes.size());
    const std::size_t keepCapacity = trimToAlignment(maxBytes_ - incomingBytes);
    const std::size_t keepExisting = std::min(
        alignedExistingSize,
        std::min(trimToAlignment(maxBytes_ / 2), keepCapacity));
    std::size_t existingStart = alignedExistingSize > keepExisting
        ? alignedExistingSize - keepExisting
        : 0;
    existingStart = skipLeadingUtf16LowSurrogate(existingBytes.data(), existingStart,
                                                  alignedExistingSize);
    std::size_t keepAligned = alignedExistingSize - existingStart;
    keepAligned = trimTrailingUtf16HighSurrogate(existingBytes.data() + existingStart,
                                                  keepAligned);
    const std::string temporaryPath = pathUtf8_ + ".tmp";
    if (!writer.Open(temporaryPath, false, true)) return false;
    const bool existingWritten = keepAligned == 0 || existingBytes.empty() ||
        writer.Write(std::string_view(reinterpret_cast<const char*>(existingBytes.data() + existingStart),
                                      keepAligned));
    const bool incomingWritten = existingWritten &&
        (incomingBytes == 0 || writer.Write(bytes.substr(incomingStart, incomingBytes)));
    const bool flushed = existingWritten && incomingWritten && writer.Flush();
    writer.Close();
    const bool written = flushed &&
        Platform::FileSystem::AtomicFileReplace(pathUtf8_, temporaryPath);
    if (!written) {
        Platform::FileSystem::RemoveFile(temporaryPath);
        return false;
    }
    cachedSizeBytes_.store(keepAligned + incomingBytes, std::memory_order_release);
    return true;
}

std::size_t DiagnosticFileLifecycle::sizeBytes() const {
    return cachedSizeBytes_.load(std::memory_order_acquire);
}

bool DiagnosticFileLifecycle::clear() {
    clearGeneration_.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard<std::mutex> ioLock(ioMutex_);
    std::string error;
    if (!Platform::FileSystem::RemoveFile(pathUtf8_, &error)) {
        const bool gone = !Platform::FileSystem::Exists(pathUtf8_);
        if (gone) {
            cachedSizeBytes_.store(0, std::memory_order_release);
        }
        return gone;
    }
    cachedSizeBytes_.store(0, std::memory_order_release);
    return true;
}

} // namespace HarkenSearch
