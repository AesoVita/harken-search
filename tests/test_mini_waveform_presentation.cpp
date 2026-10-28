#include <cstdio>

#include "src/MiniWaveformPresentation.h"

int main() {
    using MiniWaveformPresentation::Choose;
    using MiniWaveformPresentation::Mode;
    using MiniWaveformPresentation::Removal;
    using MiniWaveformPresentation::RetainTextureAfterRemoval;

    int failures = 0;
    const auto check = [&failures](bool condition, const char* label) {
        std::printf("%s: %s\n", condition ? "PASS" : "FAIL", label);
        if (!condition) ++failures;
    };

    check(Choose(false, true) == Mode::Texture,
          "loaded waveform stays visible while its CPU data is recycled");
    check(Choose(false, false) == Mode::Placeholder,
          "unloaded waveform without a texture shows the placeholder");
    check(Choose(true, false) == Mode::CpuFallback,
          "ready data without a texture can draw the waveform");
    check(Choose(true, true) == Mode::Texture,
          "ready data uses its texture");
    check(RetainTextureAfterRemoval(Removal::BudgetEviction),
          "memory budget eviction retains the bounded display texture");
    check(!RetainTextureAfterRemoval(Removal::ExplicitInvalidation),
          "file invalidation removes the old display texture");
    return failures == 0 ? 0 : 1;
}
