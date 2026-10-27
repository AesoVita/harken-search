#include "AsyncSearchAppendResultWorkflow.h"

namespace aura::search {

void HandleAsyncSearchAppendResult(
    const AsyncSearchAppendInput& input,
    AsyncSearchAppendEffects& effects) {
    effects.appendRows();
    effects.updateCounts(input.totalCount, input.hasMore);
    if (input.emptyPage) effects.stopAfterEmptyPage();
    effects.applyCoverKeyFilter();
    if (input.kind == AsyncSearchAppendKind::LoadAll) {
        effects.loadAllResult(input.loadedCount, input.hasMore, input.emptyPage);
    } else {
        effects.finishLoadingMore();
    }
}

}  // namespace aura::search
