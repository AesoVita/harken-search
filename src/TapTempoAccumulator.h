#pragma once
#include <chrono>
#include <vector>

// Collects user key-press timestamps and computes BPM from inter-tap intervals.
// Call tap() on each key press; getBpm() returns the rolling average once hasResult() is true.
// Resets automatically if more than kTimeoutSec pass between taps.
// @stage7-contract
// API contract: TapTempoAccumulator owns its timestamp history and is confined
// to the caller/UI thread. tap/reset mutate only that history; query methods
// return copied values and use steady-clock time. No callbacks, threads, or
// external state are retained.
class TapTempoAccumulator {
public:
    void tap() {
        auto now = std::chrono::steady_clock::now();
        if (!m_taps.empty()) {
            double gap = std::chrono::duration<double>(now - m_taps.back()).count();
            if (gap > kTimeoutSec)
                m_taps.clear();
        }
        m_taps.push_back(now);
    }

    void reset() { m_taps.clear(); }

    bool hasResult() const { return static_cast<int>(m_taps.size()) >= kMinTaps; }

    float getBpm() const {
        if (static_cast<int>(m_taps.size()) < kMinTaps) return 0.0f;
        double totalSec = std::chrono::duration<double>(
                              m_taps.back() - m_taps.front()).count();
        if (totalSec <= 0.0) return 0.0f;
        double avgInterval = totalSec / static_cast<double>(m_taps.size() - 1);
        return static_cast<float>(60.0 / avgInterval);
    }

    bool isExpired() const {
        if (m_taps.empty()) return true;
        double gap = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - m_taps.back()).count();
        return gap > kTimeoutSec;
    }

    int tapCount() const { return static_cast<int>(m_taps.size()); }

private:
    static constexpr int    kMinTaps    = 3;
    static constexpr double kTimeoutSec = 2.0;
    std::vector<std::chrono::steady_clock::time_point> m_taps;
};
