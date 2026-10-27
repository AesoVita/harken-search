#pragma once

struct AppState;
struct AudioRecord;

namespace aura::ui {

constexpr unsigned kRelinkShutdownJoinTimeoutMs = 10000;

// @stage7-contract
// API contract: all operations borrow AppState on the UI thread. Candidate
// workers preserve the existing publication order; render calls return true
// only when the composition root must refresh search results.
void EvaluateRelinkForRecord(AppState& state, const AudioRecord& record);
void PumpRelinkWorkflow(AppState& state);
void ShutdownRelinkWorkflow(AppState& state);
bool RenderRelinkBanner(AppState& state);
bool RenderRelinkTool(AppState& state);

} // namespace aura::ui
