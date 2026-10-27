#pragma once
#include "BpmDetector.h"
#include "KeyDetector.h"
#include "MoodAnalyzer.h"
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <cstdint>

// @stage7-contract
// API contract: MusicAnalyzer owns worker threads, queued requests, and
// returned results. start/stop/cancel control lifecycle; submit copies the
// request, pollResults moves owned results into the caller vector, and batch
// atomics are observations/control flags for the owning scan workflow. Calls
// retain no caller buffers and must not outlive stop.
enum class AnalysisPriority { P0 = 0, P1 = 1, P2 = 2 };

struct MusicAnalysisRequest {
    int64_t           fileId     = -1;
    std::string       filePath;
    AnalysisPriority  priority   = AnalysisPriority::P1;
    uint64_t          generation = 0;
    double            startSec   = 0.0;
    double            endSec     = -1.0;   // -1 = full file
    bool              runKey     = true;   // also detect musical key
};

struct MusicAnalysisResult {
    int64_t          fileId     = -1;
    BpmResult        bpm;
    KeyResult        key;
    MoodResult       mood;
    uint64_t         generation = 0;
    bool             ok         = false;
    AnalysisPriority priority   = AnalysisPriority::P1;
};

class MusicAnalyzer {
public:
    MusicAnalyzer() = default;
    ~MusicAnalyzer() { stop(); }

    void start(int threadCount = 2);
    void stop();

    void submit(const MusicAnalysisRequest& req);
    void cancelByPriority(AnalysisPriority p);
    void cancelAll();

    // Drain completed results into `out` (up to maxResults). Returns count added.
    int  pollResults(std::vector<MusicAnalysisResult>& out, int maxResults = 8);

    bool isAnalyzing()  const { return m_activeJobs.load() > 0; }
    int  pendingCount() const;

    // Batch scan state (T3, accessed directly for atomic operations)
    std::atomic<int>  m_batchTotal{0};
    std::atomic<int>  m_batchDone{0};   // total processed (progress bar numerator)
    std::atomic<int>  m_batchFound{0};  // successfully detected BPM (completion message)
    std::atomic<bool> m_batchRunning{false};
    std::atomic<bool> m_batchCancelFlag{false};

    bool batchRunning() const { return m_batchRunning.load(); }
    int  batchDone()    const { return m_batchDone.load(); }
    int  batchTotal()   const { return m_batchTotal.load(); }
    int  batchFound()   const { return m_batchFound.load(); }
    void batchCancel()  {
        m_batchCancelFlag.store(true);
        m_batchRunning.store(false);
        cancelByPriority(AnalysisPriority::P2);
        // Reset flag after queue is cleared so next batch starts clean
        m_batchCancelFlag.store(false);
    }

private:
    void workerLoop();

    std::deque<MusicAnalysisRequest> m_p0, m_p1, m_p2;
    std::vector<MusicAnalysisResult> m_results;

    mutable std::mutex       m_mutex;
    std::condition_variable  m_cv;
    std::vector<std::thread> m_threads;

    std::atomic<bool> m_stopping{false};
    std::atomic<int>  m_activeWorkers{0};
    std::atomic<int>  m_activeJobs{0};    // jobs in-flight (not thread count)
};
