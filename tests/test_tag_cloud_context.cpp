#include "TagCloudContext.h"

#include <cassert>

int main() {
    using aura::tag_cloud::IsCurrent;
    using aura::tag_cloud::TagCloudContextIdentity;
    using aura::tag_cloud::TagCloudResult;

    TagCloudContextIdentity current;
    current.databaseIdentity = "db-instance-7";
    current.databasePath = "library/main.db";
    current.tabId = "tab-2";
    current.searchResultsGeneration = 41;
    current.filterQueryGeneration = 9;

    TagCloudResult result;
    result.identity = current;
    result.tags.push_back({"impact", 7});
    assert(IsCurrent(result, current));
    assert(result.tags.front().first == "impact");

    auto changed = current;
    changed.databaseIdentity = "db-instance-8";
    assert(!IsCurrent(result, changed));
    changed = current;
    changed.databasePath = "library/other.db";
    assert(!IsCurrent(result, changed));
    changed = current;
    changed.tabId = "tab-3";
    assert(!IsCurrent(result, changed));
    changed = current;
    changed.searchResultsGeneration++;
    assert(!IsCurrent(result, changed));
    changed = current;
    changed.filterQueryGeneration++;
    assert(!IsCurrent(result, changed));

    return 0;
}
