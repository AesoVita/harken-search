#pragma once

#include "AsyncSearchRefreshDiagnosticsWorkflow.h"

struct AppState;

namespace aura::search {

class AsyncSearchRefreshDiagnosticsStateAdapter final {
public:
    explicit AsyncSearchRefreshDiagnosticsStateAdapter(AppState& state);

    void apply();

private:
    AppState& state_;
};

}  // namespace aura::search
