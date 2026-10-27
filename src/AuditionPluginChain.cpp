#include "AuditionPluginChain.h"

#include <algorithm>
#include <cmath>

namespace aura::audition {
namespace {

bool DefaultReverse(std::vector<float>& samples) {
    std::reverse(samples.begin(), samples.end());
    return true;
}

bool DefaultNormalize(std::vector<float>& samples) {
    float peak = 0.0f;
    for (float sample : samples) {
        peak = (std::max)(peak, std::fabs(sample));
    }
    if (peak > 0.0f) {
        const float scale = 1.0f / peak;
        for (float& sample : samples) {
            sample *= scale;
        }
    }
    return true;
}

bool DefaultPassthrough(std::vector<float>&) {
    return true;
}

}

void AuditionPluginChain::Append(PluginStepKind kind) {
    steps_.push_back(kind);
}

void AuditionPluginChain::SetStepHandler(PluginStepKind kind, StepHandler handler) {
    switch (kind) {
        case PluginStepKind::Reverse:
            reverseHandler_ = std::move(handler);
            break;
        case PluginStepKind::Normalize:
            normalizeHandler_ = std::move(handler);
            break;
        case PluginStepKind::Passthrough:
            passthroughHandler_ = std::move(handler);
            break;
        case PluginStepKind::Unknown:
            break;
    }
}

AuditionPluginChainResult AuditionPluginChain::Run(const std::vector<float>& input) const {
    AuditionPluginChainResult result;
    result.output = input;

    for (PluginStepKind kind : steps_) {
        if (kind == PluginStepKind::Unknown) {
            result.ok = false;
            result.output = input;
            result.appliedSteps.clear();
            return result;
        }
    }

    std::vector<float> working = input;
    std::vector<PluginStepKind> applied;

    for (PluginStepKind kind : steps_) {
        StepHandler handler;
        switch (kind) {
            case PluginStepKind::Reverse:
                handler = reverseHandler_ ? reverseHandler_ : DefaultReverse;
                break;
            case PluginStepKind::Normalize:
                handler = normalizeHandler_ ? normalizeHandler_ : DefaultNormalize;
                break;
            case PluginStepKind::Passthrough:
                handler = passthroughHandler_ ? passthroughHandler_ : DefaultPassthrough;
                break;
            case PluginStepKind::Unknown:
                handler = {};
                break;
        }

        if (!handler || !handler(working)) {
            result.ok = false;
            result.output = input;
            result.appliedSteps.clear();
            return result;
        }
        applied.push_back(kind);
    }

    result.ok = true;
    result.output = std::move(working);
    result.appliedSteps = std::move(applied);
    return result;
}

}
