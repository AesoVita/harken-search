#pragma once

#include "AsyncSearchEmptyFallbackWorkflow.h"

struct AppState;

namespace aura::search {

class AsyncSearchEmptyFallbackStateAdapter final {
public:
    explicit AsyncSearchEmptyFallbackStateAdapter(AppState& state);

    void apply();

private:
    AppState& state_;
};

}  // namespace aura::search
