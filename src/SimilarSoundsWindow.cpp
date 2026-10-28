#include "SimilarSoundsWindow.h"

#include "AppState.h"
#include "HarkenSearchWidgets.h"
#include "DockNativeCaptionBridge.h"
#include "imgui_internal.h"
#include "Localization.h"
#include "SimilarityQueryService.h"
#include "Theme.h"
#include "UIRenderer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

namespace aura::ui {

static std::optional<HarkenSearch::SimilarityReference> ResolveCurrentSimilarityReference(AppState& state) {
    auto fromRecord = [](const AudioRecord& record)
        -> std::optional<HarkenSearch::SimilarityReference> {
        if (record.id <= 0 || record.filePath.empty()) return std::nullopt;
        HarkenSearch::SimilarityReference reference;
        reference.fileId = record.id;
        reference.filePath = record.filePath;
        reference.displayName = !record.displayName.empty()
            ? record.displayName
            : (!record.fileName.empty() ? record.fileName : record.filePath);
        return reference;
    };

    if (state.selectedResultIndex >= 0 &&
        state.selectedResultIndex < static_cast<int>(state.searchResults.size())) {
        if (auto reference = fromRecord(state.searchResults[state.selectedResultIndex])) {
            return reference;
        }
    }
    if (state.currentFileId <= 0) return std::nullopt;
    for (const AudioRecord& record : state.searchResults) {
        if (record.id == state.currentFileId) return fromRecord(record);
    }
    return fromRecord(state.database.getFileById(state.currentFileId));
}

static bool DrawSimilarSoundsActionRow(
    AppState& state,
    HarkenSearch::SimilarityWorkflow& workflow,
    const HarkenSearch::SimilarityWorkflowSnapshot& snapshot) {
    const bool searching = snapshot.status == HarkenSearch::SimilarityWorkflowStatus::Searching;
    const bool indexing = state.batchFeatureExtracting.load();
    const bool hasReference = snapshot.status != HarkenSearch::SimilarityWorkflowStatus::NoSelection;
    const char* actionLabel = indexing ? L(Similar_Indexing)
        : (searching ? L(Similar_Searching) : L(Similar_Find));
    const ThemeColorScheme& theme = ThemeColors::current();
    const bool layeredNavy = state.settings.theme == AppTheme::LapisBlue &&
        !state.settings.useCustomColors && !ThemeColors::customized();
    bool searchRequested = false;

    if (layeredNavy) ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.dialogBG);
    if (ImGui::BeginChild("##SimilarActionRow", ImVec2(0.0f, 62.0f), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float rowY = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(rowY + (62.0f - ImGui::GetFrameHeight()) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.resultsSecondaryText);
        ImGui::TextUnformatted(L(Similar_CurrentAudio));
        ImGui::PopStyleColor();
        ImGui::SameLine();

        constexpr float comboWidth = 132.0f;
        const float buttonWidth = ImGui::CalcTextSize(actionLabel).x + 20.0f;
        const float fileWidth = std::max(80.0f,
            ImGui::GetContentRegionAvail().x - comboWidth - buttonWidth -
            style.ItemSpacing.x * 2.0f);
        const ImVec2 textMin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##SimilarCurrentFile", ImVec2(fileWidth, ImGui::GetFrameHeight()));
        const ImVec2 textMax(textMin.x + fileWidth, textMin.y + ImGui::GetFrameHeight());
        const char* currentName = L(Similar_NoSelection);
        if (state.similarSoundsSelectedRow >= 0 &&
            state.similarSoundsSelectedRow < static_cast<int>(snapshot.rows.size())) {
            currentName = snapshot.rows[state.similarSoundsSelectedRow].fileName.c_str();
        } else if (!snapshot.reference.displayName.empty()) {
            currentName = snapshot.reference.displayName.c_str();
        }
        ImVec2 currentNameSize = ImGui::CalcTextSize(currentName);
        ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), textMin, textMax,
            textMax.x, currentName, nullptr, &currentNameSize);

        ImGui::SameLine();
        const char* presetLabels[] = {
            L(Similar_PresetBalanced), L(Similar_PresetTimbre),
            L(Similar_PresetRhythm), L(Similar_PresetPitch),
            L(Similar_PresetSpectral)
        };
        int preset = static_cast<int>(state.similarSoundsPreset);
        ImGui::SetNextItemWidth(comboWidth);
        if (aura::ui::Combo("##SimilarPreset", &preset, presetLabels,
                IM_ARRAYSIZE(presetLabels))) {
            state.similarSoundsPreset = static_cast<SimilarityPreset>(preset);
        }
        ImGui::SameLine();
        aura::ui::ButtonOptions options;
        options.variant = aura::ui::ButtonVariant::PrimarySearch;
        options.disabled = searching || indexing || !hasReference;
        if (searching || indexing) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.resultsSecondaryText);
            ImGui::TextUnformatted(actionLabel);
            ImGui::PopStyleColor();
        } else if (aura::ui::Button(actionLabel, options)) {
            if (state.similarSoundsSelectedRow >= 0 &&
                state.similarSoundsSelectedRow < static_cast<int>(snapshot.rows.size())) {
                const HarkenSearch::SimilarityDisplayRow& selected =
                    snapshot.rows[state.similarSoundsSelectedRow];
                state.similarSoundsLocalReference = HarkenSearch::SimilarityReference{
                    selected.fileId, selected.filePath, selected.fileName};
                workflow.SetReference(state.similarSoundsLocalReference);
            }
            state.similarSoundsSelectedRow = -1;
            searchRequested = workflow.RequestSearch(state.similarSoundsPreset, 50);
        }
    }
    ImGui::EndChild();
    if (layeredNavy) ImGui::PopStyleColor();
    return searchRequested;
}

static const char* SimilarSoundsBodyMessage(
    const HarkenSearch::SimilarityWorkflowSnapshot& snapshot,
    bool indexing) {
    if (indexing) return L(Similar_Indexing);
    if (snapshot.status == HarkenSearch::SimilarityWorkflowStatus::NoSelection) {
        return L(Similar_NoSelection);
    }
    if (snapshot.status == HarkenSearch::SimilarityWorkflowStatus::Error) {
        if (snapshot.error == HarkenSearch::SimilarityWorkflowError::DatabaseUnavailable) {
            return L(Similar_DatabaseError);
        }
        if (snapshot.error == HarkenSearch::SimilarityWorkflowError::ReferenceUnreadable) {
            return L(Similar_ReadError);
        }
        return L(Similar_NoResults);
    }
    if (snapshot.status != HarkenSearch::SimilarityWorkflowStatus::Searching &&
        snapshot.rows.empty()) {
        return L(Similar_NoResults);
    }
    return nullptr;
}

static void ToggleSimilarSoundsAudition(
    AppState& state,
    const HarkenSearch::SimilarityDisplayRow& row) {
    const bool loadedThis = state.audioEngine.isLoaded() &&
        state.audioEngine.getLoadedFilePath() == row.filePath;
    if (!loadedThis) {
        state.audioEngine.loadFileAsync(row.filePath, true);
        ScheduleDeferredWaveformDetail(state, row.filePath, AppState::WAVEFORM_DEFAULT_DETAIL_PEAKS);
    } else if (state.audioEngine.isPlaying()) {
        state.audioEngine.pause();
    } else {
        if (state.audioEngine.getPlaybackPosition() >= 0.99f) {
            state.audioEngine.seekToPosition(0.0f);
        }
        state.audioEngine.play();
    }
    MarkFilePlayed(state, row.filePath, row.fileId, true);
}

static void DrawSimilarSoundsResultsTable(
    AppState& state,
    const HarkenSearch::SimilarityWorkflowSnapshot& snapshot,
    bool windowFocused) {
    const ThemeColorScheme& theme = ThemeColors::current();
    const bool layeredNavy = state.settings.theme == AppTheme::LapisBlue &&
        !state.settings.useCustomColors && !ThemeColors::customized();
    const ImVec4 tableBody = layeredNavy ? theme.dialogPanelBG : theme.resultsBG;
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, layeredNavy ? theme.toolbarBG : theme.resultsHeaderBG);
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, tableBody);
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,
        ImLerp(tableBody, layeredNavy ? theme.resultsBG : theme.resultsHeaderBG, 0.32f));
    if (layeredNavy) ImGui::PushStyleColor(ImGuiCol_ChildBg, tableBody);
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    const float similarRowHeight = std::max(
        24.0f, static_cast<float>(state.settings.rowHeight));
    if (ImGui::BeginTable("##SimilarResultsTable", 5, flags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn(L(Similar_ColumnFileName),
            ImGuiTableColumnFlags_WidthStretch, 2.5f);
        ImGui::TableSetupColumn(L(Similar_ColumnWaveform),
            ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn(L(Similar_ColumnDuration),
            ImGuiTableColumnFlags_WidthFixed, 84.0f);
        ImGui::TableSetupColumn(L(Similar_ColumnFormat),
            ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn(L(Similar_ColumnScore),
            ImGuiTableColumnFlags_WidthFixed, 88.0f);
        const LStr headers[] = {
            LStr::Similar_ColumnFileName, LStr::Similar_ColumnWaveform,
            LStr::Similar_ColumnDuration, LStr::Similar_ColumnFormat,
            LStr::Similar_ColumnScore
        };
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (int column = 0; column < IM_ARRAYSIZE(headers); ++column) {
            ImGui::TableSetColumnIndex(column);
            ImGui::TableHeader(GetLocalizedString(headers[column]));
            if (column == 4 && ImGui::IsItemHovered()) {
                aura::ui::Tooltip(L(Similar_ScoreTooltip));
            }
        }

        if (const char* message = SimilarSoundsBodyMessage(
                snapshot, state.batchFeatureExtracting.load())) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.resultsSecondaryText);
            ImGui::TextUnformatted(message);
            ImGui::PopStyleColor();
        }
        for (int index = 0; index < static_cast<int>(snapshot.rows.size()); ++index) {
            const HarkenSearch::SimilarityDisplayRow& row = snapshot.rows[index];
            ImGui::PushID(row.filePath.c_str());
            ImGui::TableNextRow(ImGuiTableRowFlags_None, similarRowHeight);
            ImGui::TableSetColumnIndex(0);
            const int previousSelectedRow = state.similarSoundsSelectedRow;
            if (ImGui::Selectable(row.fileName.c_str(),
                    state.similarSoundsSelectedRow == index,
                    ImGuiSelectableFlags_SpanAllColumns |
                        ImGuiSelectableFlags_AllowDoubleClick |
                        ImGuiSelectableFlags_AllowOverlap,
                    ImVec2(0.0f, similarRowHeight))) {
                const bool waveColumnHovered =
                    (ImGui::TableGetColumnFlags(1) & ImGuiTableColumnFlags_IsHovered) != 0;
                const bool wasPlaying = state.audioEngine.isPlaying();
                const bool selectionChanged = previousSelectedRow != index;
                state.similarSoundsSelectedRow = index;
                if (!waveColumnHovered) {
                    state.uiState.pendingMiniWaveSeekPos = -1.0f;
                    const bool loadedThis = state.audioEngine.isLoaded() &&
                        state.audioEngine.getLoadedFilePath() == row.filePath;
                    const PlaybackAction action = GetPlaybackAction(state.settings,
                        wasPlaying, selectionChanged,
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left));
                    if (action == PlaybackAction::Pause) {
                        state.audioEngine.pause();
                    } else if (action == PlaybackAction::Play) {
                        if (!loadedThis || state.audioEngine.isLoadFilePending()) {
                            state.audioEngine.loadFileAsync(row.filePath, true);
                            ScheduleDeferredWaveformDetail(
                                state, row.filePath, AppState::WAVEFORM_DEFAULT_DETAIL_PEAKS);
                        } else {
                            if (state.audioEngine.getPlaybackPosition() >= 0.99f) {
                                state.audioEngine.seekToPosition(0.0f);
                            }
                            state.audioEngine.play();
                        }
                        MarkFilePlayed(state, row.filePath, row.fileId, true);
                    }
                }
            }
            ImGui::TableSetColumnIndex(1);
            const bool loadedThis = state.audioEngine.isLoaded() &&
                state.audioEngine.getLoadedFilePath() == row.filePath;
            const float playPos = loadedThis
                ? GetSmoothedPlaybackPosition(state) : -1.0f;
            const float waveWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            const float waveHeight = similarRowHeight;
            const ImVec2 waveCellMin = ImGui::GetCursorScreenPos();
            const auto cacheIt = state.waveformCache.find(row.filePath);
            if (cacheIt == state.waveformCache.end()) {
                state.visibleFilePaths.insert(row.filePath);
                state.addPendingWaveform(row.filePath, true);
                ImGui::Dummy(ImVec2(waveWidth, waveHeight));
            } else {
                const WaveformData& cacheEntry = cacheIt->second;
                DrawMiniWaveform(cacheEntry, playPos, waveWidth, waveHeight);
            }
            ImGui::SetCursorScreenPos(waveCellMin);
            ImGui::InvisibleButton("##SimilarWaveHit", ImVec2(waveWidth, waveHeight));
            const bool waveClicked = ImGui::IsItemClicked();
            if (waveClicked) state.similarSoundsSelectedRow = index;
            if (ImGui::IsItemActive() || ImGui::IsItemClicked()) {
                const float seekPos = std::clamp(
                    (ImGui::GetMousePos().x - waveCellMin.x) / waveWidth, 0.0f, 1.0f);
                if (!loadedThis) {
                    if (ImGui::IsItemClicked()) {
                        state.uiState.pendingMiniWaveSeekPos = -1.0f;
                        state.uiState.miniWaveSeekApplied = true;
                        state.uiState.miniWaveSeekTargetPath = row.filePath;
                        state.uiState.lastMiniWaveSeekPos = seekPos;
                        state.audioEngine.loadFileAsync(row.filePath, true);
                        ScheduleDeferredWaveformDetail(
                            state, row.filePath, AppState::WAVEFORM_DEFAULT_DETAIL_PEAKS);
                        MarkFilePlayed(state, row.filePath, row.fileId, true);
                        state.audioEngine.seekToPosition(seekPos);
                    }
                } else if (ImGui::IsItemClicked()) {
                    state.uiState.pendingMiniWaveSeekPos = -1.0f;
                    state.uiState.miniWaveSeekApplied = true;
                    state.uiState.miniWaveSeekTargetPath.clear();
                    state.uiState.lastMiniWaveSeekPos = seekPos;
                    state.audioEngine.seekToPosition(seekPos);
                    if (!state.audioEngine.isPlaying()) state.audioEngine.play();
                } else if (std::abs(seekPos - state.uiState.lastMiniWaveSeekPos) > 0.002f) {
                    state.uiState.lastMiniWaveSeekPos = seekPos;
                    state.audioEngine.seekToPosition(seekPos);
                }
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(FormatTime(row.durationSeconds).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(row.format.empty() ? "-" : row.format.c_str());
            ImGui::TableSetColumnIndex(4);
            const int score = static_cast<int>(
                std::clamp(row.similarity, 0.0f, 1.0f) * 100.0f + 0.5f);
            ImGui::Text("%d%%", score);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (windowFocused && !ImGui::GetIO().WantTextInput &&
        state.similarSoundsSelectedRow >= 0 &&
        state.similarSoundsSelectedRow < static_cast<int>(snapshot.rows.size()) &&
        ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        const HarkenSearch::SimilarityDisplayRow& row =
            snapshot.rows[state.similarSoundsSelectedRow];
        // ProcessKeybindings runs before this standalone window. Let its
        // global transport toggle an already-loaded row only once.
        const bool loadedThis = state.audioEngine.isLoaded() &&
            state.audioEngine.getLoadedFilePath() == row.filePath;
        if (!loadedThis) {
            ToggleSimilarSoundsAudition(state, row);
        }
    }
    if (layeredNavy) ImGui::PopStyleColor();
    ImGui::PopStyleColor(3);
}

void OpenSimilarSoundsWindow(AppState& state) {
    if (!state.panelSimilarSounds.visible) {
        state.batchFeatureCompletionNotice.store(false);
    }
    if (!state.similarSoundsWorkflow) {
        state.similarSoundsWorkflow =
            std::make_unique<HarkenSearch::SimilarityWorkflow>(
                HarkenSearch::MakeSimilarityQueryHandler(state.similaritySearch));
    }
    state.panelSimilarSounds.visible = true;
    state.panelSimilarSounds.floating = true;
    state.panelSimilarSounds.wasDragging = false;
    if (state.panelSimilarSounds.floatW < 220.0f) state.panelSimilarSounds.floatW = 720.0f;
    if (state.panelSimilarSounds.floatH < 180.0f) state.panelSimilarSounds.floatH = 360.0f;
    if (state.panelSimilarSounds.floatX < 80.0f) state.panelSimilarSounds.floatX = 420.0f;
    if (state.panelSimilarSounds.floatY < 40.0f) state.panelSimilarSounds.floatY = 120.0f;
    state.similarSoundsWindow.restorePos = ImVec2(state.panelSimilarSounds.floatX, state.panelSimilarSounds.floatY);
    state.similarSoundsWindow.restoreSize = ImVec2(state.panelSimilarSounds.floatW, state.panelSimilarSounds.floatH);
    aura::ui::OpenStandaloneWindow(&state.similarSoundsWindow);
    state.similarSoundsWindow.requestMove = true;
}

void InvalidateSimilarSoundsWindow(AppState& state) {
    if (!state.similarSoundsWorkflow) return;
    state.similarSoundsWorkflow->Invalidate();
    state.similarSoundsSelectedRow = -1;
    state.similarSoundsLocalReference.reset();
}

static void DrawSimilarSoundsBody(AppState& state, bool windowFocused) {
    HarkenSearch::SimilarityWorkflow& workflow = *state.similarSoundsWorkflow;
    HarkenSearch::SimilarityWorkflowSnapshot snapshot = workflow.Snapshot();
    if (DrawSimilarSoundsActionRow(state, workflow, snapshot)) {
        snapshot = workflow.Snapshot();
    }
    const bool layeredNavy = state.settings.theme == AppTheme::LapisBlue &&
        !state.settings.useCustomColors && !ThemeColors::customized();
    if (layeredNavy) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::current().dialogPanelBG);
        if (ImGui::BeginChild("##SimilarIndexRegion", ImVec2(0.0f, 0.0f),
                ImGuiChildFlags_AutoResizeY,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            DrawSimilarityIndexPanel(state);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else {
        DrawSimilarityIndexPanel(state);
    }
    // Starting analysis invalidates rows, even if the task finishes this frame.
    snapshot = workflow.Snapshot();
    if (layeredNavy) ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::current().toolbarBG);
    if (ImGui::BeginChild("##SimilarResultsHeading", ImVec2(0.0f, 36.0f), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const float rowY = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(rowY + (36.0f - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::TextUnformatted(L(Similar_Results));
        ImGui::SameLine();
        ImGui::PushStyleColor(
            ImGuiCol_Text, ThemeColors::current().resultsSecondaryText);
        ImGui::Text("%d", static_cast<int>(snapshot.rows.size()));
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    if (layeredNavy) ImGui::PopStyleColor();
    DrawSimilarSoundsResultsTable(state, snapshot, windowFocused);
}

static void PollSimilarSoundsWorkflow(AppState& state) {
    auto mainReference = ResolveCurrentSimilarityReference(state);
    const int64_t referenceId = mainReference ? mainReference->fileId : -1;
    if (referenceId != state.similarSoundsObservedSelectionId) {
        state.similarSoundsObservedSelectionId = referenceId;
        state.similarSoundsSelectedRow = -1;
        state.similarSoundsLocalReference.reset();
    }
    auto reference = state.similarSoundsLocalReference ?
        state.similarSoundsLocalReference : std::move(mainReference);
    HarkenSearch::SimilarityWorkflow& workflow = *state.similarSoundsWorkflow;
    workflow.SetDatabasePath(state.database.getMainDbPath());
    workflow.SetReference(std::move(reference));
    const bool batchExtracting = state.batchFeatureExtracting.load();
    if (batchExtracting && !state.similarSoundsObservedBatchExtracting) {
        workflow.Invalidate();
        state.similarSoundsSelectedRow = -1;
    }
    state.similarSoundsObservedBatchExtracting = batchExtracting;
    workflow.Poll();
}

void RenderSimilarSoundsContent(AppState& state) {
    if (!state.similarSoundsWorkflow) {
        state.similarSoundsWorkflow =
            std::make_unique<HarkenSearch::SimilarityWorkflow>(
                HarkenSearch::MakeSimilarityQueryHandler(state.similaritySearch));
    }
    PollSimilarSoundsWorkflow(state);
    DrawSimilarSoundsBody(state, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
}

void RenderSimilarSoundsWindow(AppState& state) {
    if (!state.similarSoundsWorkflow) return;
    if (!state.panelSimilarSounds.floating) return;
    PollSimilarSoundsWorkflow(state);
    HarkenSearch::SimilarityWorkflow& workflow = *state.similarSoundsWorkflow;

    const bool windowWasOpen = state.similarSoundsWindow.open;
    if (!aura::ui::BeginStandaloneWindow(
            L(Similar_Title), "##SimilarSoundsWindow", &state.similarSoundsWindow)) {
        if (windowWasOpen && !state.similarSoundsWindow.open) {
            workflow.Invalidate();
            state.similarSoundsSelectedRow = -1;
            state.similarSoundsLocalReference.reset();
            state.panelSimilarSounds.visible = false;
        }
        return;
    }
    const bool windowFocused =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    // Host RenderUI calls driveFloatingDockDrag("similar-sounds") for native caption
    // and inner-strip docking; this window only binds the extra viewport HWND.
    if (ImGuiViewport* vp = ImGui::GetWindowViewport()) {
        aura::ui::dock::BindNativeCaptionModule(vp->PlatformHandle, "similar-sounds");
        const bool nativeHeld = aura::ui::dock::NativeCaptionDragging() &&
            aura::ui::dock::NativeCaptionModuleId() == "similar-sounds";
        if (aura::ui::dock::ShouldRequestStandaloneMoveFromNativeDrag(
                nativeHeld, state.panelSimilarSounds.floating)) {
            const ImVec2 pos = ImGui::GetWindowPos();
            state.similarSoundsWindow.restorePos = pos;
            state.similarSoundsWindow.requestMove = true;
        }
    }
    const bool IsTitleBarHovered = ImGui::GetCurrentWindowRead() &&
        ImGui::GetCurrentWindowRead()->TitleBarRect().Contains(ImGui::GetMousePos());
    if (IsTitleBarHovered && ImGui::IsMouseDragging(0)) {
        state.panelSimilarSounds.wasDragging = true;
    }
    DrawSimilarSoundsBody(state, windowFocused);
    aura::ui::EndStandaloneWindow();
}

} // namespace aura::ui
