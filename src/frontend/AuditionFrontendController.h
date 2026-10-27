#pragma once

#include "frontend/AuditionFrontendContract.h"

#include <thread>

namespace aris::frontend {

class AuditionFrontendController final {
public:
    explicit AuditionFrontendController(AuditionApplicationPort& port) noexcept;

    AuditionDispatchResult captureSnapshot(AuditionStateSnapshot& output) const noexcept;
    AuditionDispatchResult dispatch(const AuditionAction& action) noexcept;

private:
    AuditionApplicationPort& port_;
};

class AuditionUiDispatcher final {
public:
    explicit AuditionUiDispatcher(AuditionFrontendController& controller) noexcept;

    AuditionDispatchResult captureSnapshot(AuditionStateSnapshot& output) const noexcept;
    AuditionDispatchResult dispatch(const AuditionAction& action) noexcept;
    bool isOwnerThread() const noexcept;

private:
    AuditionFrontendController& controller_;
    std::thread::id ownerThread_;
};

}  // namespace aris::frontend
