#pragma once

#include "CueviFeedbackClient.h"

#include <string>

namespace aura::feedback {

// @stage7-contract
// API contract: Private application workflow state projection. References remain owned by
// the caller for the duration of one synchronous Apply/Poll call. Poll never
// blocks; it only consumes an already completed AsyncSubmission result. The
// injected resolver is called only for failed results and is retained nowhere.
struct CueviFeedbackCompletionState {
    bool& submitting;
    bool& submitOk;
    std::string& submitError;
    std::string& workItemId;
};

using CueviFeedbackErrorTextResolver =
    std::string (*)(const CueviFeedback::Result& result);

void ApplyCueviFeedbackCompletion(
    CueviFeedbackCompletionState state,
    const CueviFeedback::Result& result,
    CueviFeedbackErrorTextResolver resolveErrorText);

bool PollCueviFeedbackCompletion(
    CueviFeedbackCompletionState state,
    CueviFeedback::AsyncSubmission& task,
    CueviFeedbackErrorTextResolver resolveErrorText);

}  // namespace aura::feedback
