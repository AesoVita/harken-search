#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

class AudioEngine;

// GaplessEngine — near-gapless playback by preloading the next track.
// Does NOT own or modify AudioEngine internals. Wraps it.
// Call poll() every frame from the main thread to detect track end and trigger transitions.
// @stage7-contract
// API contract: GaplessEngine borrows AudioEngine and owns its preload thread
// and readiness state. init/shutdown/play/stop/setNextTrack are owner-thread
// lifecycle operations; preloadWorker performs background cache work and
// onTrackChanged is observed on the main-thread transition path. Callers
// must shutdown before destroying the borrowed engine.
class GaplessEngine {
public:
    GaplessEngine() = default;
    ~GaplessEngine() { shutdown(); }

    // Must be called before use. Does not take ownership of audioEngine.
    void init(AudioEngine* audioEngine);
    void shutdown();

    // Tell GaplessEngine what to play next (called when user changes queue)
    void setNextTrack(const std::string& filePath);

    // Trigger preload of next track into memory cache (call after setNextTrack)
    void preloadNext();

    // Call every frame from main thread.
    // Returns true if a track transition just occurred (caller should update UI)
    bool poll();

    // Start playing current track (delegates to AudioEngine)
    void play(const std::string& filePath);

    // Stop playback
    void stop();

    // True if next track is ready to transition without gap
    bool isNextReady() const { return m_nextReady.load(); }

    // Callback: called on main thread when track transition occurs
    // Signature: void(const std::string& newFilePath)
    std::function<void(const std::string&)> onTrackChanged;

private:
    AudioEngine*      m_engine       = nullptr;
    std::string       m_currentPath;
    std::string       m_nextPath;
    std::atomic<bool> m_nextReady{false};
    std::atomic<bool> m_shutdown{false};
    std::thread       m_preloadThread;
    std::mutex        m_mutex;

    bool m_wasPlaying = false;  // state from last poll()

    void preloadWorker();
};
