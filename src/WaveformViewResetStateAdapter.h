#pragma once

struct AppState;
struct UILocalState;

namespace aura::waveform {

void ExitTrimRefine(UILocalState& ui);
void ResetRegionEditor(UILocalState& ui, bool keepRegionFade);
void ResetZoom(AppState& state);

}  // namespace aura::waveform
