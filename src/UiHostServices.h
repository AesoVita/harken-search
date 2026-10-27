#pragma once

// @stage7-contract
// API contract: returns the borrowed native owner-window handle for transient
// UI services. The pointer may be null before startup or after shutdown; callers
// must not retain it, destroy it, or use it outside the UI thread.
namespace HarkenSearch::UiHost {

void* MainWindowOwner() noexcept;

} // namespace HarkenSearch::UiHost
