#include "AsyncSearchResultDispatchWorkflow.h"

#include <cassert>

using aura::search::AsyncSearchResultDispatch;

int main() {
    using aura::search::ClassifyAsyncSearchResultDispatch;

    assert(ClassifyAsyncSearchResultDispatch({true, false, false, false}) ==
           AsyncSearchResultDispatch::FacetsOnly);
    assert(ClassifyAsyncSearchResultDispatch({false, true, false, false}) ==
           AsyncSearchResultDispatch::CountAndFacetsOnly);
    assert(ClassifyAsyncSearchResultDispatch({false, false, true, false}) ==
           AsyncSearchResultDispatch::Refresh);
    assert(ClassifyAsyncSearchResultDispatch({false, false, false, true}) ==
           AsyncSearchResultDispatch::LoadAll);
    assert(ClassifyAsyncSearchResultDispatch({false, false, false, false}) ==
           AsyncSearchResultDispatch::LoadMore);
    return 0;
}
