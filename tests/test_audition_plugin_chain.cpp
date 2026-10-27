#include "src/AuditionPluginChain.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::printf("PASS: %s\n", message);
    } else {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

bool SameSamples(const std::vector<float>& left, const std::vector<float>& right) {
    return left == right;
}

}

int main() {
    using aura::audition::AuditionPluginChain;
    using aura::audition::PluginStepKind;

    const std::vector<float> input = {0.25f, -0.5f, 0.75f};

    {
        AuditionPluginChain chain;
        const auto result = chain.Run(input);
        Check(result.ok, "empty chain succeeds");
        Check(SameSamples(result.output, input), "empty chain returns input samples unchanged");
        Check(result.appliedSteps.empty(), "empty chain applies no steps");
    }

    {
        AuditionPluginChain chain;
        chain.Append(PluginStepKind::Reverse);
        chain.Append(PluginStepKind::Normalize);
        const auto result = chain.Run(input);
        Check(result.ok, "reverse then normalize succeeds");
        Check(result.appliedSteps.size() == 2, "reverse then normalize applies two steps");
        Check(!result.appliedSteps.empty() && result.appliedSteps[0] == PluginStepKind::Reverse,
              "first applied step is reverse");
        Check(result.appliedSteps.size() == 2 && result.appliedSteps[1] == PluginStepKind::Normalize,
              "second applied step is normalize");
    }

    {
        AuditionPluginChain chain;
        chain.Append(PluginStepKind::Reverse);
        chain.Append(PluginStepKind::Normalize);
        int reverseCalls = 0;
        int normalizeCalls = 0;
        chain.SetStepHandler(PluginStepKind::Reverse, [&](std::vector<float>& samples) {
            ++reverseCalls;
            std::reverse(samples.begin(), samples.end());
            return true;
        });
        chain.SetStepHandler(PluginStepKind::Normalize, [&](std::vector<float>&) {
            ++normalizeCalls;
            return false;
        });
        const auto result = chain.Run(input);
        Check(!result.ok, "second-step failure fails the whole chain");
        Check(SameSamples(result.output, input), "second-step failure rolls output back to input");
        Check(result.appliedSteps.empty(), "second-step failure leaves no applied steps");
        Check(reverseCalls == 1, "second-step failure still attempted the first step");
        Check(normalizeCalls == 1, "second-step failure attempted the failing step");
    }

    {
        AuditionPluginChain chain;
        chain.Append(PluginStepKind::Reverse);
        chain.Append(PluginStepKind::Unknown);
        chain.Append(PluginStepKind::Normalize);
        int reverseCalls = 0;
        int normalizeCalls = 0;
        chain.SetStepHandler(PluginStepKind::Reverse, [&](std::vector<float>& samples) {
            ++reverseCalls;
            std::reverse(samples.begin(), samples.end());
            return true;
        });
        chain.SetStepHandler(PluginStepKind::Normalize, [&](std::vector<float>&) {
            ++normalizeCalls;
            return true;
        });
        const auto result = chain.Run(input);
        Check(!result.ok, "unknown step type is rejected");
        Check(SameSamples(result.output, input), "unknown step type leaves output equal to input");
        Check(result.appliedSteps.empty(), "unknown step type applies no steps");
        Check(reverseCalls == 0, "unknown step type does not execute earlier steps");
        Check(normalizeCalls == 0, "unknown step type does not execute later steps");
    }

    std::printf("\n=== Audition Plugin Chain: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
