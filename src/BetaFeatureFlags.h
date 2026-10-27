#pragma once

// @stage7-contract
// API contract: beta flags are read-only capability/configuration values for a
// process session. They do not grant production APIs or alter persisted data;
// callers must use the existing edition/build gating semantics.

namespace HarkenSearch::BetaFeatureFlags {

// First internal beta: keep shipped systems compiled and expose surfaces that
// are ready for development validation. ProductEdition/manifest gates still
// decide whether an exposed surface is enabled for the current runtime.
inline constexpr bool kExposeMusicModeSwitch = false;
inline constexpr bool kExposeWwiseTransfer = true;
inline constexpr bool kEnableReplacementCoverWallIntegratedModeByDefault = true;

// First internal beta startup gate: show the login surface, then continue with
// the WebView2 preheat loading animation. The small PNG beta splash remains
// compiled but is not used in the startup path.
inline constexpr bool kShowStartupLogin = true;
inline constexpr bool kUseBetaSplashLoading = false;

} // namespace HarkenSearch::BetaFeatureFlags
