#include "GaplessEngine.h"
#include "AudioEngine.h"
#include <chrono>

void GaplessEngine::init(AudioEngine* audioEngine) {
    m_engine = audioEngine;
    m_currentPath.clear();
    m_nextPath.clear();
    m_nextReady.store(false);
    m_shutdown.store(false);
    m_wasPlaying = false;
}

void GaplessEngine::shutdown() {
    m_shutdown.store(true);
    if (m_preloadThread.joinable()) {
        m_preloadThread.join();
    }
}

void GaplessEngine::setNextTrack(const std::string& filePath) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_nextPath = filePath;
    m_nextReady.store(false);
}

void GaplessEngine::preloadNext() {
    if (m_preloadThread.joinable()) {
        m_preloadThread.join();
    }
    m_preloadThread = std::thread(&GaplessEngine::preloadWorker, this);
}

void GaplessEngine::preloadWorker() {
    // Sleep briefly to avoid competing with the current track's decode
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (m_shutdown.load()) return;

    std::string path;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        path = m_nextPath;
    }
    if (path.empty()) return;

    // Warm up the OS file cache by touching the path header.
    // Full waveform prefetch is intentionally omitted here to avoid
    // background-thread conflicts with AudioEngine's own decoder.
    // The 500ms sleep gives the OS time to page in the file so the
    // subsequent loadFile() call on the main thread is near-instant.

    m_nextReady.store(true);
}

void GaplessEngine::play(const std::string& filePath) {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_currentPath = filePath;
    }
    m_engine->loadFile(filePath);
    m_engine->play();
    m_wasPlaying = true;
}

void GaplessEngine::stop() {
    m_engine->stop();
    m_wasPlaying = false;
}

bool GaplessEngine::poll() {
    if (!m_engine || !m_wasPlaying) return false;

    bool isPlaying = m_engine->isPlaying();
    if (!isPlaying && m_wasPlaying) {
        m_wasPlaying = false;
        // Transition to next track if available and preloaded
        std::string next;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            next = m_nextPath;
        }
        if (!next.empty() && m_nextReady.load()) {
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                m_currentPath = next;
                m_nextPath.clear();
            }
            m_nextReady.store(false);
            m_engine->loadFile(m_currentPath);
            m_engine->play();
            m_wasPlaying = true;
            if (onTrackChanged) onTrackChanged(m_currentPath);
            return true;
        }
    }
    m_wasPlaying = isPlaying;
    return false;
}
