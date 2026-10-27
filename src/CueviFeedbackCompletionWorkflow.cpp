#include "CueviFeedbackCompletionWorkflow.h"

namespace aura::feedback {

void ApplyCueviFeedbackCompletion(
    CueviFeedbackCompletionState state,
    const CueviFeedback::Result& result,
    CueviFeedbackErrorTextResolver resolveErrorText) {
    state.submitting = false;
    state.submitOk = result.ok;
    if (result.ok) {
        state.submitError.clear();
        state.workItemId = result.workItemId;
        return;
    }
    state.submitError = resolveErrorText(result);
}

bool PollCueviFeedbackCompletion(
    CueviFeedbackCompletionState state,
    CueviFeedback::AsyncSubmission& task,
    CueviFeedbackErrorTextResolver resolveErrorText) {
    if (!state.submitting || !task.valid()) return false;
    const auto completed = task.Poll();
    if (!completed) return false;
    ApplyCueviFeedbackCompletion(state, *completed, resolveErrorText);
    return true;
}

}  // namespace aura::feedback
