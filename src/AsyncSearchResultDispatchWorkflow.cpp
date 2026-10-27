#include "AsyncSearchResultDispatchWorkflow.h"

namespace aura::search {

AsyncSearchResultDispatch ClassifyAsyncSearchResultDispatch(
    const AsyncSearchResultDispatchInput& input) {
    if (input.facetsOnly) return AsyncSearchResultDispatch::FacetsOnly;
    if (input.countAndFacetsOnly) {
        return AsyncSearchResultDispatch::CountAndFacetsOnly;
    }
    if (input.refresh) return AsyncSearchResultDispatch::Refresh;
    if (input.loadAll) return AsyncSearchResultDispatch::LoadAll;
    return AsyncSearchResultDispatch::LoadMore;
}

}  // namespace aura::search
