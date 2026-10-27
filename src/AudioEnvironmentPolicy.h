#pragma once

#include "platform/system.h"

namespace HarkenSearch::AudioEnvironment {

// @stage7-contract
// API contract: IsEnabled is a pure noexcept predicate over a caller-owned
// environment lookup result. It performs no environment access itself and
// preserves the existing success/non-empty/non-zero interpretation.

inline bool IsEnabled(const Platform::EnvironmentStringResult& result) noexcept {
    return result.status == Platform::EnvironmentStringStatus::Success &&
           !result.valueUtf8.empty() && result.valueUtf8.front() != '0';
}

} // namespace HarkenSearch::AudioEnvironment
