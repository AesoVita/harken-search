#pragma once

#include <cstdint>

namespace aura::search {

struct AsyncSearchEmptyFallbackInput {
    bool databaseView = false;
    bool resultsEmpty = false;
    std::int64_t totalCount = 0;
};

class AsyncSearchEmptyFallbackEffects {
public:
    virtual ~AsyncSearchEmptyFallbackEffects() = default;
    virtual void clearFallback() = 0;
};

void HandleAsyncSearchEmptyFallback(
    const AsyncSearchEmptyFallbackInput& input,
    AsyncSearchEmptyFallbackEffects& effects);

}  // namespace aura::search
