#pragma once

namespace aura::search {

enum class AsyncSearchResultDispatch {
    FacetsOnly,
    CountAndFacetsOnly,
    Refresh,
    LoadMore,
    LoadAll,
};

struct AsyncSearchResultDispatchInput {
    bool facetsOnly = false;
    bool countAndFacetsOnly = false;
    bool refresh = false;
    bool loadAll = false;
};

AsyncSearchResultDispatch ClassifyAsyncSearchResultDispatch(
    const AsyncSearchResultDispatchInput& input);

}  // namespace aura::search
