#pragma once

namespace MiniWaveformPresentation {

enum class Mode { Texture, CpuFallback, Placeholder };
enum class Removal { BudgetEviction, ExplicitInvalidation };

inline bool RetainTextureAfterRemoval(Removal removal) {
    return removal == Removal::BudgetEviction;
}

inline Mode Choose(bool dataReady, bool textureReady) {
    if (textureReady) return Mode::Texture;
    return dataReady ? Mode::CpuFallback : Mode::Placeholder;
}

} // namespace MiniWaveformPresentation
