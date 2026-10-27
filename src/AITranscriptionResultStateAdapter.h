#pragma once

struct AppState;
class AIEngine;

namespace aura::ai {

class AITranscriptionResultStateAdapter final {
public:
    AITranscriptionResultStateAdapter(AppState& state, AIEngine& engine);

    void pump();

private:
    AppState& state_;
    AIEngine& engine_;
};

}  // namespace aura::ai
