#pragma once

#include <functional>
#include <vector>

namespace aura::audition {

enum class PluginStepKind {
    Passthrough,
    Reverse,
    Normalize,
    Unknown,
};

struct AuditionPluginChainResult {
    bool ok = false;
    std::vector<float> output;
    std::vector<PluginStepKind> appliedSteps;
};

class AuditionPluginChain {
public:
    using StepHandler = std::function<bool(std::vector<float>&)>;

    void Append(PluginStepKind kind);
    void SetStepHandler(PluginStepKind kind, StepHandler handler);
    AuditionPluginChainResult Run(const std::vector<float>& input) const;

private:
    std::vector<PluginStepKind> steps_;
    StepHandler reverseHandler_;
    StepHandler normalizeHandler_;
    StepHandler passthroughHandler_;
};

}
