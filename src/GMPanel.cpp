#include "GMPanel.h"

#if AURA_BUILD_DIAGNOSTICS

#include "AppState.h"
#include "AudioEngine.h"
#include "DiagnosticsLogger.h"
#include "MemoryBudget.h"
#include "CoverModule.h"
#include "Theme.h"
#include "imgui.h"
#include <cstdio>

void GMPanel_Render(AppState& state) {
    if (!state.settings.diagnosticMode || !state.settings.showGMPanel)
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("GM Debug", &state.settings.showGMPanel,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    {
        const auto& tc  = ImGui::GetStyle().Colors;
        const ImVec4 accent = ThemeColors::infoPanelAccent(ThemeColors::current());
        const ImVec4 base   = tc[ImGuiCol_ChildBg];
        auto mk = [](ImVec4 a, ImVec4 b, float t, float alpha) -> ImVec4 {
            return ImVec4(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, alpha);
        };
        ImGui::PushStyleColor(ImGuiCol_Tab,                mk(base, accent, 0.16f, 0.88f));
        ImGui::PushStyleColor(ImGuiCol_TabHovered,         mk(base, accent, 0.28f, 0.94f));
        ImGui::PushStyleColor(ImGuiCol_TabActive,          mk(base, accent, 0.36f, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_TabUnfocused,       mk(base, tc[ImGuiCol_TabUnfocused],       0.46f, 0.84f));
        ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, mk(base, tc[ImGuiCol_TabUnfocusedActive], 0.50f, 0.88f));
    }
    if (ImGui::BeginTabBar("##GMTabs")) {
// Tab 1: Waveform
        if (ImGui::BeginTabItem("Waveform")) {
            const auto& fi = state.audioEngine.getFileInfo();
            auto wf = state.audioEngine.getWaveformData();
            bool ready  = wf && wf->isReady;
            bool match  = wf && (wf->filePath.empty() || wf->filePath == fi.filePath);
            bool valid  = ready && match && wf && !wf->peaks.empty();
            int  peaks  = wf ? (int)wf->peaks.size() : 0;
            int  nCh    = wf ? wf->numChannels : 0;

            ImGui::Text("valid=%d  ready=%d  match=%d", valid?1:0, ready?1:0, match?1:0);
            ImGui::Text("peaks=%d  channels=%d", peaks, nCh);
            ImGui::Separator();
            ImGui::Text("loaded: %s", fi.filePath.c_str());
            ImGui::Text("wf src: %s", wf ? wf->filePath.c_str() : "<none>");
            ImGui::Separator();
            ImGui::Text("zoom: %.2f  center: %.6f", state.waveformZoom, state.waveformViewCenter);
            ImGui::Text("amplitude: %.2f  style: %d", state.waveformAmplitudeScale, state.settings.waveformStyle);
            ImGui::Text("detail pending: %d", state.audioEngine.isWaveformDetailPending() ? 1 : 0);
            ImGui::EndTabItem();
        }

// Tab 2: Cover
        if (ImGui::BeginTabItem("Cover")) {
            auto& cw = state.coverWall;
            ImGui::Text("source: %d  generation: %llu", (int)cw.source, (unsigned long long)cw.generation);
            ImGui::Text("items: %zu  filtered: %zu", cw.items.size(), cw.filteredItems.size());
            ImGui::Text("loaded: %d  loading: %d", cw.loaded?1:0, cw.loading?1:0);
            ImGui::Text("pending facet: %zu  fallback: %zu",
                        cw.pendingFacetItems.size(), cw.fallbackItems.size());
            ImGui::Separator();
            if (state.memoryBudget) {
                auto stats = state.memoryBudget->getStatistics();
                ImGui::Text("cover cache: %.1f MB (%zu entries)",
                            (double)stats.coverUsage / (1024.0 * 1024.0), stats.coverCount);
                ImGui::Text("waveform cache: %.1f MB (%zu entries)",
                            (double)stats.waveformUsage / (1024.0 * 1024.0), stats.waveformCount);
            }
            ImGui::EndTabItem();
        }

// Tab 3: Database
        if (ImGui::BeginTabItem("Database")) {
            ImGui::Text("open: %d", state.database.isOpen() ? 1 : 0);
            ImGui::Text("files: %lld", (long long)state.totalFileCount);
            ImGui::Text("results: %zu", state.searchResults.size());
            ImGui::Text("path: %s", state.currentDbPathUtf8.c_str());
            ImGui::Separator();
            ImGui::Text("scanning: %d  phase: %d",
                        state.isScanning.load() ? 1 : 0, (int)state.scanPhase.load());
            ImGui::Text("scan progress: %d / %d",
                        state.scanProgress.load(), state.scanTotal.load());
            ImGui::Text("results gen: %llu  facets gen: %llu",
                        (unsigned long long)state.searchResultsGen_,
                        (unsigned long long)state.facetsGen_);
            ImGui::EndTabItem();
        }

// Tab 4: Audio
        if (ImGui::BeginTabItem("Audio")) {
            const auto& fi = state.audioEngine.getFileInfo();
            ImGui::Text("loaded: %d  playing: %d",
                        state.audioEngine.isLoaded() ? 1 : 0,
                        state.audioEngine.isPlaying() ? 1 : 0);
            ImGui::Text("file: %s", fi.fileName.c_str());
            ImGui::Separator();
            ImGui::Text("pos: %.4f  time: %.2fs / %.2fs",
                        state.audioEngine.getPlaybackPosition(),
                        state.audioEngine.getCurrentTimeSeconds(),
                        fi.durationSeconds);
            ImGui::Text("SR: %u  ch: %u  fmt: %s",
                        fi.sampleRate, fi.channels, fi.format.c_str());
            ImGui::Separator();
            ImGui::Text("vol: %.2f  pitch: %.1f  pan: %.2f",
                        state.audioEngine.getVolume(),
                        state.audioEngine.getPitch(),
                        state.audioEngine.getPan());
            ImGui::Text("center: %.2f  reverse: %d  mono: %d  solo: %d",
                        state.audioEngine.getCenterSense(),
                        state.audioEngine.isReversed() ? 1 : 0,
                        state.audioEngine.isMono() ? 1 : 0,
                        state.audioEngine.getChannelSolo());
            float inPt = state.audioEngine.getInPoint();
            float outPt = state.audioEngine.getOutPoint();
            ImGui::Text("in: %.4f  out: %.4f  region: %s",
                        inPt, outPt,
                        (inPt > 0.0f || outPt < 1.0f) ? "YES" : "no");
            ImGui::Separator();
            std::string lastDiag = state.audioEngine.peekLastLoadDiag();
            if (!lastDiag.empty()) {
                ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "last diag: %s", lastDiag.c_str());
            }
            ImGui::Text("engine SR: %u", state.audioEngine.getEngineSampleRate());
            ImGui::EndTabItem();
        }

// Tab 5: Performance
        if (ImGui::BeginTabItem("Performance")) {
            auto& diag = DiagnosticsLogger::instance();
            ImGui::Text("FPS: %.1f", diag.currentFps());
            ImGui::Text("Memory: %.1f MB", diag.currentMemoryMb());
            ImGui::Text("Dropped logs: %llu", (unsigned long long)diag.droppedCount());
            ImGui::Separator();
            if (state.memoryBudget) {
                auto stats = state.memoryBudget->getStatistics();
                ImGui::Text("Total: %.1f MB", (double)stats.totalUsage / (1024.0 * 1024.0));
                ImGui::Text("  Waveform: %.1f MB (%zu)", (double)stats.waveformUsage / (1024.0 * 1024.0), stats.waveformCount);
                ImGui::Text("  Cover:    %.1f MB (%zu)", (double)stats.coverUsage / (1024.0 * 1024.0), stats.coverCount);
                ImGui::Text("  Metadata: %.1f MB (%zu)", (double)stats.metadataUsage / (1024.0 * 1024.0), stats.metadataCount);
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor(5);

    ImGui::End();
}

#endif // AURA_BUILD_DIAGNOSTICS
