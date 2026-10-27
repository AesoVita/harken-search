#include <cstdio>
#include <fstream>
#include <string>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* desc) {
    if (cond) {
        std::printf("PASS: %s\n", desc);
        ++g_pass;
    } else {
        std::printf("FAIL: %s\n", desc);
        ++g_fail;
    }
}

static std::string readTextFile(const char* filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return {};
    std::string out;
    in.seekg(0, std::ios::end);
    out.resize(static_cast<size_t>(in.tellg()));
    in.seekg(0, std::ios::beg);
    in.read(&out[0], static_cast<std::streamsize>(out.size()));
    return out;
}

static std::string extractFunctionBody(const std::string& source,
                                       const std::string& signature) {
    const size_t sig = source.find(signature);
    if (sig == std::string::npos) return {};
    const size_t open = source.find('{', sig);
    if (open == std::string::npos) return {};

    int depth = 0;
    for (size_t i = open; i < source.size(); ++i) {
        if (source[i] == '{') {
            ++depth;
        } else if (source[i] == '}') {
            --depth;
            if (depth == 0) {
                return source.substr(open, i - open + 1);
            }
        }
    }
    return {};
}

static bool occursBefore(const std::string& body,
                         const std::string& earlier,
                         const std::string& later) {
    const size_t left = body.find(earlier);
    const size_t right = body.find(later);
    return left != std::string::npos &&
           right != std::string::npos &&
           left < right;
}

int main() {
#ifdef AURA_SOURCE_DIR
    const char* mainCpp = AURA_SOURCE_DIR "/src/main.cpp";
    const char* audioEngineCpp = AURA_SOURCE_DIR "/src/AudioEngine.cpp";
#else
    const char* mainCpp = "src/main.cpp";
    const char* audioEngineCpp = "src/AudioEngine.cpp";
#endif

    const std::string mainSource = readTextFile(mainCpp);
    check(!mainSource.empty(), "main.cpp source is readable");
    check(mainSource.find("OpenDatabaseAndRestart(") == std::string::npos,
          "SEMANTIC: user-triggered DB switching does not expose the legacy relaunch helper");
    check(mainSource.find("SwitchDatabase(state, openPath);") != std::string::npos,
          "SEMANTIC: open-database UI path uses the in-process switch flow");
    check(mainSource.find("SwitchDatabase(state, p);") != std::string::npos,
          "SEMANTIC: Ctrl+O/menu open-database paths use the in-process switch flow");

    const std::string switchBody = extractFunctionBody(
        mainSource,
        "static bool SwitchDatabase(AppState& state,");
    check(!switchBody.empty(), "SwitchDatabase body can be inspected");

    const std::string pumpBody = extractFunctionBody(
        mainSource,
        "void PumpDatabaseSwitchIfPending(AppState& state)");
    check(!pumpBody.empty(), "PumpDatabaseSwitchIfPending body can be inspected");

    if (!switchBody.empty()) {
        const std::string threadSpawn =
            "g_databaseSwitchWorkerRuntime.Start(";

        check(switchBody.find("state.audioEngine.discardLoadedFileForDatabaseBoundary();") != std::string::npos,
              "SEMANTIC: switch boundary explicitly discards loaded audio ownership");
        check(switchBody.find("state.clearPendingWaveforms();") != std::string::npos,
              "SEMANTIC: switch boundary clears pending waveform work");
        check(switchBody.find("state.waveformCache.clear();") != std::string::npos,
              "SEMANTIC: switch boundary clears waveform cache on the same frame");
        check(switchBody.find("state.waveformFailed.clear();") != std::string::npos,
              "SEMANTIC: switch boundary clears waveform failure cache on the same frame");
        check(switchBody.find("CoverModule::ClearAllCoverCaches();") != std::string::npos,
              "SEMANTIC: switch boundary clears cover caches on the same frame");
        check(switchBody.find("state.listTabs.clear();") != std::string::npos,
              "SEMANTIC: switch boundary clears tab-local UI state on the same frame");
        check(switchBody.find("state.currentFileId = -1;") != std::string::npos,
              "SEMANTIC: switch boundary clears current file identity on the same frame");
        check(switchBody.find("state.selectedResultIndex = -1;") != std::string::npos,
              "SEMANTIC: switch boundary clears current selection on the same frame");
        check(switchBody.find("state.pendingDbPath = effectiveDbPathUtf8;") != std::string::npos,
              "SEMANTIC: switch boundary records pending DB path before async open");

        check(occursBefore(switchBody, "state.audioEngine.discardLoadedFileForDatabaseBoundary();", threadSpawn),
              "SEMANTIC: audio discard happens before async DB-open thread starts");
        check(occursBefore(switchBody, "state.clearPendingWaveforms();", threadSpawn),
              "SEMANTIC: waveform pending queue is cleared before async DB-open thread starts");
        check(occursBefore(switchBody, "state.waveformCache.clear();", threadSpawn),
              "SEMANTIC: waveform cache is cleared before async DB-open thread starts");
        check(occursBefore(switchBody, "CoverModule::ClearAllCoverCaches();", threadSpawn),
              "SEMANTIC: cover caches are cleared before async DB-open thread starts");
        check(occursBefore(switchBody, "state.listTabs.clear();", threadSpawn),
              "SEMANTIC: tab state is cleared before async DB-open thread starts");
        check(occursBefore(switchBody, "state.currentFileId = -1;", threadSpawn),
              "SEMANTIC: current file identity is cleared before async DB-open thread starts");
        check(occursBefore(switchBody, "state.selectedResultIndex = -1;", threadSpawn),
              "SEMANTIC: selection is cleared before async DB-open thread starts");
        check(occursBefore(switchBody, "state.dbSwitchInProgress = true;", threadSpawn),
              "SEMANTIC: switch-in-progress gate is armed before async DB-open thread starts");
        check(occursBefore(switchBody, "state.dbSwitchInProgress = true;", "RunBlockingDatabaseSwitchOperation("),
              "SEMANTIC: switch-in-progress gate is armed before the background switch operation starts");
    }

    if (!pumpBody.empty()) {
        check(pumpBody.find("state.dbSwitchNeedsRefresh = true;") != std::string::npos,
              "SEMANTIC: pump path schedules deferred search refresh after DB switch settles");
        check(pumpBody.find("state.dbSwitchJustCompleted = true;") != std::string::npos,
              "SEMANTIC: success path marks dbSwitchJustCompleted for post-switch consumers");

        const size_t failurePos = pumpBody.find("} else {");
        const std::string failureBody =
            (failurePos == std::string::npos) ? std::string() : pumpBody.substr(failurePos);
        check(!failureBody.empty(), "PumpDatabaseSwitchIfPending failure branch can be inspected");
        if (!failureBody.empty()) {
            check(failureBody.find("state.database.open(oldPath, true);") != std::string::npos,
                  "SEMANTIC: failure branch reopens previous DB package");
            check(failureBody.find("ResetDatabaseState(state);") != std::string::npos,
                  "SEMANTIC: failure branch restores a fully consistent runtime state");
            check(failureBody.find("state.dbSwitchNeedsRefresh = true;") != std::string::npos,
                  "SEMANTIC: failure branch schedules refresh after reopening previous DB");
            check(failureBody.find("state.syncFromTab();") != std::string::npos,
                  "SEMANTIC: failure branch resyncs AppState from restored tab state");
            check(occursBefore(failureBody, "state.database.open(oldPath, true);", "ResetDatabaseState(state);"),
                  "SEMANTIC: previous DB reopens before failure recovery reset runs");
        }
    }

    const std::string audioEngineSource = readTextFile(audioEngineCpp);
    check(!audioEngineSource.empty(), "AudioEngine.cpp source is readable");

    const std::string discardBody = extractFunctionBody(
        audioEngineSource,
        "void AudioEngine::discardLoadedFileForDatabaseBoundary()");
    check(!discardBody.empty(), "discardLoadedFileForDatabaseBoundary body can be inspected");

    if (!discardBody.empty()) {
        check(discardBody.find("stop();") != std::string::npos,
              "SEMANTIC: DB-boundary audio discard stops playback immediately");
        check(discardBody.find("resetState(true);") != std::string::npos,
              "SEMANTIC: DB-boundary audio discard resets decoder ownership");
        check(discardBody.find("m_loadAsyncRequestPending = false;") != std::string::npos,
              "SEMANTIC: DB-boundary audio discard cancels queued async load requests");
        check(discardBody.find("m_loadAsyncRequestPath.clear();") != std::string::npos,
              "SEMANTIC: DB-boundary audio discard clears queued async file path");
        check(discardBody.find("m_loadAsyncPending.store(false") != std::string::npos,
              "SEMANTIC: DB-boundary audio discard drops async load pending state");
        check(discardBody.find("m_loadAsyncCurrentToken.fetch_add(1") != std::string::npos,
              "SEMANTIC: DB-boundary audio discard invalidates stale async load completions");
        check(discardBody.find("m_seekToken.fetch_add(1") != std::string::npos,
              "SEMANTIC: DB-boundary audio discard invalidates stale seek completions");
    }

    std::printf("\n=== Summary: %d pass, %d fail ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
