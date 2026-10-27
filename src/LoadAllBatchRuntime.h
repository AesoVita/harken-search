#pragma once

#include <cstdint>
#include <functional>

namespace aura::load_all {

struct LoadAllBatchBeginInput {
    int viewId = 0;
    std::int64_t playlistId = -1;
    int limit = 0;
};

struct LoadAllBatchRequest {
    int viewId = 0;
    std::int64_t playlistId = -1;
    int offset = 0;
    int limit = 0;
    bool resetResults = false;
};

enum class LoadAllBatchTerminal {
    Finished,
    ViewChanged,
};

class LoadAllBatchRuntime {
public:
    using SubmitPort = std::function<void(const LoadAllBatchRequest&)>;
    using TerminalPort = std::function<void(LoadAllBatchTerminal)>;

    void begin(LoadAllBatchBeginInput input,
               SubmitPort submit,
               TerminalPort terminal);
    void onResult(int currentViewId,
                  int loadedCount,
                  bool hasMore,
                  bool pageEmpty);
    void onFrame(int currentViewId);
    void reset();

    bool active() const { return active_; }
    bool acceptsResult(int currentViewId) const {
        return active_ && currentViewId == viewId_;
    }

private:
    LoadAllBatchRequest request_{};
    int viewId_ = 0;
    bool active_ = false;
    SubmitPort submit_;
    TerminalPort terminal_;

    void finish(LoadAllBatchTerminal terminal);
};

}  // namespace aura::load_all
