#include "MusicAnalyzer.h"
#include "BpmDetector.h"
#include "KeyDetector.h"
#include "MoodAnalyzer.h"
#include <cmath>

void MusicAnalyzer::start(int threadCount) {
    if (!m_threads.empty()) return;
    m_stopping.store(false);
    for (int i = 0; i < threadCount; ++i)
        m_threads.emplace_back(&MusicAnalyzer::workerLoop, this);
}

void MusicAnalyzer::stop() {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_stopping.store(true);
        m_p0.clear(); m_p1.clear(); m_p2.clear();
    }
    m_cv.notify_all();
    for (auto& t : m_threads) if (t.joinable()) t.join();
    m_threads.clear();
}

void MusicAnalyzer::submit(const MusicAnalysisRequest& req) {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        switch (req.priority) {
            case AnalysisPriority::P0: m_p0.push_back(req); break;
            case AnalysisPriority::P1: m_p1.push_back(req); break;
            case AnalysisPriority::P2: m_p2.push_back(req); break;
        }
    }
    m_cv.notify_one();
}

void MusicAnalyzer::cancelByPriority(AnalysisPriority p) {
    std::lock_guard<std::mutex> lk(m_mutex);
    switch (p) {
        case AnalysisPriority::P0: m_p0.clear(); break;
        case AnalysisPriority::P1: m_p1.clear(); break;
        case AnalysisPriority::P2: m_p2.clear(); break;
    }
}

void MusicAnalyzer::cancelAll() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_p0.clear(); m_p1.clear(); m_p2.clear();
}

int MusicAnalyzer::pollResults(std::vector<MusicAnalysisResult>& out, int maxResults) {
    std::lock_guard<std::mutex> lk(m_mutex);
    int added = 0;
    while (!m_results.empty() && added < maxResults) {
        out.push_back(m_results.back());
        m_results.pop_back();
        ++added;
    }
    return added;
}

int MusicAnalyzer::pendingCount() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return (int)(m_p0.size() + m_p1.size() + m_p2.size());
}

void MusicAnalyzer::workerLoop() {
    m_activeWorkers.fetch_add(1);
    while (true) {
        MusicAnalysisRequest req;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this] {
                return m_stopping.load() ||
                       !m_p0.empty() || !m_p1.empty() || !m_p2.empty();
            });
            if (m_stopping.load() && m_p0.empty() && m_p1.empty() && m_p2.empty())
                break;

            if (!m_p0.empty()) {
                req = m_p0.front(); m_p0.pop_front();
            } else if (!m_p1.empty()) {
                req = m_p1.front(); m_p1.pop_front();
            } else if (!m_p2.empty()) {
                if (m_batchCancelFlag.load()) {
                    m_p2.clear();
                    continue;
                }
                req = m_p2.front(); m_p2.pop_front();
            } else {
                continue;
            }
        }

        // Analyse outside the lock
        m_activeJobs.fetch_add(1);
        BpmResult bpm = BpmDetector::detectFile(req.filePath, req.startSec, req.endSec);
        KeyResult key;
        MoodResult mood;
        if (req.runKey) {
            key = KeyDetector::detectFile(req.filePath, req.startSec, req.endSec);
            MoodInput moodIn;
            moodIn.bpm           = bpm.bpm;
            moodIn.bpmConfidence = bpm.confidence;
            moodIn.isMinorKey    = key.isMinor;
            moodIn.keyConfidence = key.confidence;
            mood = MoodAnalyzer::analyze(moodIn);
        }
        m_activeJobs.fetch_sub(1);

        MusicAnalysisResult res;
        res.fileId     = req.fileId;
        res.bpm        = bpm;
        res.key        = key;
        res.mood       = mood;
        res.generation = req.generation;
        res.ok         = bpm.ok || key.ok;
        res.priority   = req.priority;

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_results.push_back(res);
        }
    }
    m_activeWorkers.fetch_sub(1);
}
