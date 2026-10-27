#include "WaveformViewResetStateAdapter.h"

#include "AppState.h"

namespace aura::waveform {

namespace {

void ResetTrimRefineFlags(UILocalState& ui) {
    ui.waveformTrimRefineActive = false;
    ui.waveformTrimAutoZoomLatched = false;
    ui.trimRefineZoomBeforeEnter = 1.0f;
    ui.trimRefineCenterBeforeEnter = 0.5;
}

}  // namespace

void ResetRegionEditor(UILocalState& ui, bool keepRegionFade) {
    ResetTrimRefineFlags(ui);
    ui.isFadeDragging = false;
    ui.fadeDragIsFadeIn = true;
    ui.edgeDragMouseDownX = 0.0f;
    if (!keepRegionFade) {
        ui.regionFadeInNorm = 0.0f;
        ui.regionFadeOutNorm = 0.0f;
    }
    ui.isEdgeDragging = false;
    ui.edgeDragIsInPoint = true;
    ui.isDraggingRegion = false;
    ui.regionDragPending = false;
    ui.regionDragClearPending = false;
    ui.regionDragStartMouse = ImVec2(0.0f, 0.0f);
    ui.dragStartPos = 0.0f;
    ui.isDraggingToExport = false;
    ui.exportDragStart = ImVec2(0.0f, 0.0f);
    ui.exportStartPos = 0.0f;
    ui.exportPendingSeek = false;
    ui.exportHoldPending = false;
    ui.exportHoldStartTime = 0.0;
    ui.exportHoldMouseStart = ImVec2(0.0f, 0.0f);
}

void ExitTrimRefine(UILocalState& ui) {
    ResetTrimRefineFlags(ui);
}

void ResetZoom(AppState& state) {
    ResetRegionEditor(state.uiState, false);
    state.waveformZoom = 1.0f;
    state.waveformViewCenter = 0.5f;
    state.waveformDisplayZoom = state.waveformZoom;
    state.waveformDisplayViewCenter = state.waveformViewCenter;
    state.waveformZoomAnchorSet = false;
    state.waveformZoomAnchor = 0.0f;
}

}  // namespace aura::waveform

void ResetWaveformZoom(AppState& state) {
    aura::waveform::ResetZoom(state);
}
